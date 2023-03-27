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
#include "nano/io_ipv4.h"
#include "nano/io_dgram_client.h"


int verbose_mode = 0;

typedef struct dns_res io_dns_req_t;


typedef
struct dns4_req {
	io_dgram_client_t c;
	io_ipv4_sock_addr_t servs[20];
	size_t servs_num;
	char const *domain;
} io_dns_req_t;


struct dns6_req {
	io_dgram_client_t c;
	io_ipv6_sock_addr_t servs[20];
	size_t servs_num;
	char const *domain;
} io_dns6_req_t;


static int io_nslookup_recv_handler(io_dgram_client_t *self, io_sock_addr_t const *from, void *data, size_t size)
{
	io_dns_req_t *req = (io_dns_req_t *)self;
}


/* -------------------------------------------------------------------------- */
static void nslookup_create(char const *domain, io_sock_addr_t const *servs, int servs_num)
{
	printf("create nslookup %s\n", domain);
	io_dns_req_t *req = (io_dns_req_t *)malloc(sizeof (io_dns_req_t *));

	req->domain = domain;
	req->servers = servs;
	req->servers_num = servs_num;

	io_dgram_client_create(&req->c, serv->family, NULL, io_nslookup_recv_handler);
}


/* -------------------------------------------------------------------------- */
#define SERVERS_LIMIT 16
static io_sock_addr_t servers[SERVERS_LIMIT];
static int servers_num = 0;


/* -------------------------------------------------------------------------- */
static int add_dns_server(char const *arg)
{
	if (servers_num >= SERVERS_LIMIT) {
		syslog(LOG_ERR, "add dns server '%s': too long servers list. ignored", arg);
		return -1;
	}
	io_sock_addr_t *s = servers + servers_num;
	if (io_sock_atos(s, arg) < 0) {
		syslog(LOG_ERR, "Invalid server address: '%s'", arg);
		return -1;
	}
	if (s->family == AF_UNIX) {
		syslog(LOG_ERR, "unix socket is not supported");
		return -1;
	}
	if (!s->port)
		s->port = 53;
	printf("added DNS server: %s\n", io_sock_stoa(s));
	++servers_num;
	return 0;
}

/* -------------------------------------------------------------------------- */
static int read_resolv_conf(char const *conf)
{
	char line[128];
	FILE *f = fopen(conf, "r");

	if (!f)
		return -1;

	while (fgets(line, sizeof(line), f) != NULL) {
		char nameserver[32], addr[80];
		sscanf(line, "%32s %64s", nameserver, addr);
		if (strcmp(nameserver, "nameserver") == 0) {
			add_dns_server(addr);
		}
	}

	fclose(f);
	return 0;
}

/* -------------------------------------------------------------------------- */
void start(int argc, char *argv[])
{
	static struct option const long_options[] = {
	/*     name, has_arg, *flag, chr */
		{ "verbose", 0, 0, 'v' },
		{ "help",    0, 0, 'h' },
		{ "server",  1, 0, 's' },
		{ "conf",    1, 0, 'c' },
		{ 0, 0, 0, 0 }
	};


	for (;;) {
		int option_index;
		switch (getopt_long(argc, argv, "?hv", long_options, &option_index)) {
		case -1:
			goto _end_of_opts;

		case 's':;
			if (add_dns_server(optarg) < 0)
				exit(1);
			break;

		case 'c':;
			if (read_resolv_conf(optarg) < 0)
				syslog(LOG_ERR, "failed to read '%s': %m", optarg);
			break;

		case 'v':
			verbose_mode = 1;
			break;

		case 'h':
		case '?':
			printf(
"Usage: %s <options> [domain]+ \n\n\
options:\n\
  -s, --server=dns_server;\n\
  -c, --conf=resolv.conf;\n\
  -v, --verbose\t\t\t: set verbose mode;\n\
  -h\t\t\t\t: print this help and exit.\n\n", io_prog_name);
			return;
		}
	}
_end_of_opts:;

	char *domains[32];
	int dc = 0;

	if (optind < argc) {
		regex_t r;
		if (regcomp(&r, "^[a-z0-9.-]+$", REG_EXTENDED) != 0) {
			syslog(LOG_CRIT, "A");
			exit(1);
		}
		for (; optind < argc; ++optind) {
			regmatch_t m[10];
			char *arg = argv[optind];
			if (!regexec(&r, arg, 10, m, 0)) {
				domains[dc++] = arg;
			} else {
				syslog(LOG_ERR, "invalid argument: '%s'", arg);
			}
		}
		regfree(&r);
	}

	for (int i = 0; i < dc; ++i)
		nslookup_create(domains[i]);
}
