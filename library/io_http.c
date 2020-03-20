#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
//#include <strings.h>
#include <errno.h>

#include "nano/io.h"
#include "nano/io_map.h"
#include "nano/io_http.h"
#include "io_internals.h"

/* ------------------------------------------------------------------------ */
static int io_after_space(char const **p)
{
	char const *s = *p;
	while (*s == ' ')
		++s;

	return *(*p = s);
}

/* ------------------------------------------------------------------------ */
static int io_match_char(char const **p, int ch)
{
	return io_after_space(p) == ch ? (++*p, 1) : 0;
}


/* ------------------------------------------------------------------------- */
static int io_http_match_type(char const **text)
{
	char const *in = *text;
	unsigned int type_offset = 0;
	switch (*in) {
	case 'C':
		if (!memcmp(in+1, "ONNECT", 6))
			type_offset = HTTP_CONNECT + 0x70;
		break;

	case 'D':
		if (!memcmp(in+1, "ELETE", 5))
			type_offset = HTTP_DELETE + 0x60;
		break;

	case 'G':
		if (in[1] == 'E' && in[2] == 'T')
			type_offset = HTTP_GET + 0x30;
		break;

	case 'H':
		if (in[1] == 'E' && in[2] == 'A' && in[3] == 'D')
			type_offset = HTTP_HEAD + 0x40;
		break;

	case 'O':
		if (!memcmp(in+1, "PTIONS", 6))
			type_offset = HTTP_OPTIONS + 0x70;
		break;

	case 'P':
		switch (in[1]) {
		case 'A':
			if (in[2] == 'T' && in[3] == 'C' && in[4] == 'H')
				type_offset = HTTP_PATCH + 0x50;
			break;
		case 'O':
			if (in[2] == 'S' && in[3] == 'T')
				type_offset = HTTP_POST + 0x40;
			break;
		case 'U':
			if (in[2] == 'T')
				type_offset = HTTP_PUT + 0x30;
			break;
		}
		break;

	case 'T':
		if (!memcmp(in+1, "RACE", 4))
			type_offset = HTTP_TRACE + 0x50;
		break;
	}

	in += type_offset >> 4;
	if (*in == ' ') {
		*text = in + 1; /* auto space skip */
		return type_offset & 15;
	}
	return HTTP_ERROR;
}

/* ------------------------------------------------------------------------- */
static int digit2num(char digit)
{
	switch (digit >> 6) {
	case 1:
		return (digit & ~32) - 'A' + 10;

	case 0:
		if ((digit -= '0') >= 0)
			if (digit < 10)
				return digit;
	}

	return 256;
}

/* ------------------------------------------------------------------------- */
static int match_dec(char const **text)
{
	char const *in = *text;
	int num = 0, dig;
	while ((dig = digit2num(*in)) < 10)
		(num = dig + 10 * num), ++in;
	return *text = in, num;
}

/* ------------------------------------------------------------------------- */
static int io_http_match_version(char const **text)
{
	char const *in = *text;

	if (in[0] != 'H' && in[1] != 'T' && in[2] != 'T' && in[3] != 'P' && in[4] != '/')
		return -1;
	in += 5;
	int major = match_dec(&in);
	if (*in != '.')
		return -1;
	++in;
	int minor = match_dec(&in);
	return *text = in, major * 0x100 + minor;
}

/* ------------------------------------------------------------------------- */
int io_http_req_parse(io_http_req_t *req, char const **text)
{
	char const *in = *text;

	int method = io_http_match_type(&in);
	if (!method)
		return errno = EINVAL, -1;

	io_after_space(&in);

	char *out = req->path;
	char *end = out + sizeof req->path - 1;
	while (*in && *in != ' ' && out < end)
		*out++ = *in++;

	if (*in != ' ')
		return errno = EINVAL, -1;

	*out = 0;

	io_after_space(&in);
	req->method = method;
	req->version = io_http_match_version(&in);
	return req->version < 0 ? (errno = EINVAL, -1) : (*text = in, 0);
}


