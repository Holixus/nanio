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
#include "nano/io_stream.h"
#include "nano/io_ipv4.h"

#include "nano/io_map.h"
#include "nano/io_http.h"


#include "multi_server_log.i"
#include "multi_server_http.i"
#include "multi_server_port.i"
#include "multi_server_mqtt.i"


#define PORTS_OFFSET 2000

#define HTTP_PORT   (  80 + PORTS_OFFSET)
#define MQTT_PORT   (1883 + PORTS_OFFSET)
#define MODBUS_PORT ( 502 + PORTS_OFFSET)
#define FTP_PORT    (  21 + PORTS_OFFSET)
#define POP3_PORT   ( 110 + PORTS_OFFSET)
#define SMTP_PORT   (  25 + PORTS_OFFSET)
#define IMAP_PORT   ( 143 + PORTS_OFFSET)

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* -------------------------------------------------------------------------- */
static int tcp_serv_accept(io_d_t *iod)
{
	io_stream_listen_t *self = (io_stream_listen_t *)iod;

//	io_stream_debug(self, "port reqested");
	switch (self->sa.port) {
	case HTTP_PORT:
		return http_accept(self);
	case MQTT_PORT:
		return mqtt_accept(self);
/*	case FTP_PORT:
		return ftp_accept(self);
	case POP3_PORT:
		return pop3_accept(self);*/
	}
	return port_accept(self);
}


/* -------------------------------------------------------------------------- */
static io_vmt_t io_tcp_serv_vmt = {
	.name = "tcp_listen",
	.ancestor = &io_stream_listen_vmt,
	.u.stream = {
		.accept = tcp_serv_accept
	}
};


enum {
	S_TCP, S_UDP
};

typedef
struct serv {
	int port;
	int type;
	char const *name;
	char const *iface;
} serv_t;


/* -------------------------------------------------------------------------- */
static int serv_start(serv_t const *s)
{
	io_stream_listen_conf_t conf = {
		.queue_size = 32
	};
	strcpy(conf.iface, s->iface ?: "");
	io_sock_any(&conf.sa, AF_INET, s->port);
	debug("listen: '%s' [%s]", io_sock_stoa(&conf.sa), s->name);
	/*io_stream_listen_t *self = */io_stream_listen_create(NULL, &conf, &io_tcp_serv_vmt);
	return 0;
}


/* -------------------------------------------------------------------------- */
void start(int argc, char *argv[])
{
#define S(NAME) { NAME##_PORT, S_TCP, #NAME, NULL }
	static const serv_t servers[] = {
		S(HTTP), S(FTP), S(MQTT), S(MODBUS), S(POP3), S(IMAP), S(SMTP)
	};

	for (serv_t const *s = servers; s < servers + countof(servers); ++s)
		serv_start(s);
}
