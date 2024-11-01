#include "io_tag.h"


/* -------------------------------------------------------------------------- */
io_sock_addr_t const *io_untag(io_tags_t const *t, int tag)
{
	return tag < t->num ? t->addrs + tag : NULL;
}


/* -------------------------------------------------------------------------- */
int io_tag(io_tags_t const *t, io_sock_addr_t const *s)
{
	for (int tag = 0, n = t->num; tag < n; ++tag) {
		io_sock_addr_t const *c = t->addrs + tag;
		if (c->family != s->family)
			continue;
		switch (s->family) {
		case AF_INET:
			if (s->addr.ipv4 == c->addr.ipv4)
				return tag;
		case AF_INET6:
			if (!memcmp(s->addr.ipv6, c->addr.ipv6, sizeof c->addr.ipv6))
				return tag;
		}
	}
	return -1;
}
