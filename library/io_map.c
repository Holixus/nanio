#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <errno.h>

#include <string.h>

#include <sys/types.h>
#include <arpa/inet.h>

#include <ctype.h>

#include "nano/io.h"
#include "nano/io_heap.h"
#include "nano/io_map.h"

#include "io_internals.h"


/* ------------------------------------------------------------------------ */
io_map_t *io_map_create(io_map_t *m, size_t keys_limit)
{
	if (!m)
		m = malloc(sizeof m->kvs[0] * keys_limit + sizeof *m);

	m->last = m->kvs;
	m->kend = m->kvs + keys_limit;
	return m;
}


/* ------------------------------------------------------------------------ */
char const *io_map_getc(io_map_t *m, char const *key)
{
	for (io_kv_t *kv = m->kvs, *last = m->last; kv < last; ++kv) {
		if (!strcasecmp(key, kv->key))
			return kv->value;
	}
	return NULL;
}

/* ------------------------------------------------------------------------ */
char const *io_map_getcs(io_map_t *m, char const *key, size_t size)
{
	for (io_kv_t *kv = m->kvs, *last = m->last; kv < last; ++kv) {
		if (!strncasecmp(key, kv->key, size) && !kv->key[size])
			return kv->value;
	}
	return NULL;
}


/* ------------------------------------------------------------------------ */
static int io_map_new_kv(io_map_t *m, char const *key, char const *value)
{
	if (m->last >= m->kend)
		return errno = ENOMEM, -1;
	io_kv_t *kv = m->last++;
	kv->key = key;
	kv->value = value;
	return 0;
}


/* ------------------------------------------------------------------------ */
int io_map_add(io_map_t *m, char const *key, char const *value)
{
	return io_map_getc(m, key) ? (errno = EEXIST, -1) : io_map_new_kv(m, key, value);
}

/* ------------------------------------------------------------------------ * /
int io_map_addl(io_map_t *m, char *key, size_t key_len, char *value, size_t value_len)
{
	if (io_map_getcs(m, key, key_len))
		return errno = EEXIST, -1;
	key[key_len] = 0;
	value[value_len] = 0;
	return io_map_new_kv(m, key, value);
}
*/



/* ------------------------------------------------------------------------ */
io_hmap_t *io_hmap_create(io_hmap_t *m, size_t keys_limit, size_t heap_size)
{
	size_t kvs_size = sizeof m->map.kvs[0] * keys_limit;

	if (!m)
		m = malloc(kvs_size + heap_size + sizeof *m);

	io_map_create(&m->map, keys_limit);
	io_oheap_init(&m->heap, (char *)&m->map + sizeof m->map + kvs_size, heap_size);
	return m;
}

/* ------------------------------------------------------------------------ */
int io_hmap_add(io_hmap_t *m, char const *key, char const *value)
{
	if (io_hmap_getc(m, key))
		return errno = EEXIST, -1;
	return io_map_new_kv(&m->map,
		io_oheap_strdup(&m->heap, key, 0),
		io_oheap_strdup(&m->heap, value, 0));
}

/* ------------------------------------------------------------------------ */
int io_hmap_addl(io_hmap_t *m, char const *key, size_t key_len, char const *value, size_t value_len)
{
	if (io_hmap_getcs(m, key, key_len))
		return errno = EEXIST, -1;
	return io_map_new_kv(&m->map,
		io_oheap_strdup(&m->heap, key, key_len),
		io_oheap_strdup(&m->heap, value, value_len));
}
