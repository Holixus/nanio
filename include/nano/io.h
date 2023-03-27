#ifndef NANO_IO_H
#define NANO_IO_H

/* ------------------------------------------------------------------------ */
#define IPV6_SOCKET
#define UNIX_SOCKET
#define IPV4_SOCKET

/* -------------------------------------------------------------------------- */

typedef void (io_atexit_fn)(void *self);

void io_atexit(io_atexit_fn *fn, void *self);


extern char const *io_prog_name;

void start(int argc, char *argv[]);

#endif
