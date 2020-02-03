#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

#include <syslog.h>
#include <regex.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/poll.h>


#include "nano/io.h"
#include "nano/io_ds.h"
#include "nano/io_buf.h"
#include "nano/io_buf_d.h"
#include "nano/io_stream.h"
#include "nano/io_ipv4.h"


/* -------------------------------------------------------------------------- */
typedef struct io_port io_port_t;

/* -------------------------------------------------------------------------- */
struct io_port {
	io_buf_sock_t bs;
	char request[200];
	char *end;
	io_stream_listen_t *up;
};


/* -------------------------------------------------------------------------- */
static void port_cmd(io_port_t *self, char *cmd)
{
	char const *sid = io_sock_stoa(&self->up->conf);
	syslog(LOG_NOTICE, "<%s> cmd: '%s'", sid, cmd);
	if (!strcmp(cmd, "quit")) {
		io_buf_sock_writef(&self->bs, "%s: bye!\r\n", sid);
		io_d_free(&self->bs.bd.d);
		syslog(LOG_NOTICE, "<%s> closed", sid);
		return ;
	} else {
		io_buf_sock_writef(&self->bs, "%s: unknown command: '%s'\r\n", sid, cmd);
	}
}


/* -------------------------------------------------------------------------- */
static void port_event_handler(io_d_t *d, int events)
{
	io_port_t *p = (io_port_t *)d;
	if (events & POLLIN) {
		size_t to_recv = (sizeof p->request) - (unsigned)(p->end - p->request);
		ssize_t len;
		do {
			len = io_buf_sock_recv(&p->bs, p->end, to_recv, 0);
		} while (len < 0 && errno == EINTR);
		if (len < 0) {
			syslog(LOG_ERR, "failed to recv (%m)");
		} else
			if (!len) {
				io_d_free(d); // end of connection
				syslog(LOG_NOTICE, "<%s - %s> closed", io_sock_stoa(&p->up->conf), io_sock_stoa(&p->bs.conf));
			} else {
				char *cmd = p->request, *data = p->end, *lf, *end = p->end + len;

				while (end > data && (lf = (char *)memchr(data, '\n', (unsigned)(end - data)))) {
					*lf++ = 0;
					char *cr = memchr(cmd, '\r', (unsigned)(lf - cmd));
					if (cr)
						*cr = 0;
					port_cmd(p, cmd);
					data = cmd = p->end = lf;
				}
				if (end > data) {
					memmove(p->request, data, (unsigned)(end - data));
					p->end = p->request + (unsigned)(end - data);
				} else {
					p->end = p->request;
				}
			}
	}
}


/* -------------------------------------------------------------------------- */
static void port_accept(io_stream_listen_t *self)
{
	io_port_t *t = (io_port_t *)calloc(1, sizeof (io_port_t));
	t->up = self;

	t->end = t->request;

	if (!io_stream_accept(&t->bs, self, port_event_handler))
		syslog(LOG_ERR, "<%s> failed to accept: %m", io_sock_stoa(&self->conf));
	else {
		char const *sid = io_sock_stoa(&self->conf);
		syslog(LOG_NOTICE, "<%s> accept: '%s'", sid, io_sock_stoa(&t->bs.conf));
		io_buf_sock_writef(&t->bs, "%s: hello, %s!\r\n", sid, io_sock_stoa(&t->bs.conf));
	}
}

/* -------------------------------------------------------------------------- */
static int port_server_create(io_sock_addr_t *sock, uint32_t port_offset, char const *iface, int queue_size)
{
	io_stream_listen_conf_t conf = {
		.queue_size = queue_size ?: 32
	};
	if (iface)
		strcpy(conf.iface, iface);
	conf.sock = *sock;
	conf.sock.port += port_offset;
	//syslog(LOG_NOTICE, "listen: '%s'", io_sock_hosttoa(&conf));
	/*io_stream_listen_t *self = */io_stream_listen_create(&conf, port_accept);
	return 0;
}





static char *pid_file;
static int verbose_mode;

