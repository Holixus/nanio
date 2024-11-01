#ifndef NANO_IO_TAG_H
#define NANO_IO_TAG_H

typedef
struct io_tags {
	size_t num;
	io_sock_addr_t addrs[0];
} io_tags_t;

io_sock_addr_t const *io_untag(io_tags_t const *t, int tag);

int io_tag(io_tags_t const *t, io_sock_addr_t const *addr);

#endif
