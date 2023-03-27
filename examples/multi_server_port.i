
typedef
struct port_con {
	io_buf_sock_t bs;
	int state;

	char *end;
	char const *header, *body;

	char req_hdr[8000];
} port_con_t;


/* -------------------------------------------------------------------------- */
static int port_con_process_request(port_con_t *c)
{
	debug("%s <%s> process request", c->bs.bd.d.vmt->name, io_sock_stoa(&c->bs.conf));

	io_buf_sock_writef(&c->bs, "back off\r\n");
	c->state = ST_CLOSE;
	return 0;
}


/* -------------------------------------------------------------------------- */
static int v_port_con_all_sent(io_d_t *iod)
{
	port_con_t *c = (port_con_t *)iod;
	debug("%s <%s> all_sent", iod->vmt->name, io_sock_stoa(&c->bs.conf));
	io_d_free(iod); // end of connection
	debug("%s <%s> .. close", iod->vmt->name, io_sock_stoa(&c->bs.conf));
	return 0;
}


/* -------------------------------------------------------------------------- */
static int v_port_con_recv(io_d_t *iod)
{
	port_con_t *c = (port_con_t *)iod;
	//debug("<%s> recv [%d]", io_sock_stoa(&c->bs.conf), c->state);
	if (c->state == ST_RECV_HEADER) {
		size_t to_recv = (unsigned)(sizeof c->req_hdr - 1 - (c->end - c->req_hdr));
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
		char const *s = strstr(c->end, "\n");
		c->end += len;
		if (!s)
			return 0;

		c->body = s + 1;
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
	io_buf_sock_t bs;
	if (!io_stream_accept(&bs, self, &port_con_vmt)) {
		error("%s <%s> failed to accept: %m", self->d.vmt->name, io_sock_stoa(&self->conf));
		return -1;
	}

	char const *sid = io_sock_stoa(&self->conf);
	debug("%s <%s> accept: '%s'", self->d.vmt->name, sid, io_sock_stoa(&bs.conf));

	port_con_t *h = (port_con_t *)calloc(1, sizeof *h);
	memcpy(&h->bs, &bs, sizeof bs);
	h->end = h->req_hdr;
	return 0;
}
