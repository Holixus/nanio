#ifndef NANO_SOCK_ADDR_H
#define NANO_SOCK_ADDR_H

#include <sys/un.h>
#include <netinet/ip.h>

typedef
struct sock_conf {
	uint16_t port;
	uint16_t family;
	union {
		char const *path;
		uint32_t ipv4;
		uint16_t ipv6[8];
	} addr;
} io_sock_addr_t;

/* -------------------------------------------------------------------------- */
/* socket address helpers */
int io_sock_atohost(io_sock_addr_t *host, char const *a);
char const *io_sock_hostoa(io_sock_addr_t const *sc);

int io_sock_atos(io_sock_addr_t *host, char const *a); // ascii to sock
char const *io_sock_stoa(io_sock_addr_t const *sc);    // sock to ascii


/* -------------------------------------------------------------------------- */
/* struct sockaddr helpers */
typedef
union _sockaddr {
	sa_family_t family;
	struct sockaddr sa;
	struct sockaddr_un un;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
} unisa_t;


/* -------------------------------------------------------------------------- */
size_t io_sock_set_addr(unisa_t *sa, io_sock_addr_t const *conf);
size_t io_sock_get_addr(io_sock_addr_t *conf, unisa_t const *sa);

#endif
