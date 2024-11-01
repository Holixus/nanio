#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <strings.h>

#include "io_http_auth.h"
#include "ut_base64.h"
#include "ut_md5.h"


/* ------------------------------------------------------------------------ */
#ifndef countof
# define countof(a) (sizeof(a)/sizeof(a[0]))
#endif

/* ------------------------------------------------------------------------ */
static char *stract(char *out, size_t size, char const *start, ssize_t len)
{
	if (len < 0)
		len = (ssize_t) strlen(start);
	if (len >= size)
		len = (ssize_t) (size - 1);
	memcpy(out, start, (size_t) len);
	out[len] = 0;
	return out;
}

/* ------------------------------------------------------------------------ */
void http_auth_empty(http_auth_t *d)
{
	if (!d)
		return;

	d->type = HTTP_AUTH_NONE;
	d->qop = AUTH_QOP_NONE;
	d->algorithm = ALG_MD5;
	d->stale = STALE_FALSE;
	d->realm[0] = d->domain[0] = d->nonce[0] = d->nextnonce[0] = d->sess_nonce[0] = d->opaque[0] =
	d->username[0] = d->password[0] = d->uri[0] = d->cnonce[0] = d->nc[0] =
	d->response[0] = 0;
	d->entity = NULL;
}

/* ------------------------------------------------------------------------ */
int http_auth_parse(http_auth_t *d, char const *auth)
{
	http_auth_empty(d);

	while (*auth == ' ') ++auth;
	if (!strncasecmp(auth, "digest ", 7)) {
		d->type = HTTP_AUTH_DIGEST;
		auth += 7;
	} else if (!strncasecmp(auth, "basic ", 6)) {
		d->type = HTTP_AUTH_BASIC;
		auth += 6;
	} else
		return -1;


#define OPTION(name) { #name, (unsigned long )&((http_auth_t *)0)->name, (unsigned long )sizeof(((http_auth_t *)0)->name) }
	static const struct ids { char const *id; unsigned short offset; unsigned short size; } ids[] = {
		OPTION(realm), OPTION(domain), OPTION(nonce), OPTION(nextnonce), OPTION(opaque),
		OPTION(username), OPTION(uri), OPTION(cnonce), OPTION(nc), OPTION(response)
	};
	char id[16];
	while (auth && *auth) {
		while (*auth == ' ')
			++auth;
		char *eq = strchr(auth, '=');
		if (eq) {
			char qop[32];
			char *value = qop;
			unsigned int valsize = sizeof qop;
			const struct ids *opt = ids, *end = ids + countof(ids);
			stract(id, sizeof id, auth, eq - auth);
			for (; opt < end; ++opt)
				if (*id == opt->id[0] && !strcmp(id, opt->id)) {
					value = (char *)d + opt->offset;
					valsize = opt->size;
					break;
				}

			++eq;
			if (*eq == '"') {
				char *v = eq + 1;
				if ((eq = strchr(v, '"'))) {
					if (value)
						stract(value, valsize, v, eq - v);
					++eq;
				}
			} else {
				char *v = eq;
				eq = strpbrk(v, ", ");
				if (value)
					stract(value, valsize, v, eq ? eq - v : -1);
			}
			if (value == qop) {
				switch (id[0]) {
				case 'q':
					if (id[1] != 'o' || id[2] != 'p')
						break;
					char *c = value;
					d->qop = AUTH_QOP_NONE;
					do {
						if (c[0] == 'a' && c[1] == 'u' && c[2] == 't' && c[3] == 'h') {
							if (c[4] == '-' && c[5] == 'i' && c[6] == 'n' && c[7] == 't' && (!c[8] || c[8] == ',')) {
								c += 8;
								d->qop |= AUTH_QOP_AUTH_INT;
								continue;
							} else if (!c[4] || c[4] == ',') {
								c += 4;
								d->qop |= AUTH_QOP_AUTH;
								continue;
							}
						}
						d->qop |= AUTH_QOP_ELSE;
						while (*c && *c != ',') ++c;
					} while (*c++ == ',');
					break;
				case 'a':
					if (strcmp(id+1, "lgorithm"))
						break;
					if (!strcmp(value, "MD5"))
						d->algorithm = ALG_MD5;
					else if (!strcmp(value, "MD5-sess"))
						d->algorithm = ALG_MD5_SESS;
					else
						d->algorithm = ALG_MD5_ELSE;
					break;
				case 's':
					if (strcmp(id+1, "tale"))
						break;
					if (!strcasecmp(value, "false"))
						d->stale = STALE_FALSE;
					else if (!strcmp(value, "true"))
						d->stale = STALE_TRUE;
					break;
				}
			}
			if (eq)
				while (*eq == ',' || *eq == ' ') ++eq;
			auth = eq;
		} else {
			if (d->type == HTTP_AUTH_BASIC) {
				char userpass[128];
				userpass[0] = 0;
				b64_decode(userpass, sizeof userpass, auth);
				char *pass = strchr(userpass, ':');
				if (!pass)
					return -1;
				stract(d->username, sizeof d->username, userpass, pass - userpass);
				stract(d->password, sizeof d->password, pass + 1, -1);
				return 0;
			}
		}
	}

	if (!d->nc[0] || strcmp(d->nonce, d->sess_nonce)) {
		strcpy(d->nc, "00000001");
		strcpy(d->sess_nonce, d->nonce);
	} else {
		unsigned int nc;
		sscanf(d->nc, "%x", &nc);
		snprintf(d->nc, sizeof d->nc, "%08X", ++nc);
	}

	if (!d->cnonce[0])
		snprintf(d->cnonce, sizeof d->cnonce, "%04X%04x%04x", rand(), rand(), rand());

	return 0;
}

