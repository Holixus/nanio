
enum http_state {
	ST_RECV_HEADER,
	ST_RECV_BODY,
	ST_CLOSE
};


typedef
struct http_con {
	io_stream_t s;
	int state;

	char *end;
	char const *header, *body;

	io_http_req_t req;
	io_hmap_t *params;

	char req_hdr[8000];
} http_con_t;



/* -------------------------------------------------------------------------- */
static void http_end(http_con_t *c)
{
	char const *conn_type = io_hmap_getc(c->params, "Connection");
	int keep_alive = conn_type && strcmp(conn_type, "keep-alive") ? 1 : 0;

	c->state = keep_alive ? ST_RECV_HEADER : ST_CLOSE;
	if (keep_alive) {
		c->end = c->req_hdr;
		c->header = c->body = NULL;
	}
}


/* -------------------------------------------------------------------------- */
static int http_empty_response(http_con_t *c, int status)
{
	io_stream_writef(&c->s, "\
HTTP/%d.%d %03d %s\r\n\
Content-Length: 0\r\n\
\r\n", c->req.version >> 8, c->req.version & 255, status, io_http_resp_status(status));

	http_end(c);
	io_stream_debug(&(c->s), "response %d", status);
	return 0;
}



/* -------------------------------------------------------------------------- */
static int http_text_response(http_con_t *c, int status, char const *text)
{
	size_t len = strlen(text);
	io_stream_writef(&c->s, "\
HTTP/%d.%d %03d %s\r\n\
Content-Length: %lu\r\n\
Content-Type: text/html; charset=UTF-8\r\n\
\r\n", c->req.version >> 8, c->req.version & 255, status, io_http_resp_status(status), len);

	io_stream_write(&c->s, text, len);

	http_end(c);

	io_stream_debug(&c->s, "response %d", status);
	return 0;
}


/* -------------------------------------------------------------------------- */
static int http_con_process_request(http_con_t *c)
{
	switch (c->req.method) {
	case HTTP_PUT:
		c->state = ST_RECV_BODY;
	default:;
	}

	io_stream_debug(&c->s, "%s %s HTTP/%d.%d", io_http_req_method(c->req.method), c->req.path, c->req.version >> 8, c->req.version & 255);
	if (!strcmp(c->req.path, "/favicon.ico")) {
		http_empty_response(c, HTTP_PAGE_NOT_FOUND);
		return 0;
	}
	http_text_response(c, HTTP_DATA_FOLLOWS, "\
<html>\n\
<head><title>Hello!</title></head>\n\
<body>Hello!</body>\n\
</html>\n\
");
	return 0;
}


/* -------------------------------------------------------------------------- */
static int v_http_con_close(io_d_t *iod)
{
	http_con_t *c = (http_con_t *)iod;
	io_stream_debug(&c->s, "real close (free params)");
	if (c->params)
		free(c->params);
	return 0;
}


/* -------------------------------------------------------------------------- */
static void v_http_con_all_sent(io_d_t *iod)
{
	http_con_t *c = (http_con_t *)iod;
	if (c->state == ST_CLOSE)
		io_d_free(iod); // end of connection
}


/* -------------------------------------------------------------------------- */
static int v_http_con_recv(io_d_t *iod)
{
	http_con_t *c = (http_con_t *)iod;
	//io_stream_debug(&c->s, "recv [%d]", c->state);
	if (c->state == ST_RECV_HEADER) {
		size_t to_recv = (unsigned)(sizeof c->req_hdr - 1 - (unsigned)(c->end - c->req_hdr));
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
		char *hdr_end = strstr(c->end, "\r\n\r\n");
		c->end += len;
		if (!hdr_end)
			return 0;

		c->body = hdr_end + 4;

		char const *s = c->req_hdr;

		int parse_result = io_http_req_parse(&c->req, &s);
		if (parse_result < 0) {
			io_stream_debug(&c->s, "bad request %d", parse_result);
			return http_empty_response(c, HTTP_BAD_REQUEST);
		}

		c->header = s;
		c->params = io_hmap_create(c->params, 48, 8192);

		parse_result = io_http_header_parse(c->params, &s);
		if (parse_result < 0) {
			io_stream_debug(&c->s, "bad header %d", parse_result);
			return http_empty_response(c, HTTP_BAD_REQUEST);
		}

		http_con_process_request(c);
		return 0;
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
io_vmt_t http_con_vmt = {
	.name = "http_con",
	.ancestor = &io_stream_vmt,
	.u.stream = {
		.close    = v_http_con_close,
		.recv     = v_http_con_recv,
		.all_sent = v_http_con_all_sent
	}
};


/* -------------------------------------------------------------------------- */
static int http_accept(io_stream_listen_t *self)
{
	http_con_t *c = (http_con_t *)calloc(1, sizeof *c);
	if (!io_stream_accept(&c->s, self, &http_con_vmt)) {
		io_stream_error(self, "failed to accept");
		free(c);
		return -1;
	}

	c->end = c->req_hdr;

	io_stream_debug(&c->s, "accept: '%s'", io_sock_stoa(&c->s.sa));
	return 0;
}
