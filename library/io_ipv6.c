#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <arpa/inet.h>

#include <ctype.h>

#include "nano/io.h"
#include "nano/io_ipv6.h"

#include "io_internals.h"


/* -------------------------------------------------------------------------- */
int ipv6_ntostr(char *s, uint8_t const *n)
{
	if (inet_ntop(AF_INET6, n, s, INET6_ADDRSTRLEN))
		return strlen(s);
	return 0;
}

/* -------------------------------------------------------------------------- */
char const *ipv6_ntoa(uint8_t const *n)
{
	char *s = _getTmpStr();
	inet_ntop(AF_INET6, n, s, INET6_ADDRSTRLEN);
	return s;
}

/* -------------------------------------------------------------------------- */
char const *ipv6_nptoa(uint8_t const *n, int prefix_len)
{
	char *s = _getTmpStr();
	inet_ntop(AF_INET6, n, s, INET6_ADDRSTRLEN);
	sprintf(s + strlen(s), "/%d", prefix_len);
	return s;
}

/* -------------------------------------------------------------------------- */
int ipv6_nptostr(char *s, uint8_t const *n, int prefix_len)
{
	if (inet_ntop(AF_INET6, n, s, INET6_ADDRSTRLEN)) {
		int len = strlen(s);
		return len + sprintf(s + len, "/%d", prefix_len);
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
int ipv6_aton(uint8_t *ip, char const *a)
{
	return inet_pton(AF_INET6, a, ip) > 0 ? strlen(a) : 0;
}

/* -------------------------------------------------------------------------- */
int ipv6_atonp(uint8_t *n, int *prefix_len, char const *a)
{
	char ip[44];
	if (sscanf(a, "%40[^/]/%d", ip, prefix_len) < 2)
		return -1;

	return inet_pton(AF_INET6, ip, n) > 0 ? 0 : -1;
}

/* -------------------------------------------------------------------------- */
uint16_t *ipv6_ntoh(uint16_t *u16, uint8_t const *u8)
{
	uint16_t *s = (uint16_t *)u8;
	u16[0] = ntohs(s[0]); u16[1] = ntohs(s[1]);
	u16[2] = ntohs(s[2]); u16[3] = ntohs(s[3]);
	u16[4] = ntohs(s[4]); u16[5] = ntohs(s[5]);
	u16[6] = ntohs(s[6]); u16[7] = ntohs(s[7]);
/*	u16[0] = u8[ 0] * 256 + u8[ 1]; u16[1] = u8[ 2] * 256 + u8[ 3];
	u16[2] = u8[ 4] * 256 + u8[ 5]; u16[3] = u8[ 6] * 256 + u8[ 7];
	u16[4] = u8[ 8] * 256 + u8[ 9]; u16[5] = u8[10] * 256 + u8[11];
	u16[6] = u8[12] * 256 + u8[13]; u16[7] = u8[14] * 256 + u8[15];*/
	return u16;
}

/* -------------------------------------------------------------------------- */
uint8_t  *ipv6_hton(uint8_t *u8, uint16_t const *u16)
{
	uint16_t *d = (uint16_t *)u8;
	d[0] = htons(u16[0]); d[1] = htons(u16[1]);
	d[2] = htons(u16[2]); d[3] = htons(u16[3]);
	d[4] = htons(u16[4]); d[5] = htons(u16[5]);
	d[6] = htons(u16[6]); d[7] = htons(u16[7]);
/*	u8[ 0] = u16[0] >> 8; u8[ 1] = u16[0]; u8[ 2] = u16[1] >> 8; u8[ 3] = u16[1];
	u8[ 4] = u16[2] >> 8; u8[ 5] = u16[2]; u8[ 6] = u16[3] >> 8; u8[ 7] = u16[3];
	u8[ 8] = u16[4] >> 8; u8[ 9] = u16[4]; u8[10] = u16[5] >> 8; u8[11] = u16[5];
	u8[12] = u16[6] >> 8; u8[13] = u16[6]; u8[14] = u16[7] >> 8; u8[15] = u16[7];*/
	return u8;
}

/* -------------------------------------------------------------------------- */
char const *ipv6_htoa(uint16_t const ip[8])
{
	uint8_t n[16];
	ipv6_hton(n, ip);
	char *s = _getTmpStr();
	inet_ntop(AF_INET6, n, s, INET6_ADDRSTRLEN);
	return s;
}


/* -------------------------------------------------------------------------- */
char const *ipv6_stoa(uint16_t const ip[8], int port)
{
	uint8_t n[16];
	ipv6_hton(n, ip);
	char *s = _getTmpStr();
	char *p = s;
	*p++ = '[';
	inet_ntop(AF_INET6, n, p, INET6_ADDRSTRLEN);
	p = strchr(p, 0);
	sprintf(p, "/%d]", port);
	return s;
}

/* -------------------------------------------------------------------------- */
int ipv6_atoh(uint16_t h[8], char const *a)
{
	uint8_t n[16];
	int r = inet_pton(AF_INET6, a, n) > 0 ? strlen(a) : 0;
	ipv6_ntoh(h, n);
	return r < 0 ? r : (r > 0 ? 0 : (errno = EINVAL, -1));
}

/* -------------------------------------------------------------------------- */
int ipv6_atohp(uint16_t h[8], int *prefix_len, char const *a)
{
	char ip[44];
	if (sscanf(a, "%40[^/]/%d", ip, prefix_len) < 2)
		return -1;

	return ipv6_atoh(h, ip);
}

/* -------------------------------------------------------------------------- */
int ipv6_isip(char const *str)
{
	uint8_t ip[16];
	return inet_pton(AF_INET6, str, ip) == 1;
}

