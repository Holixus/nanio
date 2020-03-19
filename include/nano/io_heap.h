#ifndef NANO_IO_OHEAP_H
#define NANO_IO_OHEAP_H

/* ------------------------------------------------------------------------ */
typedef
struct io_oheap {
	char *data, *end;
} io_oheap_t;

/* ------------------------------------------------------------------------ */
void io_oheap_init(io_oheap_t *h, char *data, size_t size);
char *io_oheap_strdup(io_oheap_t *h, char const *v, size_t length);

#endif
