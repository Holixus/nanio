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
#include "nano/io_sock.h"
#include "nano/io_ipv4.h"


/* -------------------------------------------------------------------------- */
#define IO_CON_BUFFER_SIZE (0x1000)


typedef struct io_port io_port_t;

/* -------------------------------------------------------------------------- */
struct io_port {
	io_buf_sock_t bs;
	char request[200];
	char *end;
	io_sock_listen_t *up;
};


/* -------------------------------------------------------------------------- */
static void port_free(io_d_t *d)
{
	io_port_t *p = (io_port_t *)d;
	io_sock_free(&p->bs.bd.d);
	p->up = NULL;
}


/* -------------------------------------------------------------------------- */
static void port_cmd(io_port_t *self, char *cmd)
{
	int port = self->up->conf.port;
	syslog(LOG_NOTICE, "<%d> cmd: '%s'", port, cmd);
	if (!strcmp(cmd, "quit")) {
		io_buf_sock_writef(&self->bs, "%d: bye!\r\n", port);
		io_d_free(&self->bs.bd.d);
		syslog(LOG_NOTICE, "<%d> closed", port);
		return ;
	} else {
		io_buf_sock_writef(&self->bs, "%d: unknown command: '%s'\r\n", port, cmd);
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
				int port = p->up->conf.port;
				io_d_free(d); // end of connection
				syslog(LOG_NOTICE, "<%d> closed", port);
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
static void port_accept(io_sock_listen_t *self)
{
	io_port_t *t = (io_port_t *)calloc(1, sizeof (io_port_t));
	t->up = self;

	t->end = t->request;

	if (!io_sock_accept(&t->bs, self, port_event_handler))
		syslog(LOG_ERR, "<%d> failed to accept: %m", self->conf.port);
	else {
		syslog(LOG_NOTICE, "<%d> accept: '%s'", self->conf.port, io_sock_hostoa(&t->bs.conf));
		io_buf_sock_writef(&t->bs, "%d: hello, %s!\r\n", self->conf.port, io_sock_hostoa(&t->bs.conf));
	}
}

/* -------------------------------------------------------------------------- */
static int port_server_create(uint32_t ip, uint32_t port, char const *iface, int queue_size)
{
	io_sock_listen_conf_t conf = {
		.queue_size = queue_size ?: 32
	};
	if (iface)
		strcpy(conf.iface, iface);
	conf.sock.port = port;
	conf.sock.family = AF_INET;
	conf.sock.addr.ipv4 = ip;
	//syslog(LOG_NOTICE, "listen: '%s'", io_sock_hosttoa(&conf));
	/*io_sock_listen_t *self = */io_sock_listen_create(&conf, port_accept);
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
struct ip_sock_addr {
	uint32_t ip;
	uint32_t port;
	uint32_t ports_num;
} ip_sock_addr_t;


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

	ip_sock_addr_t servs[30];
	int servs_num = 0;

	if (optind < argc) {
		regex_t r;
		if (regcomp(&r, "^([0-9.]{7,15})?:([0-9]{1,5})(-[0-9]{1,5})?$", REG_EXTENDED) != 0) {
			syslog(LOG_CRIT, "A");
			exit(1);
		}
		for (; optind < argc; ++optind) {
			regmatch_t m[10];
			char *arg = argv[optind];
			if (!regexec(&r, arg, 10, m, 0)) {
				ip_sock_addr_t *s = servs + servs_num++;
				s->ip = m[1].rm_so >= 0 ? ipv4_atoi(arg + m[1].rm_so) : 0;
				s->port = atoi(arg + m[2].rm_so);
				s->ports_num = (m[3].rm_so > 0) ? atoi(arg + m[3].rm_so + 1) - s->port + 1 : 1;
				if (s->ports_num < 1) {
					syslog(LOG_ERR, "invalid argument: '%s'", arg);
					--servs_num;
				} else
					syslog(LOG_NOTICE, "listen server: %s:%d-%d", ipv4_itoa(s->ip), s->port, s->port + s->ports_num - 1);
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
		ip_sock_addr_t *s = servs + i;
		for (int n = 0; n < s->ports_num; ++n)
			port_server_create(s->ip, s->port + n, NULL, 32);
	}
}
