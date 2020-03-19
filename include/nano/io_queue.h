#ifndef NANO_QUEUE_H
#define NANO_QUEUE_H

#ifndef IO_QUEUE_PART_BLOCKS_LENGTH
#define IO_QUEUE_PART_BLOCKS_LENGTH 32
#endif


/* ------------------------------------------------------------------------ */
typedef struct io_queue_block io_queue_block_t;
typedef struct io_queue_seg io_queue_seg_t;
typedef struct io_queue io_queue_t;


/* ------------------------------------------------------------------------ */
enum io_queue_block_type {
//                  need to be freed    duplicate by put
	IOQB_STATIC, //       no                   no
	IOQB_DYNAMIC,//      yes                   no
	IOQB_LOCAL   //      yes                  yes
};

struct io_queue_block {
	uint16_t type;            // enum io_queue_block_type
	uint16_t size;
	void const *tag;
	void *data;
};


struct io_queue_seg {
	io_queue_seg_t *next;
	io_queue_block_t *begin, *end;
	io_queue_block_t blocks[IO_QUEUE_PART_BLOCKS_LENGTH];
};


/* ------------------------------------------------------------------------ */
struct io_queue {
	io_queue_seg_t *first, *last;
};


/* ------------------------------------------------------------------------ */
void io_queue_init(io_queue_t *q);
void io_queue_free(io_queue_t *q);

static inline int io_queue_is_empty(io_queue_t *q) { return !q->first; }

int io_queue_put_block(io_queue_t *q, void const *tag, void *data, size_t size, int type);

static inline int io_queue_put_cblock(io_queue_t *q, void const *tag, void const *data, size_t size)
{	return io_queue_put_block(q, tag, (void *)data, size, IOQB_STATIC); }


io_queue_block_t *io_queue_get_block(io_queue_t *q);

int io_queue_block_drop(io_queue_t *q);

#endif
