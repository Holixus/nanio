

/* -------------------------------------------------------------------------- */
enum mqtt_state {
	ST_RECV_HEADER,
	ST_RECV_PACKET,
	ST_DISCONNECT
};


/* -------------------------------------------------------------------------- */
enum {
	PKT_FIXED_HEADER,
	PKT_VAR_HEADER_ID,
	PKT_VAR_HEADER_PROPS,
	PKT_PAYLOAD,
	PKT_DONE
}


/* -------------------------------------------------------------------------- */
typedef
struct mqtt_pkt {
	int bad;
	int type;
	unsigned length;
	unsigned id; // packet identifier

	mqtt_props_t props;

	uint8_t *data, *end;

	uint8_t *payload;

	unsigned phase;

} mqtt_pkt_t;


/* -------------------------------------------------------------------------- */
typedef
struct mqtt_con {
	io_stream_t s;
	int state;

	uint8_t *end;
	uint8_t *next;

	uint8_t buf[256];

	mqtt_pkt_t pkt;
	mqtt_state_t mqtt_state;

	mqtt_str_t server_ref;
	unsigned max_pkt_size;
} mqtt_con_t;



/* -------------------------------------------------------------------------- */
static int mqtt_response(mqtt_con_t *c, unsigned type, unsigned flags, unsigned length, uint8_t *payload)
{
	uint8_t pkt[5], *p = pkt;

	if (length >= OVER_VAR_INT) {
		io_stream_debug(&c->s, "too big response length %d", length);
		return -1;
	}

	*p++ = ((type << 4) & 240) | (flags & 15);
	p += mqtt_put_var_int(p, length);

	io_stream_write(&c->s, pkt, (unsigned)(p - pkt));
	if (length)
		io_stream_write(&c->s, payload, length);

	//io_stream_debug(&c->s, "response");
	return 0;
}


/* -------------------------------------------------------------------------- */
#define MQTT_STR(s)                     { .length = sizeof s - 1, .str = s }
#define MQTT_REASON(code, types, str)   { MQTT_##code, types, MQTT_STR(str) }

typedef
struct mqtt_reason {
	unsigned code;
	unsigned types;
	mqtt_cstr_t str;
} mqtt_reason_t;