/* ------------------------------------------------------------------------ */
/* 401 WWW-Authenticate: */
int http_auth_response(http_auth_t *d, char *out, size_t size)
{
	switch (d->type) {
	case HTTP_AUTH_BASIC:;
		char *rlm = out + snprintf(out, size, "Basic");
		if (d->realm[0])
			snprintf(rlm, size - (size_t) (rlm-out), " realm=\"%s\"", d->realm);
		break;

	case HTTP_AUTH_DIGEST:;
		char *e = out + size;
		char *n = out + snprintf(out, size, "Digest realm=\"%s\"", d->realm);
		if (d->domain[0])
			n += snprintf(n, (size_t) (e - n), ", domain=\"%s\"", d->domain);
		n += snprintf(n, (size_t) (e - n), ", nonce=%s", d->nonce);
		if (d->opaque[0])
			n += snprintf(n, (size_t) (e - n), ", opaque=\"%s\"", d->opaque);
		n += snprintf(n, (size_t) (e - n), ", stale=%s", d->stale ? "true" : "false");
		if (d->algorithm == ALG_MD5_SESS)
			n += snprintf(n, (size_t) (e - n), ", algorithm=MD5-sess");
		if (d->qop != AUTH_QOP_NONE)
			n += snprintf(n, (size_t) (e - n), ", qop=\"%s%s%s\"",
					d->qop & AUTH_QOP_AUTH ? "auth" : "",
					d->qop == (AUTH_QOP_AUTH|AUTH_QOP_AUTH_INT) ? "|" : "",
					d->qop & AUTH_QOP_AUTH_INT ? "auth-int" : "");
		break;

	case HTTP_AUTH_NONE:
		break;
	}
	return 0;
}

