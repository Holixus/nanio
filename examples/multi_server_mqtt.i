
enum mqtt_state {
	ST_RECV_PACKET,
	ST_DISCONNECT
};

enum {
	CONNECT,     // Connection request
	CONNACK,     // Connect acknowledgment
	PUBLISH,     // Publish message
	PUBACK,      // Publish acknowledgment (QoS 1)
	PUBREC,      // Publish received (QoS 2 delivery part 1)
	PUBREL,      // Publish release (QoS 2 delivery part 2)
	PUBCOMP,     // Publish complete (QoS 2 delivery part 3)
	SUBSCRIBE,   // Subscribe request
	SUBACK,      // Subscribe acknowledgment
	UNSUBSCRIBE, // Unsubscribe request
	UNSUBACK,    // Unsubscribe acknowledgment
	PINGREQ,     // PING request
	PINGRESP,    // PING response
	DISCONNECT,  // Disconnect notification
	AUTH         // Authentication exchange
} mqtt_packet_type_t;


/* -------------------------------------------------------------------------- */
static unsigned mqtt_get_var_int(uint8_t const **pp)
{
	unsigned n = 0, bit = 0;
	uint8_t const *p = *pp;
	for (;;) {
		unsigned b = *p++;
		n |= (b & 127) << bit;
		if (!(b & 128))
			break;
		if ((bit += 7) > 21)
			return -1; // Malformed Variable Byte Integer
	}
	*pp = p;
	return n;
}


/* -------------------------------------------------------------------------- */
static int mqtt_put_var_int(uint8_t **pp, unsigned n)
{
	if (n < 128) {
		*(*pp)++ = n;
		return 0;
	}

	if (n >= 268435456)
		return -1; // too big value

	uint8_t *p = *pp;
	do {
		int b = n & 127;
		if (n >>= 7)
			b |= 128;
		*p++ = b;
	} while (n);

	return *pp = p, 0;
}


/* -------------------------------------------------------------------------- */
typedef
struct mqtt_pkt {
	int type;
	int flags;
	unsigned length;
	uint8_t *payload;
} mqtt_pkt_t;


/* -------------------------------------------------------------------------- */
typedef
struct mqtt_con {
	io_stream_t s;
	int state;

	uint8_t *end;
	mqtt_pkt_t pkt;
	uint8_t *next;

	uint8_t buf[8000];
} mqtt_con_t;



/* -------------------------------------------------------------------------- */
static int mqtt_response(mqtt_con_t *c, unsigned type, unsigned flags, unsigned length, uint8_t *payload)
{
	uint8_t pkt[5], *p = pkt;

	*p++ = ((type << 4) & 240) | (flags & 15);
	if (mqtt_put_var_int(&p, length) < 0) {
		io_stream_debug(&c->s, "too big response length %d", length);
		return -1;
	}

	io_stream_write(&c->s, pkt, (unsigned)(p - pkt));
	if (length)
		io_stream_write(&c->s, payload, length);

	//io_stream_debug(&c->s, "response");
	return 0;
}


/* -------------------------------------------------------------------------- */
static int mqtt_unpack(mqtt_con_t *c)
{
	return 0;
}


/* -------------------------------------------------------------------------- */
static int mqtt_on_packet(mqtt_con_t *c)
{
	return 0;
}


/* -------------------------------------------------------------------------- */
static int v_mqtt_con_close(io_d_t *iod)
{
	mqtt_con_t *c = (mqtt_con_t *)iod;
	io_stream_debug(&c->s, "close");
	return 0;
}

/* -------------------------------------------------------------------------- */
static int v_mqtt_con_all_sent(io_d_t *iod)
{
	mqtt_con_t *c = (mqtt_con_t *)iod;
	if (c->state == ST_DISCONNECT)
		io_stream_close(&c->s); // end of connection
	return 0;
}

/* -------------------------------------------------------------------------- */
static int v_mqtt_con_recv(io_d_t *iod)
{
	mqtt_con_t *c = (mqtt_con_t *)iod;
	//io_stream_debug(&c->s, "recv [%d]", c->state);
	if (c->state == ST_RECV_PACKET) {
		size_t to_recv = (unsigned)(sizeof c->buf - 1 - (c->end - c->buf));
		ssize_t len = io_stream_recv(&c->s, c->end, to_recv);
		if (len < 0) {
			io_stream_error(&c->s, "recv failed");
			return -1;
		}
		if (!len) {
			io_stream_close(&c->s); // end of connection
			return 0;
		}

		c->end += len;

		if (mqtt_unpack(c))
			mqtt_on_packet(c);
		return 0;
	}
	return 0;
}



/* -------------------------------------------------------------------------- */
io_vmt_t mqtt_con_vmt = {
	.name = "mqtt_con",
	.ancestor = &io_stream_vmt,
	.u.stream = {
		.close    = v_mqtt_con_close,
		.recv     = v_mqtt_con_recv,
		.all_sent = v_mqtt_con_all_sent
	}
};


/* -------------------------------------------------------------------------- */
static int mqtt_accept(io_stream_listen_t *self)
{
	mqtt_con_t *c = (mqtt_con_t *)calloc(1, sizeof *c);
	if (!io_stream_accept(&c->s, self, &mqtt_con_vmt)) {
		io_stream_error(&c->s, "failed to accept");
		free(c);
		return -1;
	}

	c->end = c->buf;

	io_stream_debug(&c->s, "accept: '%s'", io_sock_stoa(&c->s.sa));
	return 0;
}
