
/* -------------------------------------------------------------------------- */
static const mqtt_prop_info_t mqtt_props[] = {
	[MQTT_PAYLOAD_FORMAT_INDICATOR] = PROP( 0x10008, BYTE,     payload_format_ind ),
	[MQTT_MSG_EXP_INTERVAL]         = PROP( 0x10008, DWORD,    msg_exp_interval ),
	[MQTT_CONTENT_TYPE]             = PROP( 0x10008, STRING,   content_type ),
	[MQTT_RESPONSE_TOPIC]           = PROP( 0x10008, STRING,   response_topic ),
	[MQTT_CORRELATION_DATA]         = PROP( 0x10008, BINARY,   corr_data ),
	[MQTT_SUBSCRIPTION_ID]          = PROP( 0x00108, VAR_NUM,  subscription_id ),
	[MQTT_SESSION_EXP_INTERVAL]     = PROP( 0x04006, DWORD,    session_exp_interval ),
	[MQTT_ASSIGNED_CLIENT_ID]       = PROP( 0x00004, STRING,   assigned_client_id ),
	[MQTT_SERVER_KEEP_ALIVE]        = PROP( 0x00004, WORD,     server_keep_alive ),
	[MQTT_AUTH_METHOD]              = PROP( 0x08006, STRING,   auth_method ),
	[MQTT_AUTH_DATA]                = PROP( 0x08006, BINARY,   auth_data ),
	[MQTT_REQ_PROBLEM_INFO]         = PROP( 0x00002, BYTE,     req_problem_info ),
	[MQTT_WILL_DELAY_INTERVAL]      = PROP( 0x10000, DWORD,    will_delay_interval ),
	[MQTT_REQ_RESP_INFO]            = PROP( 0x00002, BYTE,     req_resp_info ),
	[MQTT_RESP_INFO]                = PROP( 0x00004, STRING,   resp_info ),
	[MQTT_SERVER_REF]               = PROP( 0x04004, STRING,   server_ref ),
	[MQTT_REASON_STR]               = PROP( 0x0CAF4, STRING,   reason_str ),
	[MQTT_RECV_MAX]                 = PROP( 0x00006, WORD,     recv_max ),
	[MQTT_TOPIC_ALIAS_MAX]          = PROP( 0x00006, WORD,     topic_alias_max ),
	[MQTT_TOPIC_ALIAS]              = PROP( 0x00008, WORD,     topic_alias ),
	[MQTT_MAX_QOS]                  = PROP( 0x00004, BYTE,     max_qos ),
	[MQTT_RETAIN_AVAIL]             = PROP( 0x00004, BYTE,     retain_avail ),
	[MQTT_USER_PROPERTY]            = PROP( 0x1FFFF, STR_PAIR, user_prop ),
	[MQTT_MAX_PACKET_SIZE]          = PROP( 0x00006, DWORD,    max_packet_size ),
	[MQTT_WILDCARD_SUBSCR_AVAIL]    = PROP( 0x00004, BYTE,     wildcard_subscr_avail ),
	[MQTT_SUBSCR_ID_AVAIL]          = PROP( 0x00004, BYTE,     subscr_id_avail ),
	[MQTT_SHARED_SUBSCR_AVAIL]      = PROP( 0x00004, BYTE,     shared_subscr_avail )
};



/* -------------------------------------------------------------------------- */
unsigned mqtt_get_var_int(uint8_t const **pp, uint8_t *end)
{
	unsigned b = **pp;
	if (b < 128)
		return ++*pp, b;

	unsigned n = b & 127, bit = 7;
	uint8_t const *p = *pp + 1;
	for (;;) {
		if (p >= end)
			return NOT_COMPLETE;
		if (!(b = *p++))
			return MALFORMED; // zero byte -- Malformed Variable Byte Integer
		n |= (b & 127) << bit;
		if (!(b & 128))
			break;
		if ((bit += 7) > 21)
			return MALFORMED; // too big value -- Malformed Variable Byte Integer
	}
	*pp = p;
	return n;
}


/* -------------------------------------------------------------------------- */
unsigned mqtt_get_var_int_size(unsigned n)
{
	if (n < 16384)
		return n < 128     ? 1 : 2;

	if (n < 268435456)
		return n < 2097152 ? 3 : 4;

	return ~0;
}


