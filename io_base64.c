#include <stddef.h>

/* -------------------------------------------------------------------------- */
static inline unsigned int _b64_demap(char i) {
	if (i < 43 || 122 < i)
		return 0;
	char c = "|\0\0\0}rstuvwxyz{\0\0\0\0\0\0\0>?@ABCDEFGHIJKLMNOPQRSTUVW\0\0\0\0\0\0XYZ[\\]^_`abcdefghijklmnopq"[i-43];
	if (c)
		return (unsigned int) (c - 62);
	return 0;
}


/* -------------------------------------------------------------------------- */
size_t b64_decode(unsigned char *out, size_t size, const char *str)
{
	unsigned char *to = out;
	unsigned char *end = out + size;

	while (*str && to < end) {
		unsigned int r =
				(_b64_demap(str[0]) << 18) |
				(_b64_demap(str[1]) << 12) |
				(_b64_demap(str[2]) << 6) |
				(_b64_demap(str[3]));
		*to++ = (char) (r >> 16);
		if (str[2] == '=' || to >= end)
			break;
		*to++ = (char) (r >> 8);
		if (str[3] == '=' || to >= end)
			break;
		*to++ = (char) r;
		str += 4;
	}
	return (size_t) (to - out);
}


/* -------------------------------------------------------------------------- */
size_t b64_decoded_len(size_t size)
{
	return size / 4U * 3U + 1U;
}

/* -------------------------------------------------------------------------- */
size_t b64_encode(char *out, size_t osize, const unsigned char *in, size_t size)
{
	static const char b64[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const unsigned char *end = (const unsigned char *)in + size;
	char *to = out;
	char *oend = out + ((osize-1) & ~3);

	for (; in < end && to < oend; in += 3, to += 4) {
		unsigned int v = (unsigned int) ((in[0] << 24) | 12);
		if (in < end)
			v |= (unsigned int) (in[1] << 16) | 0x2;
		if (in < end)
			v |= (unsigned int) (in[2] <<  8) | 0x1;

		to[0] = b64[v >> 26];
		to[1] = b64[0x3F & (v >> 20)];
		to[2] = (char) ((v & 2) ? b64[0x3F & (v >> 14)] : '=');
		to[3] = (char) ((v & 1) ? b64[0x3F & (v >>  8)] : '=');
	}
	*to = 0;
	return (size_t) (to - out);
}

/* -------------------------------------------------------------------------- */
size_t b64_encoded_len(size_t size)
{
	return ((size * 4U) / 3U + 3U) & ~3U;
}

