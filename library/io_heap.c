#include <stddef.h>
#include <stdint.h>
#include <ctype.h>

#include <string.h>

#include <errno.h>

#include <sys/types.h>

#include "nano/io.h"
#include "nano/io_heap.h"

#include "io_internals.h"


/* ------------------------------------------------------------------------ */
void io_oheap_init(io_oheap_t *h, char *data, size_t size)
{
	 h->end = (h->data = data) + size;
}

/* ------------------------------------------------------------------------ */
char *io_oheap_strdup(io_oheap_t *h, char const *v, size_t length)
{
	if (!length)
		length = strlen(v);
	size_t size = length + 1;
	if (h->end - h->data < size)
		return errno = ENOMEM, NULL;
	char *m = h->data;
	h->data += size;
	memcpy(m, v, length);
	m[length] = 0;
	return m;
}

