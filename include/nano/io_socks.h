#ifndef NANO_SOCKS_H
#define NANO_SOCKS_H


/* -------------------------------------------------------------------------- */
typedef struct sock io_sock_t;

typedef void (*io_sock_poll_handler_t)(io_sock_t *sock);

/* -------------------------------------------------------------------------- */
typedef
struct {
	void (*free)(io_sock_t *sock);
	void (*idle)(io_sock_t *sock);
	void (*event)(io_sock_t *sock, int events);
} io_sock_ops_t;

/* -------------------------------------------------------------------------- */
struct sock {
	io_sock_t *next;
	int fd;
	int events;
	io_sock_ops_t const *ops;
};

/* -------------------------------------------------------------------------- */
void io_sock_init(io_sock_t *self, int fd, int events, io_sock_ops_t const *ops);
void io_sock_free(io_sock_t *self);

/* -------------------------------------------------------------------------- */
/* internals */
void io_socks_init();
void io_socks_free();
int io_socks_poll(int timeout);

#endif