/* -------------------------------------------------------------------------- */
unsigned mqtt_get_num_size(unsigned type, unsigned n)
{
	static unsigned ns[] = { [MQTT_BYTE] = 1, [MQTT_WORD] = 2, [MQTT_DWORD] = 4 };
	return type < MQTT_VAR_NUM ? ns[type] : mqtt_get_var_int_size(n);
}


/* -------------------------------------------------------------------------- */
int mqtt_rev_put_var_int(uint8_t *p, unsigned n)
{
	if (n < 16384) {
		if (n < 128) {
			p[-1] = n;
			return 1;
		}

		p[-1] = (n >> 7);
		p[-2] = (n     ) | 128;
		return 2;
	}

	if (n < 2097152) {
		p[-1] = (n >> 14);
		p[-2] = (n >>  7) | 128;
		p[-3] = (n      ) | 128;
		return 3;
	}

	p[-1] = (n >> 21);
	p[-2] = (n >> 14) | 128;
	p[-3] = (n >>  7) | 128;
	p[-4] = (n      ) | 128;
	return 4;
}

/* -------------------------------------------------------------------------- */
int mqtt_put_var_int(uint8_t *p, unsigned n)
{
	if (n < 16384) {
		if (n < 128) {
			p[0] = n;
			return 1;
		}

		p[0] = (n     ) | 128;
		p[1] = (n >> 7);
		return 2;
	}

	if (n < 2097152) {
		p[0] = (n      ) | 128;
		p[1] = (n >>  7) | 128;
		p[2] = (n >> 14);
		return 3;
	}

	p[0] = (n      ) | 128;
	p[1] = (n >>  7) | 128;
	p[2] = (n >> 14) | 128;
	p[3] = (n >> 21);
	return 4;
}


/* -------------------------------------------------------------------------- */
unsigned mqtt_get_str(mqtt_str_t *s, uint8_t **pp, uint8_t *end)
{
	uint8_t *p = *pp;

	unsigned length = p[1] * 256 + p[0];
	if (size < length + 2)
		return NOT_COMPLETE;

	*pp = (s->str = p + 2) + (s->length = length);
	return 0;
}


/* -------------------------------------------------------------------------- */
unsigned mqtt_get_bin(mqtt_bin_t *s, uint8_t **pp, uint8_t *end)
{
	uint8_t *p = *pp;

	unsigned length = p[1] * 256 + p[0];
	if (size < length + 2)
		return NOT_COMPLETE;

	*pp = (s->bin = p + 2) + (s->length = length);
	return 0;
}


