#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "nano/io.h"
#include "nano/io_log.h"
#include "nano/io_debug.h"


static char const io_trace_file[] = "/tmp/io.log";

/* -------------------------------------------------------------------------- */
void trace_clean()
{
	close(open(io_trace_file, O_CREAT|O_TRUNC|O_WRONLY, 0666));
}

/* -------------------------------------------------------------------------- */
void trace(char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int fd = open(io_trace_file, O_APPEND|O_CREAT|O_WRONLY, 0666);
	char msg[256];
	if (write(fd, msg, (size_t)vsnprintf(msg, sizeof msg, fmt, ap)) < 0)
		io_err("io_trace");
	close(fd);
	va_end(ap);
}

/* -------------------------------------------------------------------------- */
void die(char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

