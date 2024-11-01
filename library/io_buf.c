#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include <errno.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/ip.h>

#include "nano/io.h"
#include "nano/io_buf.h"
#include "nano/io_log.h"


/* ------------------------------------------------------------------------ */
io_seg_t *io_seg_new()
{
	io_seg_t *s = (io_seg_t *)malloc(sizeof (io_seg_t));
	s->begin = s->end = s->data;
	s->next = 0;
	return s;
}


/* ------------------------------------------------------------------------ */
void io_buf_init(io_buf_t *b)
{
	b->first = b->last = NULL;
}

/* ------------------------------------------------------------------------ */
void io_buf_add(io_buf_t *b, io_seg_t *s)
{
	if (!b->last) {
		b->first = b->last = s;
	} else {
		b->last->next = s;
		b->last = s;
	}
}

/* ------------------------------------------------------------------------ */
void io_buf_free(io_buf_t *b)
{
	if (!b->first)
		return ;

	io_seg_t *s = b->first, *n;
	do {
		n = s->next;
		free(s);
	} while ((s = n));
	b->first = b->last = NULL;
}

/* ------------------------------------------------------------------------ */
int io_buf_length(io_buf_t *b)
{
	int len = 0;
	for (io_seg_t *s = b->first; s; s = s->next)
		len += s->end - s->begin;
	return len;
}

/* ------------------------------------------------------------------------ */
static io_seg_t *io_buf_get_last_seg(io_buf_t *b)
{
	io_seg_t *s = b->last;
	if (!s)
		b->first = b->last = s = io_seg_new();
	return s;
}

/* ------------------------------------------------------------------------ */
static size_t io_buf_get_seg_to_write(io_buf_t *b, io_seg_t **ps)
{
	io_seg_t *s = *ps;
	size_t to_recv = sizeof s->data - (unsigned)(s->end - s->data);
	if (!to_recv) {
		s = io_seg_new();
		b->last->next = s;
		b->last = s;
		to_recv = sizeof s->data - (unsigned)(s->end - s->data);
	}
	*ps = s;
	return to_recv;
}

/* ------------------------------------------------------------------------ */
int io_buf_send(io_buf_t *b, int sd)
{
	ssize_t total = 0;

	io_seg_t *s;
	while ((s = b->first)) {
		ssize_t to_send = (int)(s->end - s->begin);
		if (to_send) {
			ssize_t sent;
			do {
				sent = send(sd, s->begin, (size_t)to_send, 0);
			} while (sent < 0 && errno == EINTR);
			if (sent < 0)
				return -1;
			s->begin += sent;
			total += sent;
			if (sent < to_send)
				return (int)total;
		}
		if (!(b->first = s->next))
			b->last = NULL;
		free(s);
	}

	return (int)total;
}

/* ------------------------------------------------------------------------ */
int io_buf_recv(io_buf_t *b, int sd)
{
	ssize_t total = 0;

	io_seg_t *s = io_buf_get_last_seg(b);

_recv_more:;
	size_t to_recv = io_buf_get_seg_to_write(b, &s);
	ssize_t len;
	do {
		len = recv(sd, s->end, to_recv, 0);
	} while (len < 0 && errno == EINTR);
	if (len < 0)
		return -1;

	s->end += len;
	total += len;
	if ((size_t)len == to_recv)
		goto _recv_more;

	return (int)total;
}

/* ------------------------------------------------------------------------ */
ssize_t io_buf_write(io_buf_t *b, void *vdata, size_t size)
{
	if (!size)
		return 0;

	size_t tail = size;
	ssize_t total = 0;

	io_seg_t *s = io_buf_get_last_seg(b);
	uint8_t const *data = (uint8_t const *)vdata;

	do {
		size_t to_recv = io_buf_get_seg_to_write(b, &s);

		size_t len = to_recv > tail  ? tail : to_recv;
		memcpy(s->end, data, len);
		data += len;
		tail -= len;

		s->end += len;
		total += len;
	} while (tail);

	return total;
}

/* ------------------------------------------------------------------------ */
ssize_t io_buf_read(io_buf_t *b, void *vdata, size_t size)
{
	size_t tail = size;
	ssize_t total = 0;

	uint8_t *data = (uint8_t *)vdata;

	io_seg_t *s;
	while (tail && (s = b->first)) {
		ssize_t to_send = (int)(s->end - s->begin);
		if (to_send) {
			size_t sent = to_send > tail ? tail : to_send;
			memcpy(data, s->begin, sent);
			data += sent;
			tail -= sent;

			s->begin += sent;
			total += sent;

			if (sent < to_send)
				return total;
		}
		if (!(b->first = s->next))
			b->last = NULL;
		free(s);
	}

	return total;
}

