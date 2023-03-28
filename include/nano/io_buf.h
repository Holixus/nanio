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
void io_buf_add (io_buf_t *b, io_seg_t *s);
void io_buf_free(io_buf_t *b);

int io_buf_send(io_buf_t *b, int sd);
int io_buf_recv(io_buf_t *b, int sd);

static inline int io_buf_is_empty(io_buf_t *b) { return !b->first; }

ssize_t io_buf_write(io_buf_t *b, void *data, size_t size);
ssize_t io_buf_read (io_buf_t *b, void *data, size_t size);

#endif

