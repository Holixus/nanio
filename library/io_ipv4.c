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
unsigned int ipv4_atoi(char const *ip)
{
	unsigned int num = 0, c = 4;
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
		num = num << 8 | n;
	} while (--c);

	return num;
}


/* ------------------------------------------------------------------------ */
char const *ipv4_aton(unsigned char *n, char const *a)
{
	unsigned int c = 4;
	goto _start;
	do {
		if (*a != '.')
			return NULL;
		++a;
_start:;
		char const *next;
		unsigned long v = strtoul(a, (char **)&next, 10);
		if (next == a || v >= 256)
			return NULL;
		a = next;
		*n++ = (unsigned char)v;
	} while (--c);

	return a;
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

	return *ip ? 0 : 1;
}

/* ------------------------------------------------------------------------ */
int ipv4_ismask(char const *ip)
{
	if (!ipv4_isip(ip))
		return 0;
	uint32_t m = ipv4_atoi(ip);
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
