#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

#include <errno.h>
#include <syslog.h>

#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/ip.h>

#include "nano/io.h"
#include "nano/io_signals.h"


/* ------------------------------------------------------------------------ */
void sig_any(int signo)
{
	switch (signo) {
	case SIGTERM:
	case SIGINT:
		exit(-1);
	}
}

/* ------------------------------------------------------------------------ */
void signals_init()
{
	signal(SIGTERM, sig_any);
	signal(SIGINT, sig_any);
	signal(SIGPIPE, SIG_IGN);
}
