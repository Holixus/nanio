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

#include "nano/io_timers.h"
#include "nano/io_ds.h"
#include "nano/io_signals.h"
#include "nano/io_debug.h"

#include "nano/io.h"
#include "nano/io_log.h"

#define MAX_ATEXIT_HANDLERS (10)

static struct at_exit_cb {
	io_atexit_fn *fn;
	void *self;
} at_exits[MAX_ATEXIT_HANDLERS];

static size_t ae_len;

/* -------------------------------------------------------------------------- */
void io_atexit(io_atexit_fn *fn, void *self)
{
	if (ae_len < MAX_ATEXIT_HANDLERS) {
		at_exits[ae_len++] = (struct at_exit_cb){ fn, self };
	} else
		io_err("%s queue full", __FUNCTION__);
}

/* -------------------------------------------------------------------------- */
static void io_free()
{
	for (int i = 0; i < ae_len; ++i)
		at_exits[i].fn(at_exits[i].self);

	io_timers_free();
	io_ds_free();
	io_log_close();
}

char const *io_prog_name;

/* -------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
	ae_len = 0;
	signals_init();
	srand((unsigned int)time(NULL));

	io_prog_name = strrchr(argv[0], '/') ?: argv[0];
	if (*io_prog_name == '/')
		++io_prog_name;

	io_log_open(io_prog_name);

	io_timers_init();
	io_ds_init();

	atexit(io_free);

	start(argc, argv);

	if (getppid() == 1)
		io_log_open(io_prog_name);

	int ret;
	do {
		ret = io_ds_poll(io_get_timeout());
	} while (ret > 0 || (ret < 0 && errno == EINTR) || io_has_active_timers());

	return 0;
}

