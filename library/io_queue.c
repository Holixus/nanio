#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/ip.h>

#include "nano/io.h"
#include "nano/io_log.h"
#include "nano/io_buf.h"

#include "nano/io_queue.h"


/* ------------------------------------------------------------------------ */
void io_queue_init(io_queue_t *q)
{
	q->first = q->last = NULL;
}


/* ------------------------------------------------------------------------ */
static void io_queue_block_free(io_queue_block_t *b)
{
	if (b->type)
		free(b->data);
}

/* ------------------------------------------------------------------------ */
static void io_queue_seg_free(io_queue_seg_t *s)
{
	if (s->begin < s->end)
		for (io_queue_block_t *b = s->begin; b < s->end; ++b)
			io_queue_block_free(b);
	s->begin = s->end = s->blocks;
}

/* ------------------------------------------------------------------------ */
void io_queue_free(io_queue_t *q)
{
	if (!q->first)
		return ;

	io_queue_seg_t *s = q->first, *n;

	q->first = q->last = NULL;

	do {
		n = s->next;
		io_queue_seg_free(s);
	} while ((s = n));
}


/* ------------------------------------------------------------------------ */
static io_queue_seg_t *io_queue_seg_new(io_queue_t *q)
{
	io_queue_seg_t *s = (io_queue_seg_t *)malloc(sizeof (io_queue_seg_t));
	s->begin = s->end = s->blocks;
	s->next = 0;
	if (q->last)
		q->last->next = s;
	else
		q->first = s;
	q->last = s;
	return s;
}

/* ------------------------------------------------------------------------ */
static io_queue_seg_t *io_queue_get_last_seg(io_queue_t *q)
{
	return q->last ?: io_queue_seg_new(q);
}

/* ------------------------------------------------------------------------ */
static io_queue_block_t *io_queue_new_block(io_queue_t *q)
{
	io_queue_seg_t *s = q->last;
	if (!s || s->end >= s->blocks + sizeof s->blocks / sizeof s->blocks[0])
		s = io_queue_seg_new(q);
	return s->end++;
}

/* ------------------------------------------------------------------------ */
int io_queue_put_block(io_queue_t *q, void const *tag, void *data, size_t size, int type)
{
	io_queue_block_t *b = io_queue_new_block(q);
	if ((b->type = type) == IOQB_LOCAL) {
		void *d = malloc(size);
		memcpy(d, data, size);
		b->data = d;
	} else
		b->data = (void *)data;
	b->size = size;
	b->tag = tag;
	return size;
}

/* ------------------------------------------------------------------------ */
io_queue_block_t *io_queue_get_block(io_queue_t *q)
{
	if (!q->first)
		return NULL;
	io_queue_seg_t *s = q->first;
	return s->begin < s->end ? s->begin : NULL;
}

/* ------------------------------------------------------------------------ */
int io_queue_block_drop(io_queue_t *q)
{
	if (!q->first)
		return -1;
	io_queue_seg_t *s = q->first;
	if (s->begin < s->end) {
		io_queue_block_free(s->begin++);
		if (s->begin >= s->end) {
			io_queue_seg_free(s); // reset segment
			if (s->next) {
				q->first = s->next;
				free(s);
			} /* else { don't free last segment by performance reason } */
		}
		return 0;
	}
	return -1;
}
