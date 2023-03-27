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


typedef
struct io_http_client {
	io_buf_sock_t bs;
	int state;

	char *end;
	char const *header, *body;

	io_http_resp_t resp;
	io_hmap_t *params;

	char req_hdr[8000];
} http_con_t;

