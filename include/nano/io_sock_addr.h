#ifndef NANO_SOCK_ADDR_H
#define NANO_SOCK_ADDR_H

#include <sys/un.h>
#include <netinet/ip.h>


#ifdef IPV4_SOCKET

typedef uint32_t io_ipv4_t;

/* -------------------------------------------------------------------------- */
typedef struct io_ipv4_sock_addr {
	uint16_t port;
	uint16_t family;    // always AF_INET
	union {
		io_ipv4_t ipv4; // uint32_t
	} addr;
} io_ipv4_sock_addr_t;

#endif


#ifdef IPV6_SOCKET

typedef uint16_t io_ipv6_t[8];

/* -------------------------------------------------------------------------- */
typedef struct io_ipv6_sock_addr {
	uint16_t port;
	uint16_t family;    // always AF_INET6
	union {
		io_ipv6_t ipv6; // uint16_t[8]
	} addr;
} io_ipv6_sock_addr_t;

#endif


#ifdef UNIX_SOCKET

/* -------------------------------------------------------------------------- */
typedef struct io_unix_sock_addr {
	uint16_t port;
	uint16_t family;    // always AF_UNIX
	union {
		char const *path;
	} addr;
} io_unix_sock_addr_t;

#endif

/* -------------------------------------------------------------------------- */
typedef
struct io_sock_addr {
	uint16_t port;
	uint16_t family;
	union {
#ifdef UNIX_SOCKET
		char const *path;
#endif
#ifdef IPV4_SOCKET
		io_ipv4_t ipv4; // uint32_t
#endif
#ifdef IPV6_SOCKET
		io_ipv6_t ipv6; // uint16_t[8]
#endif
	} addr;
} io_sock_addr_t;


/* -------------------------------------------------------------------------- */
/* socket address helpers */
// just address (like as "/tmp/unix.sock", "127.0.0.1", "fe00::0")
int io_sock_atohost(io_sock_addr_t *host, char const *a);
char const *io_sock_hostoa(io_sock_addr_t const *sc);

// address with port (like as "/tmp/unix.sock", "127.0.0.1:80", "[fe00::0/80]")
int io_sock_atos(io_sock_addr_t *host, char const *a); // ascii to sock
char const *io_sock_stoa(io_sock_addr_t const *sc);    // sock to ascii


int io_sock_any(io_sock_addr_t *host, int family, int port);

/* -------------------------------------------------------------------------- */
/* struct sockaddr helpers */
typedef
union _sockaddr {
	struct sockaddr sa;
#ifdef UNIX_SOCKET
	struct sockaddr_un un;
#endif
#ifdef IPV4_SOCKET
	struct sockaddr_in in;
#endif
#ifdef IPV6_SOCKET
	struct sockaddr_in6 in6;
#endif
} unisa_t;


/* -------------------------------------------------------------------------- */
size_t io_sock_set_addr(unisa_t *usa, io_sock_addr_t const *sa);
size_t io_sock_get_addr(io_sock_addr_t *sa, unisa_t const *usa);

/* -------------------------------------------------------------------------- */
int io_socket(int type, int family, char const *iface);
int io_binded_socket(int type, io_sock_addr_t *sa, char const *iface);

#endif