/* -------------------------------------------------------------------------- */
unsigned mqtt_get_num(unsigned *val, unsigned type, uint8_t const *pp, uint8_t const *end)
{
	uint8_t *p = *pp;
	unsigned size = end - p;
	switch (type) {
	case MQTT_BYTE:
		if (size < 1)
			return NOT_COMPLETE;
		*value = *p++;
		break;

	case MQTT_WORD:
		if (size < 2)
			return NOT_COMPLETE;
		*value = p[0] * 256 + p[1];
		p += 2;
		break;

	case MQTT_DWORD:
		if (size < 4)
			return NOT_COMPLETE;
		*value = (((p[0] * 256 + p[1]) * 256 + p[2]) * 256 + p[3];
		p += 4;
		break;

	case MQTT_VAR_NUM:
		unsigned num = mqtt_get_var_int(&p, end);
		if (num >= FAILED)
			return num;
	}

	return *pp = p, 0;
}


/* -------------------------------------------------------------------------- */
int mqtt_unpack_props(unsigned pkt_type, mqtt_props_t *ps, uint8_t **pp, uint8_t *end)
{
	uint8_t *p = *pp;

	while (p < end) {
		unsigned id = mqtt_get_var_int(&p, end);
		if (id >= FAILED)
			return id;
		if (id >= countof(mqtt_props))
			return MALFORMED;

		mqtt_prop_t *info = mqtt_props + id;
		if (info->type <= MQTT_VAR_NUM) {
			unsigned res = mqtt_get_num(((unsigned *)(((uint8_t *)((void *)ps)) + info->offset)), type, &p, end);
			if (res >= FAILED)
				return res;
			ps->map |= 1 << id;
			continue;
		}
		switch (type) {
		case MQTT_STRING: {
				unsigned res = mqtt_get_str(((mqtt_str_t *)(((uint8_t *)((void *)ps)) + info->offset)), &p, end);
				if (res >= FAILED)
					return res;
				break;
			}
		case MQTT_BINARY: {
				unsigned res = mqtt_get_bin(((mqtt_bin_t *)(((uint8_t *)((void *)ps)) + info->offset)), &p, end);
				if (res >= FAILED)
					return res;
				break;
			}
		case MQTT_STR_PAIR: {
				mqtt_str_pair_t *pair = ((mqtt_str_pair_t *)(((uint8_t *)((void *)ps)) + info->offset))
				unsigned res = mqtt_get_str((&pair->name, &p, end);
				if (res >= FAILED)
					return res;
				res = mqtt_get_str((&pair->value, &p, end);
				if (res >= FAILED)
					return res;
			}
		}
		ps->map |= 1 << id;
	}
}


/* -------------------------------------------------------------------------- */
int mqtt_get_num_prop_size(unsigned prop, unsigned value)
{
	return mqtt_get_var_int_size(prop) + mqtt_get_num_size(mqtt_props[prop].type, value);
}

/* -------------------------------------------------------------------------- */
int mqtt_put_num_prop(uint8_t *ptr, unsigned prop, unsigned value)
{
	if (prop >= countof(mqtt_props))
		return -1;

	unsigned type = mqtt_props[prop].type;

	if (type > MQTT_VAR_NUM)
		return -1;

	if (type == MQTT_VAR_NUM)
		if (value >= 268435456)
			return -1;

	uint8_t *p = ptr + mqtt_put_var_int(p, prop);

	switch (type) {
	case MQTT_BYTE:
		p[0] = value;
		return 1;
	case MQTT_WORD:
		p[0] = value >> 8;
		p[1] = value;
		return 2;
	case MQTT_DWORD:
		p[0] = value >> 24;
		p[1] = value >> 16;
		p[2] = value >> 8;
		p[3] = value;
		return 4;
	}
	return p - ptr + mqtt_put_var_int(p, value);
}


/* -------------------------------------------------------------------------- */
int mqtt_get_str_prop_size(unsigned prop, unsigned length)
{
	return mqtt_get_var_int_size(prop) + 2 + length;
}

/* -------------------------------------------------------------------------- */
int mqtt_put_str_prop(uint8_t **pp, unsigned prop, mqtt_cstr *ms)
{
	if (prop >= countof(mqtt_props))
		return M
		ALFORMED;

	unsigned type = mqtt_props[prop].type;
	if (type != MQTT_STRING)
		return -1;

	uint8_t *p = *pp + mqtt_put_var_int(p, prop);

	p[0] = ms->length >> 8;
	p[1] = ms->length;
	p += 2;
	memcpy(p, ms->str, ms->length);

	return *pp = p + ms->length, 0;
}


/* -------------------------------------------------------------------------- */
static int mqtt_put_bin_prop(uint8_t **pp, unsigned prop, uint8_t *bin, unsigned size)
{
	if (prop >= countof(mqtt_props))
		return MALFORMED;

	unsigned type = mqtt_props[prop].type;
	if (type != MQTT_BINARY)
		return -1;

	uint8_t *p = *pp + mqtt_put_var_int(p, prop);

	p[0] = ms->length >> 8;
	p[1] = ms->length;
	p += 2;
	memcpy(p, ms->bin, ms->length);

	return *pp = p + ms->length, 0;
}


/* -------------------------------------------------------------------------- */
mqtt_str_t *mqtt_str(mqtt_str_t *ms, char *s)
{
	ms->length = strlen(s);
	ms->str = s;
	return ms;
}


/* -------------------------------------------------------------------------- */
mqtt_cstr_t *mqtt_cstr(mqtt_cstr_t *ms, char const *s)
{
	ms->length = strlen(s);
	ms->str = s;
	return ms;
}

