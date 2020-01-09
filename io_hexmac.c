#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <string.h>

#include <sys/types.h>
#include <arpa/inet.h>

#include <ctype.h>

#include "nano/io.h"
#include "nano/io_hexmac.h"

#include "io_internals.h"


/* ------------------------------------------------------------------------ */
static char nibbletohex(unsigned int nibble) __attribute__ ((pure));

char nibbletohex(unsigned int nibble)
{
	static const char hexdigits[] = "0123456789ABCDEF";
	return hexdigits[nibble & 0xF];
}

/* ------------------------------------------------------------------------ */
int hex_ntostr(char *str, size_t ssize, unsigned char const *bin, size_t bsize)
{
	char *begin = str, *end = str + ssize - 2;
	unsigned char const *bend = bin + bsize;
	for (; bin < bend && str < end; str += 2, ++bin) {
		str[0] = nibbletohex(*bin >> 4);
		str[1] = nibbletohex(*bin);
	}

	*str = 0;
	return str - begin;
}

/* ------------------------------------------------------------------------ */
char const *hex_ntoa(unsigned char const *bin, size_t size)
{
	char *buf = _getTmpStr();

	if (size * 2 - 2 > TMP_STR_SIZE) {
		char *to = buf + hex_ntostr(buf, TMP_STR_SIZE, bin, TMP_STR_SIZE / 2 - 2);
		to[0] = to[1] = to[2] = '.';
		to[3] = 0;
	} else
		hex_ntostr(buf, TMP_STR_SIZE, bin, size);

	return buf;
}

/* ------------------------------------------------------------------------ */
int mac_ntostr(char *str, unsigned char const *mac)
{
	char *begin = str;
	int size = 6;
	do {
		str[0] = nibbletohex(*mac >> 4);
		str[1] = nibbletohex(*mac++);
		str[2] = ':';
		str += 3;
	} while(--size);

	*--str = 0;
	return str - begin;
}

/* ------------------------------------------------------------------------ */
char const *mac_ntoa(unsigned char const *mac)
{
	char *buf = _getTmpStr();
	mac_ntostr(buf, mac);
	return buf;
}

/* ------------------------------------------------------------------------ */
int mac_copy(unsigned char *dst, unsigned char *src)
{
	dst[0] =src[0];
	dst[1] =src[1];
	dst[2] =src[2];
	dst[3] =src[3];
	dst[4] =src[4];
	dst[5] =src[5];
	return 0;
}

/* ------------------------------------------------------------------------ */
int mac_is_equal(const unsigned char *dst, const unsigned char *src)
{
	return (dst[0] == src[0])
			&&(dst[1] == src[1])
			&&(dst[2] == src[2])
			&&(dst[3] == src[3])
			&&(dst[4] == src[4])
			&&(dst[5] == src[5]);
}

/* ------------------------------------------------------------------------ */
int mac_is_ipv4_multicast(const unsigned char *mac)
{
	static const unsigned char  prefix[] = {0x01, 0x00, 0x5E};
	return mac[0] == prefix[0] && mac[1] == prefix[1]  && mac[2] == prefix[2]; 
}

/* ------------------------------------------------------------------------ */
int mac_is_ipv6_multicast(const unsigned char *mac)
{
	static const unsigned char prefix[] = {0x33, 0x33};
	return mac[0] == prefix[0] && mac[1] == prefix[1];
}

/* ------------------------------------------------------------------------ */
int mac_is_multicast(const unsigned char *mac)
{
	return mac[0] & 0x01;
}

/* ------------------------------------------------------------------------ */
int mac_is_dns_multicast(const unsigned char *mac)
{
	return (mac_is_ipv4_multicast(mac) || mac_is_ipv4_multicast(mac)) && (mac[5] == 0xfb);
}


/* ------------------------------------------------------------------------ */
static int hextonibble(char digit)
{
	switch (digit >> 6) {
	case 1:;
		int n = (digit & ~32) - 'A' + 10;
		return 9 < n && n < 16 ? n : 256;

	case 0:
		digit -= '0';
		if (0 <= digit && digit < 10)
			return digit;
	}

	return 256;
}

/* ------------------------------------------------------------------------ */
static unsigned int hextobyte(char const *text)
{
	return hextonibble(text[0]) * 16 + hextonibble(text[1]);
}

/* ------------------------------------------------------------------------ */
int mac_aton(unsigned char *mac, char const *str)
{
	char const *buf = str;
	unsigned int abyte, size = 6;

	goto _start;
	do {
		str += (*str == ':' || *str == '-') ? 1 : 0;

	_start:
		if ((abyte = hextobyte(str)) >= 256)
			return 0;

		str += 2;

		*mac++ = (unsigned char)abyte;
	} while (--size);

	return str - buf;
}

/* ------------------------------------------------------------------------ */
int mac_isnull(unsigned char *mac)
{
	return !mac || !(mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]);
}


/* ------------------------------------------------------------------------ */
int hex_aton(unsigned char *bin, size_t size, char const *hex)
{
	char const *buf = hex;
	unsigned int abyte;

	for (; size; --size) {
		if ((abyte = hextobyte(hex)) >= 256)
			return *hex ? 0 : hex - buf;

		hex += 2;

		*bin++ = (unsigned char) abyte;
	}

	return hex - buf;
}

/* ------------------------------------------------------------------------ */
int hex_check(size_t size, char const *hex)
{
	unsigned int abyte;
	if (!size) {
		while (*hex) {
			if ((abyte = hextobyte(hex)) >= 256)
				return 0;
			hex += 2;
		}
	} else {
		for (; size; --size) {
			if ((abyte = hextobyte(hex)) >= 256)
				return 0;
			hex += 2;
		}
	}

	return 1;
}

