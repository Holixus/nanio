#define DNS_BUF_SIZE 1024
#define DNS_SRV_PORT 53

#define SEND_DNS_REQ_NUM 3
#define MAX_DNS_NUM 3


typedef struct dns_query_header {
	uint16_t id;
	uint16_t tag;
	uint16_t numq;
	uint16_t numa;
	uint16_t numa1;
	uint16_t numa2;
} dns_query_header_t;


#define RR_NAMESIZE 512

typedef struct _rr {
	unsigned short flags;
	char name[RR_NAMESIZE];

	unsigned int type;
	unsigned int class;

	unsigned long ttl;
	int len;
	unsigned char data[RR_NAMESIZE];
} rr_t;

typedef struct _header {
	unsigned short int id;
	unsigned short u;

	short int qdcount;
	short int ancount;
	short int nscount;
	short int arcount; /* Till here it would fit perfectly to a real
	                    * DNS packet if we had big endian. */

	unsigned char *here;   /* For packet parsing. */
	unsigned char *packet; /* The actual data packet ... */
	int len;               /* ... with this size in bytes. */

	unsigned char *rdata; /* For packet assembly. */
} dns_header_t;


typedef struct dns_query {
	uint16_t type;
	uint16_t classes;
} dns_query_t;


/* ------------------------------------------------------------------------ */
int dns_readResolvConf(char const *resolv_conf, uint32_t *dns_ip, size_t limit)
{
	char *text = loadFile(resolv_conf, NULL);
	if (!text)
		return -1;

	char *line = text;
	int i = 0;
	while (line && i < limit) {
		char *next = strchr(line, '\n');
		if (!memcmp(line, "nameserver ", 11)) {
			for (line += 11; *line == ' '; ++line)
				;
			dns_ip[i++] = ipv4_atoi(line);
		}
		if ((line = next))
			++line;
	}

	free(text);
	return i;
}


/* ------------------------------------------------------------------------ */
int dns_writeResolvConf(char const *resolv_conf, uint32_t const *dns_ip, size_t count)
{
	size_t size = count * sizeof "nameserver 123.123.123.123\n" + 1;
	char *text = (char*)malloc(size), *p = text, *e = p + size;
	text[0] = 0;
	for (; count; --count, ++dns_ip)
		if (*dns_ip)
			p += snprintf(p, (size_t)(e - p), "nameserver %s\n", ipv4_itoa(*dns_ip));
	int ret = writeStrToFile(resolv_conf, text);
	free(text);
	return ret;
}


/* ------------------------------------------------------------------------ */
static int get_objectname(unsigned char *msg, unsigned const char *limit, unsigned char **here, char *string, int strlen, int k)
{
	unsigned int len;
	int i;

	if ((*here >= limit) || (k >= strlen))
		return (-1);
	while ((len = **here) != 0) {
		*here += 1;
		if (*here >= limit)
			return (-1);
		/* If the name is compressed (see 4.1.4 in rfc1035)  */
		if (len & 0xC0) {
			unsigned offset;
			unsigned char *p;

			offset = ((len & ~0xc0) << 8) + **here;
			if ((p = &msg[offset]) >= limit)
				return (-1);
			if (p == *here - 1) {
				// log_debug("looping ptr");
				return (-2);
			}

			if ((k = get_objectname(msg, limit, &p, string, RR_NAMESIZE, k)) < 0)
				return (-1); /* if we cross the limit, bail out */
			break;
		} else if (len < 64) {
			/* check so we dont pass the limits */
			if (((*here + len) > limit) || (len + k + 2 > strlen))
				return (-1);

			for (i = 0; i < len; i++) {
				string[k++] = **here;
				*here += 1;
			}

			string[k++] = '.';
		}
	}

	*here += 1;
	string[k] = 0;

	return (k);
}


/* ------------------------------------------------------------------------ */
static unsigned char *read_record(dns_header_t *x, rr_t *y, unsigned char *here, int question, unsigned const char *limit)
{
	int k, len;
	unsigned short int conv;
	unsigned short check_len = 0;
	/*
	 * Read the name of the resource ...
	 */

	k = get_objectname(x->packet, limit, &here, y->name, sizeof(y->name), 0);
	if (k < 0)
		return (NULL);
	y->name[k] = 0;

	/* safe to read TYPE and CLASS? */
	if ((here + 4) > limit)
		return (NULL);

	/*
	 * ... the type of data ...
	 */

	memcpy(&conv, here, sizeof(unsigned short int));
	y->type = ntohs(conv);
	here += 2;

	/*
	 * ... and the class type.
	 */

	memcpy(&conv, here, sizeof(unsigned short int));
	y->class = ntohs(conv);
	here += 2;

	/*
	 * Question blocks stop here ...
	 */

	if (question != 0)
		return (here);

	/*
	 * ... while answer blocks carry a TTL and the actual data.
	 */

	/* safe to read TTL and RDLENGTH? */
	if ((here + 6) > limit)
		return (NULL);
	memcpy(&y->ttl, here, sizeof(unsigned long int));
	y->ttl = ntohl(y->ttl);
	here += 4;

	/*
	 * Fetch the resource data.
	 */

	// memcpy(&y->len, here, sizeof(unsigned short int));
	memcpy(&check_len, here, sizeof(unsigned short));
	// printf("the check len=%X\n", check_len);

	len = y->len = ntohs(check_len);

	// printf("A the len =%X\n", y->len);
	// len = y->len = ntohs(y->len);

	here += 2;

	/* safe to read RDATA? */
	if ((here + y->len) > limit)
		return (NULL);

	if (y->len > sizeof(y->data) - 4)
		y->len = sizeof(y->data) - 4;

	memcpy(y->data, here, y->len);
	here += len;
	y->data[y->len] = 0;

	return (here);
}


