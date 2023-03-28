
enum ftp_state {
	FTP_RECV_CMD,
	FTP_CLOSE
};


typedef
struct port_con {
	io_stream_t s;
	int state;

	char *end, *next;

	char cmd[8000];
} port_con_t;


/* -------------------------------------------------------------------------- */
static int port_con_process_request(port_con_t *c)
{

	int req_len = c->next - c->cmd;

	c->next[-2] = 0;

	io_stream_debug(&c->s, "process command '%s'", c->cmd);

	if (!strcasecmp(c->cmd, "QUIT")) {
		io_stream_writef(&c->s, "221 Goodbye.\r\n");
		c->state = FTP_CLOSE;
	} else {
		io_stream_writef(&c->s, "200-%s\r\n200 OK.\r\n", c->cmd);
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
static int v_port_con_close(io_d_t *iod)
{
	port_con_t *c = (port_con_t *)iod;
	io_stream_debug(&c->s, "close");
	return 0;
}


/* -------------------------------------------------------------------------- */
static int v_port_con_all_sent(io_d_t *iod)
{
	port_con_t *c = (port_con_t *)iod;
	//io_stream_debug(&c->s, "all_sent");
	if (c->state == FTP_CLOSE) {
		io_stream_close(&c->s); // end of connection
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
static int v_port_con_recv(io_d_t *iod)
{
	port_con_t *c = (port_con_t *)iod;
	//io_stream_debug(&c->s, "recv [%d]", c->state);
	if (c->state == FTP_RECV_CMD) {
		size_t to_recv = (unsigned)(sizeof c->cmd - 1 - (c->end - c->cmd));
		ssize_t len = io_stream_recv(&c->s, c->end, to_recv);
		if (len < 0) {
			io_stream_error(&c->s, "recv failed: %m");
			return -1;
		}
		if (!len) {
			io_stream_debug(&c->s, "closed");
			io_stream_close(&c->s); // end of connection
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
		.close    = v_port_con_close,
		.recv     = v_port_con_recv,
		.all_sent = v_port_con_all_sent
	}
};



/* -------------------------------------------------------------------------- */
static int port_accept(io_stream_listen_t *self)
{
	port_con_t *c = (port_con_t *)calloc(1, sizeof *c);
	if (!io_stream_accept(&c->s, self, &port_con_vmt)) {
		io_stream_error(&c->s, "failed to accept");
		free(c);
		return -1;
	}

	io_stream_writef(&c->s, "220 welcome to my FTP server\r\n");

	c->end = c->cmd;

	io_stream_debug(&c->s, "accept: '%s'", io_sock_stoa(&c->s.sa));
	return 0;
}
