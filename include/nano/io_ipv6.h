#ifndef NANO_IO_IPV6_H
#define NANO_IO_IPV6_H

int ipv6_ntostr(char *str, uint8_t const *n);
char const *ipv6_ntoa(     uint8_t const *n);

int ipv6_nptostr(char *str, uint8_t const *n, int prefix_len);
char const *ipv6_nptoa(     uint8_t const *n, int prefix_len);

int ipv6_aton(uint8_t *n, char const *a);

int ipv6_atonp(uint8_t *n, int *prefix_len, char const *a);

uint16_t *ipv6_ntoh(uint16_t *u16, uint8_t const *u8);
uint8_t  *ipv6_hton(uint8_t *u8, uint16_t const *u16);

char const *ipv6_htoa(uint16_t const ip[8]);

int ipv6_atoh(uint16_t h[8], char const *a);
int ipv6_atohp(uint16_t h[8], int *prefix_len, char const *a);

int ipv6_isip(char const *str);


#endif