/* ------------------------------------------------------------------------- */
char const *io_http_code(int code)
{
typedef
struct _http_code {
	int code;
	char const *message;
} http_code_t;

	static const http_code_t codes[] = {
		{ 101, "Switching Protocols" },
		{ 200, "Data follows" },
		{ 204, "No Content" },
		{ 301, "Redirect" },
		{ 302, "Redirect" },
		{ 304, "User local copy" },
		{ 307, "Temporary Redirect" },
		{ 400, "Bad request" },
		{ 401, "Unauthorized" },
		{ 403, "Forbidden" },
		{ 404, "Page Not Found" },
		{ 405, "Access Denied" },
		{ 413, "Request Entity Too Large" },
		{ 415, "Unsupported Media Type" },
		{ 420, "Process Failed" },
		{ 426, "Upgrade Required" },
		{ 500, "Web Error" },
		{ 501, "Not Implemented" },
		{ 503, "Service Unavailable" }
	};

	const http_code_t *left = codes, *right = codes + countof(codes);

	while (right - left > 1) {
		const http_code_t *middle = left + (right-left)/2;
		if (middle->code == code)
			return middle->message;

		if (middle->code < code)
			left = middle;
		else
			right = middle;
	}

	if (left->code == code)
		return left->message;

	return "";
}

/* ------------------------------------------------------------------------- */
size_t io_url_decoded_length(char const *encoded_url)
{
	char const *p = encoded_url;
	size_t count = 0;
	while (*p)
		count += *p++ == '%' ? 1 : 0;
	return (unsigned)(p - encoded_url) - 2*count + 1;
}

/* ------------------------------------------------------------------------- */
char *io_url_decode(char *out, size_t size, char const *encoded_url)
{
	char *end = out + size - 1;
	while (*encoded_url && out < end) {
		int ch = *encoded_url++;
		switch (ch) {
		case '%':;
			int num = digit2num(encoded_url[0])*16 + digit2num(encoded_url[1]);
			if (num < 256) {
				ch = num;
				encoded_url += 2;
			}
			break;
		case '+':
			ch = ' ';
			break;
		}
		*out++ = ch;
	}
	*out = 0;
	return out;
}

/* ------------------------------------------------------------------------- */
static inline int is_url_encode_char(char ch)
{
	return !(ch & 0xE0) || strchr("'\"\%\\", ch);
}

/* ------------------------------------------------------------------------- */
size_t io_url_encoded_length(char const *url)
{
	char const *p = url;
	size_t count = 0;
	while (*p)
		count += is_url_encode_char(*p++) ? 1 : 0;
	return (unsigned)(p - url) + 2*count + 1;
}

/* ------------------------------------------------------------------------- */
char *io_url_encode(char *to, size_t size, char const *url)
{
	char const *str = url;
	char *out = to, *end = to + size - 1;
	while (out < end && *str) {
		if (is_url_encode_char(*str)) {
			*out++ = '%';
			*out++ = "0123456789ABCDEF"[15 & (*str >> 4)];
			*out++ = "0123456789ABCDEF"[15 & (*str)];
			++str;
		} else {
			*out++ = *str++;
		}
	}
	*out = 0;
	return to;
}


/* ------------------------------------------------------------------------- */
static inline int is_http_token_char(char ch)
{
	return (ch & 0xE0) && !strchr("()<>@,;:\\\"/[]?={} \t", ch);
}

/* ------------------------------------------------------------------------- */
static int match_token(char const **t)
{
	char const *c = *t;
	while (is_http_token_char(*c))
		++c;
	return *t == c ? 0 : (*t = c, 1);
}

/* ------------------------------------------------------------------------- */
int io_http_header_parse(io_hmap_t *h, char const **header)
{
	char const *p = *header;
	do {
		io_after_space(&p);
		char const *name = p;
		if (!match_token(&p))
			return -1;

		size_t name_len = (size_t)(p - name);

		if (io_after_space(&p) != ':')
			return -2;

		++p;
		io_after_space(&p);
		char const *value = p;

		char const *value_end = strstr(p, "\r\n");
		if (value_end == p)
			return -3;

		char const *next_line = value_end + 2;

		while (value_end[-1] == ' ' && value_end > value)
			--value_end;

		if (io_hmap_addl(h, name, name_len, value, (unsigned)(value_end - value)) < 0)
			return -4;
		p = next_line;
	} while (p[0] != '\r');

	return p[1] != '\n' ? -5 : (*header = p, 0);
}

