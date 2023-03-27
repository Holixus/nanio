#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <string.h>

#include <sys/types.h>
#include <arpa/inet.h>

#include "nano/io.h"
#include "nano/io_ipv4.h"

#include "io_internals.h"

/* ------------------------------------------------------------------------ */
static char *bput(char *p, unsigned int byte)
{
	byte &= 255;
	if (byte >= 100) {
		*p++ = '0' + byte / 100;
		byte %= 100;
		goto _2;
	}
	if (byte >= 10) {
_2:
		*p++ = '0' + byte / 10;
		byte %= 10;
	}
	*p++ = '0' + byte;
	return p;
}

/* ------------------------------------------------------------------------ */
int ipv4_ntostr(char *p, unsigned char const *ip)
{
	char *begin = p;
	p = bput(p, ip[0]);
	*p++ = '.';
	p = bput(p, ip[1]);
	*p++ = '.';
	p = bput(p, ip[2]);
	*p++ = '.';
	p = bput(p, ip[3]);
	*p = 0;
	return p - begin;
}

/* ------------------------------------------------------------------------ */
int ipv4_itostr(char *p, unsigned int ip)
{
	char *begin = p;
	p = bput(p, ip >> 24);
	*p++ = '.';
	p = bput(p, ip >> 16);
	*p++ = '.';
	p = bput(p, ip >> 8);
	*p++ = '.';
	p = bput(p, ip);
	*p = 0;
	return p - begin;
}

/* ------------------------------------------------------------------------ */
char const *ipv4_ntoa(unsigned char const *ip)
{
	char *buf = _getTmpStr();
	ipv4_ntostr(buf, ip);
	return buf;
}

/* ------------------------------------------------------------------------ */
char const *ipv4_itoa(unsigned int num)
{
	char *buf = _getTmpStr();
	ipv4_itostr(buf, num);
	return buf;
}


/* ------------------------------------------------------------------------ */
char const *ipv4_stoa(unsigned int num, int port)
{
	char *buf = _getTmpStr();
	sprintf(buf + ipv4_itostr(buf, num), ":%d", port);
	return buf;
}

/* ------------------------------------------------------------------------ */
unsigned int ipv4_strtoi(char const *ip, char const **after)
{
	if (!ip)
		return 0;

	unsigned int num = 0, sub = *ip == '.', c = sub ? 3 : 4;
	if (!sub)
		goto _start;
	do {
		if (*ip != '.') {
			if (sub)
				break;
			return 0;
		}
		++ip;
_start:;
		char const *next;
		unsigned long n = strtoul(ip, (char **)&next, 10);
		if (next == ip || n >= 256)
			return 0;
		ip = next;
		num = num << 8 | n;
	} while (--c);

	if (after)
		*after = ip;
	return num;
}


/* ------------------------------------------------------------------------ */
unsigned int ipv4_atoi(char const *ip)
{
	return ipv4_strtoi(ip, NULL);
}


/* ------------------------------------------------------------------------ */
// list of comma separated IP addresses to int array
int ipv4_ltoi(uint32_t *ips, size_t limit, char const *list)
{
	if (!list)
		return 0; // no list means empty result

	uint32_t *to = ips;
	char const *p = list;
	while (limit-- && *p) {
		*to++ = ipv4_strtoi(p, &p);
		while (*p == ' ')
			++p;
		if (*p != ',')
			break;
		while (*p == ' ')
			++p;
		++p;
	}
	return to - ips;
}


/* ------------------------------------------------------------------------ */
int ipv4_isip(char const *ip)
{
	unsigned int c = 4;
	goto _start;
	do {
		if (*ip != '.')
			return 0;
		++ip;
_start:;
		char const *next;
		unsigned long n = strtoul(ip, (char **)&next, 10);
		if (next == ip || n >= 256)
			return 0;
		ip = next;
	} while (--c);

	return 1;
}

/* ------------------------------------------------------------------------ */
int ipv4_ismask(char const *ip)
{
	if (!ipv4_isip(ip))
		return 0;
	uint32_t m = ~ipv4_atoi(ip);
	return !(m & (m+1));
//	return !m || m == ipv4_wtoi(ipv4_itow(m));
}

/* ------------------------------------------------------------------------ */
int ipv4_isimask(uint32_t ip)
{
	return !(ip & (ip+1));
}

/* ------------------------------------------------------------------------ */
unsigned int ipv4_ntoi(unsigned char const *ip)
{
	return ip[0] << 24 | ip[1] << 16 | ip[2] << 8 | ip[3];
}

/* ------------------------------------------------------------------------ */
unsigned int ipv4_iton(unsigned char *n, unsigned int i)
{ // host to net
	n[0] = 255 & (i >> 24);
	n[1] = 255 & (i >> 16);
	n[2] = 255 & (i >> 8);
	n[3] = 255 & (i);
	return i;
}

/* ------------------------------------------------------------------------ */
unsigned int ipv4_itow(unsigned int mask)
{
	uint32_t n = mask;
	n = (n >> 1) & 0x77777777; mask -= n;
	n = (n >> 1) & 0x77777777; mask -= n;
	n = (n >> 1) & 0x77777777; mask -= n;
	mask = (mask + (mask >>4)) & 0x0F0F0F0F;
	return mask*0x1010101 >> 24;
}


/* ------------------------------------------------------------------------ */
unsigned int ipv4_wtoi(int width)
{
	return width ? -1 << (32 - width) : 0;
}


/* ------------------------------------------------------------------------ */
unsigned int ipv4_netbits(uint32_t ip)
{
	unsigned bits = ip >> 29 < 4 ? 8 : (ip >> 29 < 5 ? 16 : 24);

	if (bits == 8) // 100.64.0.0/10
		return (ip & 0xFFC00000) == 0x64400000 ? 10 : bits;

	switch (ip >> 24) {
	case 169: // 169.254.0.0/16
		return (ip & 0xFF0000) == 0xFE0000 ? 16 : bits;
	case 172: // 172.16.0.0/12
		return (ip & 0xF00000) == 0x100000 ? 12 : bits;
	case 192: // 192.168.0.0/16
		return (ip & 0xFFFF00) == 0xA80000 ? 16 : bits;
	case 198: // 198.18.0.0/15
		return (ip & 0xFFFF00) == 0x120000 ? 15 : bits;
	case 224:
	case 240:
		return 4; // multicast
	case 255: // broadcast
		return (ip & 0xFFFFFF) == 0xFFFFFF ? 1 : bits;
	}
	return bits;
}