/* ------------------------------------------------------------------------ */
static int mk_dns_query(unsigned char *buf, size_t size, char const *domain)
{
	dns_query_header_t *dnshdr = (dns_query_header_t *)buf;
	dns_query_t *dnsqer = (dns_query_t *)(buf + sizeof(dns_query_header_t));
	memset(buf, 0, size);
	dnshdr->id   = (uint16_t)1;
	dnshdr->tag  = htons(0x0100);
	dnshdr->numq = htons(1);
	dnshdr->numa = 0;

	size_t domain_len = (size_t)strlen(domain);

	unsigned char *p = buf + sizeof(dns_query_header_t) + 1;
	unsigned char *end = p + domain_len;

	memcpy(p, domain, domain_len);
	int i;
	for (i = 0; p < end; ++p) {
		if (*p != '.') {
			++i;
			continue;
		}
		p[-i-1] = i;
		i = 0;
	}
	p[-i-1] = i;

	dnsqer = (dns_query_t *)(end + 1);
	dnsqer->type    = htons(1); // address
	dnsqer->classes = htons(1); // internet

	return sizeof(dns_query_header_t) + sizeof(dns_query_t) + domain_len + 2;
}


/* ------------------------------------------------------------------------ */
static int parse_dns_response(unsigned char *recv_buf, int len, uint32_t *resolved, size_t res_limit)
{
	if (!(recv_buf[6] | recv_buf[7]))
		return errno = EPROTO, -1;

	dns_header_t x;
	memset(&x, 0, sizeof(dns_header_t));

	x.packet = recv_buf;
	x.len    = len;

	unsigned short int *p = (unsigned short *)x.packet;

	x.id      = ntohs(p[0]);
	x.u       = ntohs(p[1]);
	x.qdcount = ntohs(p[2]);
	x.ancount = ntohs(p[3]);
	x.nscount = ntohs(p[4]);
	x.arcount = ntohs(p[5]);

	x.here = x.packet + 12;

	if (!x.ancount)
		return errno = EPROTO, -1;

	unsigned char *limit = x.packet + len;

	rr_t y;
	if (!(x.here = read_record(&x, &y, x.here, 1, limit)))
		return errno = EPROTO, -1;

	int j, res_len = 0;
	for (j = 0; j < x.ancount; ++j) {
		if (!(x.here = read_record(&x, &y, x.here, 0, limit)))
			return res_len ?: (errno = EPROTO, -1);

		if (y.type == 0x0001 && y.class == 0x0001 && y.len == 4)
			if (res_len < res_limit)
				resolved[res_len++] = ipv4_ntoi(y.data);
	}
	return res_len;
}


/* ------------------------------------------------------------------------ */
// return number of IP-address putted in array resolved[res_limit] in host bytes order
int dns_gethostsbyname(char const *iface, char const *resolv_conf, char const *host_name, uint32_t *resolved, size_t res_limit)
{
	if (!host_name || !resolved)
		return errno = EINVAL, -1;

	if (ipv4_isip(host_name))
		return !res_limit ? 0 : (resolved[0] = ipv4_atoi(host_name), 1);

	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
		return -1;

	in_addr_t dns_ips[MAX_DNS_NUM];
	int retval = -1;

#if 1
	int dns_num = dns_readResolvConf(resolv_conf, dns_ips, sizeof dns_ips / sizeof dns_ips[0]);
	if (dns_num < 1) {
		//printf("$> resolv\n");
		errno = ENETDOWN;
		goto _desock;
	}

	if (setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface) + 1) < 0)
		goto _desock;
#else
	int dns_num = 1;
	dns_ips[0] = 0x08080808;
