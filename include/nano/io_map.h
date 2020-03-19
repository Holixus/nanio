#ifndef NANO_IO_KVS_H
#define NANO_IO_KVS_H

#include "io_heap.h"

typedef
struct io_kv {
	char const *key;
	char const *value;
} io_kv_t;

/* ------------------------------------------------------------------------ */
typedef
struct io_map {
	io_kv_t *last, *kend;
	io_kv_t kvs[0];
} io_map_t;

/* ------------------------------------------------------------------------ */
io_map_t *io_map_create(size_t keys_limit);

int io_map_add(io_map_t *m, char const *key, char const *value);
//int io_map_addl(io_map_t *m, char *key, size_t key_size, char *value, size_t value_size);

char const *io_map_getc(io_map_t *m, char const *key);
char const *io_map_getcs(io_map_t *m, char const *key, size_t size);


/* ------------------------------------------------------------------------ */
typedef
struct io_kvs {
	io_oheap_t heap;
	io_map_t map;
} io_hmap_t;

/* ------------------------------------------------------------------------ */
io_hmap_t *io_hmap_create(size_t keys_limit, size_t heap_size);

int io_hmap_add(io_hmap_t *h, char const *key, char const *value);
int io_hmap_addl(io_hmap_t *h, char const *key, size_t key_len, char const *value, size_t value_len);

static inline char const *io_hmap_getc(io_hmap_t *h, char const *key)
{ return io_map_getc(&h->map, key); }

static inline char const *io_hmap_getcs(io_hmap_t *h, char const *key, size_t size)
{ return io_map_getcs(&h->map, key, size); }


#endif