static const mqtt_reason_t mqtt_reasons[] = {
	MQTT_REASON( NORMAL,                        0x4000, "Normal disconnection" ),         // DISCONNECT
	MQTT_REASON( SUCCESS,                       0x8016, "Success" ),                      // CONNACK, PUBACK
	MQTT_REASON( GRANTED_QOS_0,                 0x0200, "Granted QoS 0" ),                // SUBACK
	MQTT_REASON( GRANTED_QOS_1,                 0x0200, "Granted QoS 1" ),                // SUBACK
	MQTT_REASON( GRANTED_QOS_2,                 0x0200, "Granted QoS 2" ),                // SUBACK
	MQTT_REASON( NO_MATCHING_SUBSCRBRS,         0x0010, "No matching subscribers" ),
	MQTT_REASON( CONTINUE_AUTH,                 0x8000, "Continue authentication" ),
//	MQTT_REASON( RE_AUTH,                       0x8000, "Re-authenticate" ),
//	MQTT_REASON( WILL_MESSAGE,                  0x0000, "Disconnect with Will Message" ),
	MQTT_REASON( UNSPECIFIED_ERROR,             0xFFFF, "Unspecified error" ),
	MQTT_REASON( MALFORMED_PACKET,              0xFFFF, "Malformed Packet" ),
	MQTT_REASON( PROTOCOL_ERROR,                0xFFFF, "Protocol Error" ),
	MQTT_REASON( IMPL_SPEC_ERROR,               0xFFFF, "Implementation specific error" ),
	MQTT_REASON( UNSUP_PROTO_VER,               0xFFFF, "Unsupported Protocol Version" ),
	MQTT_REASON( CLID_NOT_VALID,                0xFFFF, "Client Identifier not valid" ),
	MQTT_REASON( BAD_USER_OR_PASS,              0xFFFF, "Bad User Name or Password" ),
	MQTT_REASON( NOT_AUTHORIZED,                0xFFFF, "Not authorized" ),
	MQTT_REASON( SERVER_NOT_AVAIL,              0xFFFF, "Server unavailable" ),
	MQTT_REASON( SERVER_BUSY,                   0xFFFF, "Server busy" ),
	MQTT_REASON( BANNED,                        0xFFFF, "Banned" ),
	MQTT_REASON( BAD_AUTH_METHOD,               0xFFFF, "Bad authentication method" ),
	MQTT_REASON( SHUTTING_DOWN,                 0xFFFF, "Server shutting down" ),
	MQTT_REASON( KEEP_ALIVE_TIMEOUT,            0xFFFF, "Keep Alive timeout" ),
	MQTT_REASON( SESSION_TAKEN_OVER,            0xFFFF, "Session taken over" ),
	MQTT_REASON( TOPIC_FILTER_INVALID,          0xFFFF, "Topic Filter invalid" ),
	MQTT_REASON( TOPIC_NAME_INVALID,            0xFFFF, "Topic Name invalid" ),
	MQTT_REASON( PACKET_ID_IN_USE,              0xFFFF, "Packet identifier in use" ),
	MQTT_REASON( PACKET_ID_NOT_FOUND,           0xFFFF, "Packet identifier not found" ),
	MQTT_REASON( RECV_MAX_EXCEEDED,             0xFFFF, "Receive Maximum exceeded" ),
	MQTT_REASON( TOPIC_ALIAS_INVALID,           0xFFFF, "Topic Alias invalid" ),
	MQTT_REASON( PACKET_TOO_LARGE,              0xFFFF, "Packet too large" ),
	MQTT_REASON( MSG_RATE_TOO_HIGH,             0xFFFF, "Message rate too high" ),
	MQTT_REASON( QUOTA_EXCEEDED,                0xFFFF, "Quota exceeded" ),
	MQTT_REASON( ADMIN_ACTION,                  0xFFFF, "Administrative action" ),
	MQTT_REASON( PAYLOAD_FORMAT_INVALID,        0xFFFF, "Payload format invalid" ),
	MQTT_REASON( RETAIN_NOT_SUPPORTED,          0xFFFF, "Retain not supported" ),
	MQTT_REASON( QOS_NOT_SUPPORTED,             0xFFFF, "QoS not supported" ),
	MQTT_REASON( USE_ANOTHER_SERVER,            0xFFFF, "Use another server" ),
	MQTT_REASON( SERVER_MOVED,                  0xFFFF, "Server moved" ),
	MQTT_REASON( SHARED_SURSCR_NOT_SUPPORTED,   0xFFFF, "Shared Subscriptions not supported" ),
	MQTT_REASON( CONN_RATE_EXCEEDED,            0xFFFF, "Connection rate exceeded" ),
	MQTT_REASON( MAX_CONNECT_TIME,              0xFFFF, "Maximum connect time" ),
	MQTT_REASON( SUBSCR_ID_NOT_SUPPORTED,       0xFFFF, "Subscription Identifiers not supported" ),
	MQTT_REASON( WILDCARD_SUBSCR_NOT_SUPPORTED, 0xFFFF, "Wildcard Subscriptions not supported" )
};