#endif

	unsigned char send_buf[DNS_BUF_SIZE];
	int send_len = mk_dns_query(send_buf, sizeof send_buf, host_name);

	unsigned int start = uptime_getms(), finish = start + 2000;

	int send_count = SEND_DNS_REQ_NUM;
	while (send_count--) {
		int i;
		for (i = 0; i < dns_num; ++i) {
			struct sockaddr_in send_servaddr;
			send_servaddr.sin_family      = AF_INET;
			send_servaddr.sin_port        = htons(DNS_SRV_PORT);
			send_servaddr.sin_addr.s_addr = htonl(dns_ips[i]);

			int len;
			do {
				len = sendto(sd, send_buf, send_len, 0, (struct sockaddr *)&send_servaddr, sizeof(struct sockaddr_in));
			} while (len < 0 && errno == EINTR);

			if (len < 0) {
				//printf("$> sendto\n");
				continue;
			}

			int r = poll_sock_recv(sd, finish);
			if (r <= 0)
				goto _exit; // timeout or fail

			struct sockaddr_in rcv_servaddr;
			socklen_t socklen = sizeof(struct sockaddr_in);

			unsigned char recv_buf[DNS_BUF_SIZE];

			do {
				len = recvfrom(sd, recv_buf, DNS_BUF_SIZE, 0, (struct sockaddr *)&rcv_servaddr, &socklen);
			} while (len < 0 && errno == EINTR);

			if (len < 0 || (retval = parse_dns_response(recv_buf, len, resolved, res_limit)) >= 0)
				goto _exit;
		}
	}

	errno = ENOENT;
_exit:
_desock:
	if (sd > 0)
		close(sd);
	return retval;
}


/* ------------------------------------------------------------------------ */
// return number of IP-address putted in array resolved[res_limit] in host bytes order
int dns_gethostbyname(char const *iface, char const *resolv_conf, char const *host_name, uint32_t *resolved)
{
	return dns_gethostsbyname(iface, resolv_conf, host_name, resolved, 1);
}


/* ------------------------------------------------------------------------ */
// return number of IP-address putted in array resolved[res_limit] in host bytes order
int dns_resolve_domain(char const *iface, char const *resolv_conf, char const *host_name, uint32_t *resolved, size_t res_limit, int timeout)
{
	if (!host_name || !resolved)
		return errno = EINVAL, -1;

	if (ipv4_isip(host_name))
		return !res_limit ? 0 : (resolved[0] = ipv4_atoi(host_name), 1);

	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0)
		return -1;

	in_addr_t dns_ips[MAX_DNS_NUM];
	int retval = -1;

#if 1
	int dns_num = dns_readResolvConf(resolv_conf, dns_ips, sizeof dns_ips / sizeof dns_ips[0]);
	if (dns_num < 1) {
		//printf("$> resolv\n");
		errno = ENETDOWN;
		goto _desock;
	}

	if (setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface) + 1) < 0)
		goto _desock;
#else
	int dns_num = 2;
	dns_ips[0] = 0x08080808;
	dns_ips[1] = 0x4d580808;

	if (setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface) + 1) < 0)
		goto _desock;
#endif

	unsigned char send_buf[DNS_BUF_SIZE];
	int send_len = mk_dns_query(send_buf, sizeof send_buf, host_name);

	u_int32_t start = uptime_getms(), finish = start + (timeout ?: 10000);

	//syslog(LOG_NOTICE, "dns_resolve_domain: %s", host_name);

	int send_count = SEND_DNS_REQ_NUM;
	while (send_count--) {
		int i;
		for (i = 0; i < dns_num; ++i) {
			struct sockaddr_in send_servaddr;
			send_servaddr.sin_family      = AF_INET;
			send_servaddr.sin_port        = htons(DNS_SRV_PORT);
			send_servaddr.sin_addr.s_addr = htonl(dns_ips[i]);

			int len;
			do {
				len = sendto(sd, send_buf, send_len, 0, (struct sockaddr *)&send_servaddr, sizeof(struct sockaddr_in));
			} while (len < 0 && errno == EINTR);
			if (len < 0)
				goto _exit;
			//syslog(LOG_NOTICE, "dns_resolve_domain: sent to %s", ipv4_itoa(dns_ips[i]));
		}

		for (i = 0; i < dns_num; ++i) {
			int r = poll_sock_recv(sd, finish);
			if (r <= 0) {
				if (!r)
					errno = ETIME;
				goto _exit; // timeout or fail
			}

			struct sockaddr_in rcv_servaddr;
			socklen_t socklen = sizeof(struct sockaddr_in);

			unsigned char recv_buf[DNS_BUF_SIZE];
			int len;
			do {
				len = recvfrom(sd, recv_buf, DNS_BUF_SIZE, 0, (struct sockaddr *)&rcv_servaddr, &socklen);
			} while (len < 0 && errno == EINTR);

			if (len < 0 || (retval = parse_dns_response(recv_buf, len, resolved, res_limit)) >= 0) {
				//syslog(LOG_NOTICE, "dns_resolve_domain: received from %s", ipv4_itoa(ntohl(rcv_servaddr.sin_addr.s_addr)));
				goto _exit;
			}
		}
	}

	errno = ENOENT;
_exit:
_desock:;
	if (sd > 0)
		close(sd);
	return retval;
}
