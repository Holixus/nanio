
enum ftp_state {
	FTP_RECV_CMD,
	FTP_CLOSE
};


typedef
struct port_con {
	io_buf_sock_t bs;
	int state;

	char *end, *next;

	char cmd[8000];
} port_con_t;


/* -------------------------------------------------------------------------- */
static int port_con_process_request(port_con_t *c)
{

	int req_len = c->next - c->cmd;

	c->next[-2] = 0;

	debug("%s <%s> process command '%s'", c->bs.bd.d.vmt->name, io_sock_stoa(&c->bs.conf), c->cmd);

	if (!strcasecmp(c->cmd, "QUIT")) {
		io_buf_sock_writef(&c->bs, "221 Goodbye.\r\n");
		c->state = FTP_CLOSE;
	} else {
		io_buf_sock_writef(&c->bs, "200-%s\r\n200 OK.\r\n", c->cmd);
	}

	if (c->next < c->end) {
		memmove(c->cmd, c->next, (unsigned)(c->end - c->next));
		c->end -= req_len;
	} else {
		c->end = c->cmd;
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
static int v_port_con_all_sent(io_d_t *iod)
{
	port_con_t *c = (port_con_t *)iod;
	debug("%s <%s> all_sent", iod->vmt->name, io_sock_stoa(&c->bs.conf));
	if (c->state == FTP_CLOSE) {
		io_d_free(iod); // end of connection
		debug("%s <%s> .. close", iod->vmt->name, io_sock_stoa(&c->bs.conf));
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
static int v_port_con_recv(io_d_t *iod)
{
	port_con_t *c = (port_con_t *)iod;
	//debug("<%s> recv [%d]", io_sock_stoa(&c->bs.conf), c->state);
	if (c->state == FTP_RECV_CMD) {
		size_t to_recv = (unsigned)(sizeof c->cmd - 1 - (c->end - c->cmd));
		ssize_t len = io_buf_sock_recv(&c->bs, c->end, to_recv);
		if (len < 0) {
			error("%s <%s> recv failed: %m", iod->vmt->name, io_sock_stoa(&c->bs.conf));
			return -1;
		}
		if (!len) {
			io_buf_sock_free(&c->bs); // end of connection
			debug("%s <%s> closed", iod->vmt->name, io_sock_stoa(&c->bs.conf));
			return 0;
		}
		c->end[len] = 0;
		char *s = strstr(c->end, "\r\n");
		c->end += len;
		if (!s)
			return 0;

		c->next = s + 2;
		port_con_process_request(c);
		return 0;
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
io_vmt_t port_con_vmt = {
	.name = "port_con",
	.ancestor = &io_stream_vmt,
	.u.stream = {
		.close    = NULL,
		.recv     = v_port_con_recv,
		.all_sent = v_port_con_all_sent
	}
};



/* -------------------------------------------------------------------------- */
static int port_accept(io_stream_listen_t *self)
{
	port_con_t *h = (port_con_t *)calloc(1, sizeof *h);
	if (!io_stream_accept(&h->bs, self, &port_con_vmt)) {
		error("%s <%s> failed to accept", self->d.vmt->name, io_sock_stoa(&self->conf));
		free(h);
		return -1;
	}

	io_buf_sock_writef(&h->bs, "220 welcome to my FTP server\r\n");

	h->end = h->cmd;

	char const *sid = io_sock_stoa(&self->conf);
	debug("%s <%s> accept: '%s'", h->bs.bd.d.vmt->name, sid, io_sock_stoa(&h->bs.conf));

	return 0;
}