/* -------------------------------------------------------------------------- */
static int mqtt_disconnect(mqtt_con_t *c, unsigned reason)
{
	uint8_t buf[256], *pkt = buf + 20, *p = pkt, *end = buf + sizeof buf;

	/*
		byte     type + flags
		var_int  packet length

		byte     reason
		var_int  properties length

		properties
		  str Reason String    // optional (if packet size < max_packet_size)
		  str Server Reference // from CONNECT

		no payload
	*/

	unsigned reason_str_prop_size = mqtt_size(mqtt_get_str_prop_size(MQTT_REASON_STR, mqtt_reasons[reason].length);

	unsigned sr_size = c->server_ref.length ? mqtt_get_str_prop_size(MQTT_SERVER_REF, c->server_ref.length) : 0;

	if (c->max_pkt_size) {
		unsigned pkt_size = 1 + mqtt_size(1 + mqtt_size(sr_size + reason_str_prop_size));
		if (pkt_size < c->max_pkt_size)
			mqtt_put_str_prop(&p, MQTT_REASON_STR, mqtt_reasons + reason);
	}

	if (c->server_ref.length)
		mqtt_put_str_prop(&p, MQTT_SERVER_REF, &c->server_ref);

	pkt -= mqtt_rev_put_var_int(pkt, (unsigned)(p - pkt));
	*--pkt = reason;

	pkt -= mqtt_rev_put_var_int(pkt, (unsigned)(p - pkt));
	*--pkt = DISCONNECT;

	io_stream_write(&c->s, pkt, (unsigned)(p - pkt));
	//io_stream_debug(&c->s, "response");
	c->mqtt_state = MQTT_DISCONNECT;
	return 0;
}


/* -------------------------------------------------------------------------- */
static int mqtt_recv_header(mqtt_con_t *c)
{
	uint8_t *p = c->next;

	mqtt_pkt_t *pkt = &s->pkt;
	pkt->type = *p++;

	unsigned length = mqtt_get_var_int(&p, c->end);
	if (length == MALFORMED) {
		pkt->bad = 1; // broken packet
		return -1;
	} else if (length == NOT_COMPLETE)
		return 0; // wait for next part of packet

	pkt->length = length;

	pkt->end = pkt->data = (uint8_t *)malloc(length);
	c->next = p;
	if (c->end > p) {
		unsigned tail = (unsigned)(c->end - p);
		if (tail > length) {
			memcpy(pkt->end, p, length);
			c->next = p + length;
			memmove(c->buf, c->next, (unsigned)(tail -= length));
			c->end -= tail;
			c->next = c->buf; // removed packed from buffer
			c->state = ST_PACKET;
		} else {
			memcpy(pkt->end, p, tail);
			pkt->end += tail;
			c->state = ST_RECV_PACKET;
			c->next = c->end = c->buf; // reset buf
		}
	}

	return 0;
}


/* -------------------------------------------------------------------------- */
int mqtt_parse_packet(mqtt_pkt_t *pkt)
{
	unsigned type = pkt->type;

	uint8_t *p = pkt->data;
	// Variable Header

	switch (type) {
// packets from client
	case CONNECT:     // Connection request
		// 0 string Protocol Name
		// 6 byte   Protocol Version
		// 7 byte   Connect Flags
		// 8 word   Keep Alive
		if (memcmp(p, "\000\004MQTT", 6))
			return -1; // malformed

		if (p[6] != 5 && p[6] != 3) {
			mqtt_conack(
		}
		break;

	case SUBSCRIBE:   // Subscribe request
	case UNSUBSCRIBE: // Unsubscribe request
	case PINGREQ:     // PING request
	case PINGRESP:    // PING response
	case DISCONNECT:  // Disconnect notification
	case AUTH:        // Authentication exchange

// packets from server or client
	case PUBLISH:     // Publish message
	case PUBACK:      // Publish acknowledgment (QoS 1)

	case PUBREC:      // Publish received (QoS 2 delivery part 1)
	case PUBREL:      // Publish release (QoS 2 delivery part 2)
	case PUBCOMP:     // Publish complete (QoS 2 delivery part 3)
	}
}


/* -------------------------------------------------------------------------- */
static int mqtt_on_packet(mqtt_con_t *c)
{
	mqtt_pkt_t *pkt = &c->pkt;

	if (c->mqtt_state == MQTT_NEW) {
		if (pkt->type != CONNECT) {
			
		}
	}

	switch (pkt->type) {
// packets from client
	case SUBSCRIBE:   // Subscribe request
	case UNSUBSCRIBE: // Unsubscribe request
	case PINGREQ:     // PING request
	case PINGRESP:    // PING response
	case DISCONNECT:  // Disconnect notification
	case AUTH:        // Authentication exchange

// packets from server or client
	case PUBLISH:     // Publish message
	case PUBACK:      // Publish acknowledgment (QoS 1)

	case PUBREC:      // Publish received (QoS 2 delivery part 1)
	case PUBREL:      // Publish release (QoS 2 delivery part 2)
	case PUBCOMP:     // Publish complete (QoS 2 delivery part 3)

// packets from server
//	case CONNACK:     // Connect acknowledgment
//	case SUBACK:      // Subscribe acknowledgment
//	case UNSUBACK:    // Unsubscribe acknowledgment
	}
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
	if (c->state == ST_RECV_HEADER) {
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

		if (mqtt_recv_header(c) < 0) {
			io_stream_error(&c->s, "bad mqtt header");
			return -1;
		}
	} else if (c->state == ST_RECV_PACKET) {
		mqtt_pkt_t *pkt = &c->pkt;
		size_t to_recv = (unsigned)(pkt->length - (pkt->end - pkt->data));
		ssize_t len = io_stream_recv(&c->s, pkt->end, to_recv);
		if (len < 0) {
			io_stream_error(&c->s, "recv failed");
			return -1;
		}
		if (!len) {
			io_stream_close(&c->s); // end of connection
			return 0;
		}
		pkt->end += len;
		if ((unsigned)(pkt->end - pkt->data) == pkt->length)
			c->state = ST_PACKET;
	}

	if (c->state == ST_PACKET) {
		if (mqtt_parse_packet(c) < 0) {
			io_stream_error(&c->s, "malformed packet");
			return -1;
		}
		if (mqtt_on_packet(c) < 0)
			return -1;
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

	c->end = c->next = c->buf;

	io_stream_debug(&c->s, "accept: '%s'", io_sock_stoa(&c->s.sa));
	return 0;
}
