#ifndef NANO_IO_H
#define NANO_IO_H

/* ------------------------------------------------------------------------ */
#define IPV6_SOCKET
#define UNIX_SOCKET
#define IPV4_SOCKET

/* -------------------------------------------------------------------------- */

typedef void (void_fn_t)();

void io_atexit(void_fn_t *fn);


extern char const *io_prog_name;

void start(int argc, char *argv[]);

#endif