/* -------------------------------------------------------------------------- */
static void create_pid_file()
{
	if (!pid_file)
		return ;
	FILE *f = fopen(pid_file, "w");
	if (!f) {
		syslog(LOG_ERR, "failed to create pid-file '%s' (%m)", pid_file);
		return ;
	}

	fprintf(f, "%d\n", getpid());

	fclose(f);
}

/* -------------------------------------------------------------------------- */
static void free_all()
{
	if (pid_file)
		unlink(pid_file);
}

/* -------------------------------------------------------------------------- */
typedef
struct sock_addr {
	io_sock_addr_t conf;
	uint32_t ports_num;
} sock_addr_t;


/* -------------------------------------------------------------------------- */
void start(int argc, char *argv[])
{
	static struct option const long_options[] = {
	/*     name, has_arg, *flag, chr */
		{ "daemon",  0, 0, 'd' },
		{ "pid-file",1, 0, 'f' },
		{ "verbose", 0, 0, 'v' },
		{ "help",    0, 0, 'h' },
		{ 0, 0, 0, 0 }
	};

	int daemon_mode = 0;

	for (;;) {
		int option_index;
		switch (getopt_long(argc, argv, "?hdvf:", long_options, &option_index)) {
		case -1:
			goto _end_of_opts;

		case 'v':
			verbose_mode = 1;
			break;

		case 'd':
			daemon_mode = 1;
			break;

		case 'f':
			pid_file = strdup(optarg);
			break;

		case 'h':
		case '?':
			printf(
"Usage: %s <options> [ip:port[-port]]+ \n\n\
options:\n\
  -d, --daemon\t\t\t: start as daemon;\n\
  -v, --verbose\t\t\t: set verbose mode;\n\
  -f, --pid-file=<filename>\t: set PID file name;\n\
  -h\t\t\t\t: print this help and exit.\n\n", io_prog_name);
			return;
		}
	}
_end_of_opts:;

	sock_addr_t servs[30];
	int servs_num = 0;

	if (optind < argc) {
		regex_t r;
		if (regcomp(&r, "^(\\[[^]]+]|[0-9\\.]+)?:([0-9]{1,5})(-[0-9]{1,5})?$", REG_EXTENDED) != 0) {
			syslog(LOG_CRIT, "A");
			exit(1);
		}
		for (; optind < argc; ++optind) {
			regmatch_t m[10];
			char *arg = argv[optind];
			if (!regexec(&r, arg, 10, m, 0)) {
				sock_addr_t *s = servs + servs_num++;
				if (m[1].rm_so >= 0) {
					char a[50];
					size_t len = (unsigned)(m[1].rm_eo - m[1].rm_so);
					if (arg[m[1].rm_so] == '[') {
						len -= 2;
						memcpy(a, arg + m[1].rm_so + 1, len);
					} else
						memcpy(a, arg + m[1].rm_so, len);
					a[len] = 0;
					io_sock_atohost(&s->conf, a);
				}
				s->conf.port = atoi(arg + m[2].rm_so);
				s->ports_num = (m[3].rm_so > 0) ? atoi(arg + m[3].rm_so + 1) - s->conf.port + 1 : 1;
				if (s->ports_num < 1) {
					syslog(LOG_ERR, "invalid argument: '%s'", arg);
					--servs_num;
				} else
					syslog(LOG_NOTICE, "listen server: %s:%d-%d", io_sock_hostoa(&s->conf), s->conf.port, s->conf.port + s->ports_num - 1);
			} else {
				syslog(LOG_ERR, "invalid argument: '%s'", arg);
			}
		}
		regfree(&r);
	}

	if (daemon_mode) {
		if (daemon(0, 0) < 0) {
			syslog(LOG_ERR, "daemonize failed (%m)");
			exit(1);
		}
	}

	create_pid_file();

	io_atexit(free_all);

	for (int i = 0; i < servs_num; ++i) {
		sock_addr_t *s = servs + i;
		for (int n = 0; n < s->ports_num; ++n)
			port_server_create(&s->conf, n, NULL, 32);
	}
}
