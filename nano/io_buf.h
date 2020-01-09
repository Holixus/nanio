#ifndef NANO_BUFS_H
#define NANO_BUFS_H

#define IO_BUFFER_SEGMENT_SIZE (2048)

typedef struct io_seg io_seg_t;

/* ------------------------------------------------------------------------ */
struct io_seg {
	io_seg_t *next;
	char *begin, *end;
	char data[IO_BUFFER_SEGMENT_SIZE - 3 * sizeof (void*)];
};

typedef
struct io_buf {
	io_seg_t *first, *last;
} io_buf_t;



/* ------------------------------------------------------------------------ */
io_seg_t *io_seg_new();

void io_buf_init(io_buf_t *b);
void io_buf_add(io_buf_t *b, io_seg_t *s);
void io_buf_free(io_buf_t *b);

int io_buf_send(io_buf_t *b, int sock);
int io_buf_recv(io_buf_t *b, int sock);

#endif

