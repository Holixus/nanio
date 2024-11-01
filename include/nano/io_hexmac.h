#ifndef NANO_IO_HEXMAC_H
#define NANO_IO_HEXMAC_H

int mac_ntostr(char *str, unsigned char const *mac);

char const *mac_ntoa(unsigned char const *mac);
int mac_aton(unsigned char *mac, char const *str);

int mac_isnull(unsigned char *mac);

int mac_copy(unsigned char *dst, unsigned char *src);

int mac_is_equal(const unsigned char *dst, const unsigned char *src);

int mac_is_ipv4_multicast(const unsigned char *mac);
int mac_is_ipv6_multicast(const unsigned char *mac);
int mac_is_multicast(const unsigned char *mac);
int mac_is_dns_multicast(const unsigned char *mac);


int hex_ntostr(char *str, size_t ssize, unsigned char const *bin, size_t size);
char const *hex_ntoa(unsigned char const *bin, size_t size);
int hex_aton(unsigned char *bin, size_t size, char const *hex);
int hex_check(size_t size, char const *hex);

unsigned int hextobyte(char const *text);

#endif