/* ------------------------------------------------------------------------ */
/* Authorization: */
int http_auth_request(http_auth_t *d, char *out, size_t size)
{
	switch (d->type) {
	case HTTP_AUTH_BASIC:;
		char *rlm = out + snprintf(out, size, "Basic");
		if (!d->username[0] || !d->password[0])
			return -1;
		char userpass[128];
		int len = snprintf(userpass, sizeof userpass, "%s:%s", d->username, d->password);
		if (size + 2 - (size_t) (rlm-out) < b64_encoded_len((size_t) len))
			return -1; /* too small output buffer */
		rlm[0] = ' ';
		b64_encode(rlm + 1, userpass, (size_t) len);
		break;

	case HTTP_AUTH_DIGEST:;
		if (d->nextnonce[0]) {
			strcpy(d->nonce, d->nextnonce);
			d->nextnonce[0] = 0;
		}
		char *e = out + size;
		char *n = out + snprintf(out, size, "Digest username=\"%s\", realm=\"%s\", nonce=%s, uri=%s, response=%s",
				d->username, d->realm, d->nonce, d->uri, d->response);
		if (d->algorithm == ALG_MD5_SESS)
			n += snprintf(n, (size_t) (e - n), ", algorithm=MD5-sess");
		if (d->cnonce[0])
			n += snprintf(n, (size_t) (e - n), ", cnonce=%s", d->cnonce);
		if (d->opaque[0])
			n += snprintf(n, (size_t) (e - n), ", opaque=\"%s\"", d->opaque);
		if (d->qop != AUTH_QOP_NONE)
			n += snprintf(n, (size_t) (e - n), ", qop=\"%s%s%s\"",
					d->qop & AUTH_QOP_AUTH ? "auth" : "",
					d->qop == (AUTH_QOP_AUTH|AUTH_QOP_AUTH_INT) ? "|" : "",
					d->qop & AUTH_QOP_AUTH_INT ? "auth-int" : "");
		if (d->nc[0])
			n += snprintf(n, (size_t) (e - n), ", nc=%s", d->nc);
		break;

	case HTTP_AUTH_NONE:
		break;
	}
	return 0;
}

/* ------------------------------------------------------------------------ */
static void md5_to_hex(char *hex, unsigned char const md5[16])
{
	static const char dig[] = "0123456789abcdef";
	unsigned char const *end = md5 + 16;
	for (; md5 < end; ++md5) {
		*hex++ = dig[*md5 >> 4];
		*hex++ = dig[*md5 & 15];
	}
	*hex = 0;
}

static inline void md5_hex_finish(md5_state_t *pms, char *hexhash /* 33 bytes */)
{
	unsigned char h[16];
	md5_finish(pms, h);
	md5_to_hex(hexhash, h);
}

static void md5_calc(char *hexhash, char const *str)
{
	md5_state_t h;
	md5_init(&h);
	if (str)
		md5_append(&h, (md5_byte_t *) str, strlen(str));
	md5_hex_finish(&h, hexhash);
}

static void md5_printf(char *hexhash, char const *fmt, ...)
{
	md5_state_t h;
	md5_init(&h);

	char str[768];
	va_list ap;
	va_start(ap, fmt);
	md5_append(&h, (md5_byte_t *) str, (size_t) vsnprintf(str, sizeof str, fmt, ap));
	va_end(ap);
	md5_hex_finish(&h, hexhash);
}

/* ------------------------------------------------------------------------ */
int http_auth_response_calc(http_auth_t *d, char *response)
{
	char A1[33], A2[33], entity_hash[33];

	md5_printf(A1, "%s:%s:%s", d->username, d->realm, d->password);
	if (d->algorithm == ALG_MD5_SESS)
		md5_printf(A1, "%s:%s:%s", A1, d->nonce, d->cnonce);

	if (d->qop & AUTH_QOP_AUTH_INT)
		md5_calc(entity_hash, d->entity);

	md5_printf(A2, (d->qop & AUTH_QOP_AUTH_INT) ? "%s:%s:%s" : "%s:%s", d->method, d->uri, entity_hash);

	if (d->qop & AUTH_QOP_AUTH_INT)
		d->qop = AUTH_QOP_AUTH_INT;
	else if (d->qop & AUTH_QOP_AUTH)
		d->qop = AUTH_QOP_AUTH;

	md5_printf(response, d->qop & (AUTH_QOP_AUTH|AUTH_QOP_AUTH_INT) ? "%1$s:%2$s:%4$s:%5$s:auth%6$s:%3$s" : "%s:%s:%s",
		A1, d->nonce, A2, d->nc, d->cnonce, d->qop == AUTH_QOP_AUTH_INT ? "-int":"");
	return 0;
}

