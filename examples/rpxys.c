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
#include "nano/io_sock_addr.h"
#include "nano/io_ipv4.h"
#include "nano/io_ds.h"
#include "nano/io_buf.h"
#include "nano/io_buf_d.h"
#include "nano/io_stream.h"
#include "nano/io_map.h"
#include "nano/io_http.h"

enum {
	ST_RECV_HEADER,
	ST_PIPE,
	ST_FORWARDING,
	ST_CLOSE
};


typedef
struct io_http_con {
	io_buf_sock_t bs;
	unsigned int id;
	int state;

	char *end;
	char const *header, *body;

	io_http_req_t req;
	io_hmap_t *params;

	char req_hdr[8000];
} http_con_t;


/* -------------------------------------------------------------------------- */
static int http_empty_response(http_con_t *c, int code)
{
	io_buf_sock_writef(&c->bs, "HTTP/%d.%d %03d %s\r\nConnection: close\r\nContent-Length: 0\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", c->req.version >> 8, c->req.version & 255, code, io_http_code(code));
	c->state = ST_CLOSE;
	return 0;
}


/* -------------------------------------------------------------------------- */
static int proxy_process_request(http_con_t *c)
{
	switch (c->req.method) {
	case HTTP_CONNECT:
	default:;
	}

	syslog(LOG_NOTICE, "<%s> %s %s HTTP/%d.%d", io_sock_stoa(&c->bs.conf), io_http_req_method(c->req.method), c->req.path, c->req.version >> 8, c->req.version & 255);
	http_empty_response(c, HTTP_NO_CONTENT);
	return 0;
}


/* -------------------------------------------------------------------------- */
static int proxy_stream_close(io_d_t *iod)
{
	http_con_t *c = (http_con_t *)iod;
	if (c->params)
		free(c->params);
	return 0;
}

/* -------------------------------------------------------------------------- */
static int proxy_all_sent_handler(io_d_t *iod)
{
	http_con_t *c = (http_con_t *)iod;
	syslog(LOG_NOTICE, "<%s> all_sent", io_sock_stoa(&c->bs.conf));
	if (c->state == ST_CLOSE) {
		io_d_free(iod); // end of connection
		syslog(LOG_NOTICE, "<%s> .. close", io_sock_stoa(&c->bs.conf));
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int proxy_stream_recv(io_d_t *iod)
{
	http_con_t *c = (http_con_t *)iod;
	//syslog(LOG_NOTICE, "<%s> recv [%d]", io_sock_stoa(&c->bs.conf), c->state);
	if (c->state == ST_RECV_HEADER) {
		size_t to_recv = (unsigned)(sizeof c->req_hdr - 1 - (c->end - c->req_hdr));
		ssize_t len;
		do {
			len = io_buf_sock_recv(&c->bs, c->end, to_recv, 0);
		} while (len < 0 && errno == EINTR);
		if (len < 0) {
			syslog(LOG_ERR, "<%s> recv failed: %m", io_sock_stoa(&c->bs.conf));
			return -1;
		}
		if (!len) {
			io_buf_sock_free(&c->bs); // end of connection
			syslog(LOG_NOTICE, "<%s> closed", io_sock_stoa(&c->bs.conf));
			return 0;
		}
		c->end[len] = 0;
		int has_body = !!strstr(c->end, "\r\n\r\n");
		c->end += len;
		if (!has_body)
			return 0;

		char const *s = c->req_hdr;
		if (io_http_req_parse(&c->req, &s) < 0)
			return http_empty_response(c, HTTP_BAD_REQUEST);

		c->header = s;
		c->params = io_hmap_create(48, 8192);
		if (io_http_header_parse(c->params, &s) < 0)
			return http_empty_response(c, HTTP_BAD_REQUEST);

		c->body = s;
		proxy_process_request(c);
		return 0;
	}
	if (c->state == ST_PIPE) {
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
io_vmt_t proxy_stream_vmt = {
	.name = "io_proxy_stream",
	.ancestor = &io_stream_vmt,
	.u.stream = {
		.close    = proxy_stream_close,
		.recv  = proxy_stream_recv,
		.all_sent = proxy_all_sent_handler
	}
};


/* -------------------------------------------------------------------------- */
typedef
struct proxy_server {
	io_stream_listen_t ls;
	unsigned int next_con_id;
} proxy_server_t;

/* -------------------------------------------------------------------------- */
static int proxy_server_con_id(proxy_server_t *ps)
{
	return ps->next_con_id++;
}

/* -------------------------------------------------------------------------- */
static int proxy_accept(io_d_t *iod)
{
	proxy_server_t *self = (proxy_server_t *)iod;
	http_con_t *t = (http_con_t *)calloc(1, sizeof (http_con_t));

	t->id = proxy_server_con_id(self);
	t->end = t->req_hdr;

	if (!io_stream_accept(&t->bs, &self->ls, &proxy_stream_vmt)) {
		syslog(LOG_ERR, "<%s> failed to accept: %m", io_sock_stoa(&self->ls.conf));
		return -1;
	} else {
		char const *sid = io_sock_stoa(&self->ls.conf);
		syslog(LOG_NOTICE, "<%s> accept: '%s'", sid, io_sock_stoa(&t->bs.conf));
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
static io_vmt_t io_proxy_server_vmt = {
	.name = "io_proxy_server",
	.ancestor = &io_stream_listen_vmt,
	.u.stream = {
		.accept = proxy_accept
	}
};

/* -------------------------------------------------------------------------- */
static int proxy_server_create(io_sock_addr_t *sock, int queue_size)
{
	io_stream_listen_conf_t conf = {
		.queue_size = queue_size ?: 32
	};
	conf.iface[0] = 0;
	conf.sock = *sock;
	syslog(LOG_NOTICE, "listen: '%s'", io_sock_stoa(sock));
	proxy_server_t *self = (proxy_server_t *)calloc(1, sizeof (proxy_server_t));
	/*io_sock_listen_t *self = */io_stream_listen_create(&self->ls, &conf, &io_proxy_server_vmt);
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
"Usage: %s <options> [ip[:port]]+ \n\n\
options:\n\
  -d, --daemon\t\t\t: start as daemon;\n\
  -v, --verbose\t\t\t: set verbose mode;\n\
  -f, --pid-file=<filename>\t: set PID file name;\n\
  -h\t\t\t\t: print this help and exit.\n\n", io_prog_name);
			return;
		}
	}
_end_of_opts:;

	io_sock_addr_t servers[30];
	int servers_num = 0;

	if (optind < argc) {
		for (; optind < argc; ++optind) {
			char *arg = argv[optind];
			io_sock_addr_t *s = servers + servers_num;
			if (io_sock_atos(s, arg) < 0)
				syslog(LOG_ERR, "invalid argument: '%s' (%m)", arg);
			else
				++servers_num;
		}
	}

	if (daemon_mode) {
		if (daemon(0, 0) < 0) {
			syslog(LOG_ERR, "daemonize failed (%m)");
			exit(1);
		}
	}

	create_pid_file();

	io_atexit(free_all);

	for (int i = 0; i < servers_num; ++i) {
		io_sock_addr_t *s = servers + i;
		proxy_server_create(s, 32);
	}
}
