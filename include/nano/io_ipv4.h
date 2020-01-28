#ifndef NANO_IO_IPV4_H
#define NANO_IO_IPV4_H

int ipv4_ntostr(char *str, unsigned char const *ip);
int ipv4_itostr(char *str, unsigned int ip);

char const *ipv4_ntoa(unsigned char const *ip);
char const *ipv4_itoa(unsigned int num);
int ipv4_isip(char const *str);
int ipv4_ismask(char const *str);
int ipv4_isimask(unsigned int mask);

unsigned int ipv4_atoi(char const *a);
char const  *ipv4_aton(unsigned char *n, char const *a);
unsigned int ipv4_ntoi(unsigned char const *n) __attribute__ ((pure));
unsigned int ipv4_itow(unsigned int mask) __attribute__ ((pure));       // mask to width
unsigned int ipv4_wtoi(int width) __attribute__ ((pure));               // width to mask
unsigned int ipv4_iton(unsigned char *n, unsigned int i);               // host to net

char const *ipv4_stoa(unsigned int num, int port); // ip:port(socket) to ascii

#endif


