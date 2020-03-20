#ifndef NANO_BUF_FDS_H
#define NANO_BUF_FDS_H


typedef
struct io_buf_d io_buf_d_t;

/* -------------------------------------------------------------------------- */
struct io_buf_d {
	io_d_t d;
	io_buf_t out;
};


/* -------------------------------------------------------------------------- */
io_buf_d_t *io_buf_d_create(io_buf_d_t *t, int fd, io_vmt_t *vmt);

/* -------------------------------------------------------------------------- */
int io_buf_d_write(io_d_t *d, char const *data, size_t size);
int io_buf_d_vwritef(io_d_t *d, char const *fmt, va_list ap);
int io_buf_d_writef(io_d_t *d, char const *fmt, ...) __attribute__ ((format (printf, 2, 3)));


/* -------------------------------------------------------------------------- */
/* internals */
io_vmt_t io_buf_d_vmt;

void io_buf_d_free(io_d_t *d);
int  io_buf_d_event_handler(io_d_t *d, int events);

#endif
