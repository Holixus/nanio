#ifndef NANO_SOCKS_H
#define NANO_SOCKS_H


/* -------------------------------------------------------------------------- */
typedef struct io_d io_d_t;

typedef int (*io_d_poll_handler_t)(io_d_t *d);

typedef
int (io_d_event_handler_t)(io_d_t *iod, int events);


/* -------------------------------------------------------------------------- */
typedef
struct {
	void (*free)(io_d_t *d);
	void (*idle)(io_d_t *d);
	int  (*event)(io_d_t *d, int events);
} io_d_ops_t;

/* -------------------------------------------------------------------------- */
struct io_d {
	io_d_t *next;
	int fd;
	int events;
	io_d_ops_t const *ops;
};

/* -------------------------------------------------------------------------- */
void io_d_init(io_d_t *self, int fd, int events, io_d_ops_t const *ops);
void io_d_free(io_d_t *self);

/* -------------------------------------------------------------------------- */
/* internals */
void io_ds_init();
void io_ds_free();
int  io_ds_poll(int timeout);

#endif
