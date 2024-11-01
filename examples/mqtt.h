
typedef
enum {
	MQTT_NEW,        // and keep alive
	MQTT_CONNECTED,
	MQTT_DISCONNECT
} mqtt_state_t;

enum {
	MQTT_NORMAL = 0,
	MQTT_SUCCESS = 0,
	MQTT_GRANTED_QOS_0 = 0,
	MQTT_GRANTED_QOS_1 = 1,
	MQTT_GRANTED_QOS_2 = 2,
	MQTT_WILL_MESSAGE = 4,
	MQTT_NO_MATCHING_SUBSCRBRS = 16,
	MQTT_CONTINUE_AUTH = 24,
	MQTT_RE_AUTH = 25,
	MQTT_UNSPECIFIED_ERROR = 128,
	MQTT_MALFORMED_PACKET = 129,
	MQTT_PROTOCOL_ERROR = 130,
	MQTT_IMPL_SPEC_ERROR = 131,
	MQTT_UNSUP_PROTO_VER = 132,
	MQTT_CLID_NOT_VALID = 133,
	MQTT_BAD_USER_OR_PASS = 134,
	MQTT_NOT_AUTHORIZED = 135,
	MQTT_SERVER_NOT_AVAIL = 134,
	MQTT_SERVER_BUSY = 137,
	MQTT_BANNED = 138,
	MQTT_SHUTTING_DOWN = 139,
	MQTT_BAD_AUTH_METHOD = 140,
	MQTT_KEEP_ALIVE_TIMEOUT = 141,
	MQTT_SESSION_TAKEN_OVER = 142,
	MQTT_TOPIC_FILTER_INVALID = 143,
	MQTT_TOPIC_NAME_INVALID = 144,
	MQTT_PACKET_ID_IN_USE = 145,
	MQTT_PACKET_ID_NOT_FOUND = 146,
	MQTT_RECV_MAX_EXCEEDED = 147,
	MQTT_TOPIC_ALIAS_INVALID = 148,
	MQTT_PACKET_TOO_LARGE = 149,
	MQTT_MSG_RATE_TOO_HIGH = 150,
	MQTT_QUOTA_EXCEEDED = 151,
	MQTT_ADMIN_ACTION = 152,
	MQTT_PAYLOAD_FORMAT_INVALID = 153,
	MQTT_RETAIN_NOT_SUPPORTED = 154,
	MQTT_QOS_NOT_SUPPORTED = 155,
	MQTT_USE_ANOTHER_SERVER = 156,
	MQTT_SERVER_MOVED = 157,
	MQTT_SHARED_SURSCR_NOT_SUPPORTED = 158,
	MQTT_CONN_RATE_EXCEEDED = 159,
	MQTT_MAX_CONNECT_TIME = 160,
	MQTT_SUBSCR_ID_NOT_SUPPORTED = 161,
	MQTT_WILDCARD_SUBSCR_NOT_SUPPORTED = 162
} mqtt_reason_t;


/* -------------------------------------------------------------------------- */
enum {
	RESERVED     = 0x00,
	CONNECT      = 0x10, // Connection request
	CONNACK      = 0x20, // Connect acknowledgment with flags
	PUBLISH      = 0x30, // Publish message
		PUB_DUP     = 8, // flags
		PUB_QOS     = 6,
		PUB_RETAIN  = 1,
	PUBACK       = 0x40, // Publish acknowledgment (QoS 1)
	PUBREC,      = 0x50, // Publish received (QoS 2 delivery part 1)
	PUBREL,      = 0x62, // Publish release  (QoS 2 delivery part 2)
	PUBCOMP,     = 0x70, // Publish complete (QoS 2 delivery part 3)
	SUBSCRIBE,   = 0x82, // Subscribe request
	SUBACK,      = 0x90, // Subscribe acknowledgment
	UNSUBSCRIBE, = 0xA2, // Unsubscribe request
	UNSUBACK,    = 0xB0, // Unsubscribe acknowledgment
	PINGREQ,     = 0xC0, // PING request
	PINGRESP,    = 0xD0, // PING response
	DISCONNECT,  = 0xE0, // Disconnect notification
	AUTH         = 0xF0  // Authentication exchange
} mqtt_packet_type_t;


/* -------------------------------------------------------------------------- */
static inline int mqtt_packet_has_pid(unsigned type)
{
	return type >= PUBLISH && type < PINGREQ;
}

/* -------------------------------------------------------------------------- */
typedef
struct mqtt_str {
	unsigned length;
	char *str;
} mqtt_str_t;

/* -------------------------------------------------------------------------- */
typedef
struct mqtt_cstr {
	unsigned length;
	char const *str;
} mqtt_cstr_t;


/* -------------------------------------------------------------------------- */
typedef
struct mqtt_str_pair
	mqtt_str_t name;
	mqtt_str_t value;
} mqtt_str_pair_t;


/* -------------------------------------------------------------------------- */
typedef
struct mqtt_bin {
	unsigned length;
	uint8_t *bin;
} mqtt_bin_t;


/* -------------------------------------------------------------------------- */
enum {
	MALFORMED = ~0,
	NOT_COMPLETE = ~1,
	FAILED = ~1
}


/* -------------------------------------------------------------------------- */
enum {
	MQTT_PAYLOAD_FORMAT_INDICATOR =  1,
	MQTT_MSG_EXP_INTERVAL         =  2,
	MQTT_CONTENT_TYPE             =  3,
	MQTT_RESPONSE_TOPIC           =  8,
	MQTT_CORRELATION_DATA         =  9,
	MQTT_SUBSCRIPTION_ID          = 11,
	MQTT_SESSION_EXP_INTERVAL     = 17,
	MQTT_ASSIGNED_CLIENT_ID       = 18,
	MQTT_SERVER_KEEP_ALIVE        = 19,
	MQTT_AUTH_METHOD              = 21,
	MQTT_AUTH_DATA                = 22,
	MQTT_REQ_PROBLEM_INFO         = 23,
	MQTT_WILL_DELAY_INTERVAL      = 24,
	MQTT_REQ_RESP_INFO            = 25,
	MQTT_RESP_INFO                = 26,
	MQTT_SERVER_REF               = 28,
	MQTT_REASON_STR               = 31,
	MQTT_RECV_MAX                 = 33,
	MQTT_TOPIC_ALIAS_MAX          = 34,
	MQTT_TOPIC_ALIAS              = 35,
	MQTT_MAX_QOS                  = 36,
	MQTT_RETAIN_AVAIL             = 37,
	MQTT_USER_PROPERTY            = 38,
	MQTT_MAX_PACKET_SIZE          = 39,
	MQTT_WILDCARD_SUBSCR_AVAIL    = 40,
	MQTT_SUBSCR_ID_AVAIL          = 41,
	MQTT_SHARED_SUBSCR_AVAIL      = 42
} mqtt_prop_id_t;


/* -------------------------------------------------------------------------- */
typedef
struct mqtt_props {
	uint64_t map;
	unsigned payload_format_ind;    //  1  byte   Will PUBLISH
	unsigned msg_exp_interval;      //  2  dword  Will PUBLISH
	mqtt_str_t content_type;        //  3  string Will PUBLISH

	mqtt_str_t response_topic;      //  8  string Will PUBLISH
	mqtt_bin_t corr_data;           //  9  binary Will PUBLISH

	unsigned subscription_id;       // 11  varint      PUBLISH SUBSCRIBE

	unsigned session_exp_interval;  // 17  dword                         CONNECT CONNACK DISCON
	mqtt_str_t assigned_client_id;  // 18  string                                CONNACK
	unsigned server_keep_alive;     // 19  word                                  CONNACK

	mqtt_str_t auth_method;         // 21  string                        CONNECT CONNACK        AUTH
	mqtt_bin_t auth_data;           // 22  binary                        CONNECT CONNACK        AUTH
	unsigned req_problem_info;      // 23  byte                          CONNECT
	unsigned will_delay_interval;   // 24  dword  Will
	unsigned req_resp_info;         // 25  byte                          CONNECT
	mqtt_str_t resp_info;           // 26  string                                CONNACK

	mqtt_str_t server_ref;          // 28  string                                CONNACK DISCON

	mqtt_str_t reason_str;          // 31  string PUBACK PUBREC PUBREL PUBCOMP SUBACK UNSUBACK CONNACK DISCON AUTH

	unsigned recv_max;              // 33  word                          CONNECT CONNACK
	unsigned topic_alias_max;       // 34  word                          CONNECT CONNACK
	unsigned topic_alias;           // 35  word        PUBLISH
	unsigned max_qos;               // 36  byte                                  CONNACK
	unsigned retain_avail;          // 37  byte                                  CONNACK
	mqtt_str_t user_prop;           // 38  string  all
	unsigned max_packet_size;       // 39  dword                         CONNECT CONNACK
	unsigned wildcard_subscr_avail; // 40  byte                                  CONNACK
	unsigned subscr_id_avail;       // 41  byte                                  CONNACK
	unsigned shared_subscr_avail;   // 42  byte                                  CONNACK
} mqtt_props_t;


/* -------------------------------------------------------------------------- */
enum {
	MQTT_BYTE,
	MQTT_WORD,
	MQTT_DWORD,
	MQTT_VAR_NUM,
	MQTT_STRING,
	MQTT_BINARY,
	MQTT_STR_PAIR
};


typedef
struct mqtt_conn {
	unsigned proto_version;// byte
	unsigned flags;        // byte
		// 128 - User Name
		//  64 - Password
		//  32 - Will Retain
		//  24 - Will QoS
		//   4 - Will Flag
		//   2 - Clean Start
		//   1 - Reserved
	unsigned keep_alive;   // word

	unsigned session_expire;
	unsigned receive_max;
	unsigned max_packet_size;
	unsigned topic_alias_maximum;
	unsigned req_response_info;
	unsigned req_problem_info;
	mqtt_bin_t auth_data;

	mqtt_map_t user_props;

	mqtt_str_t client_id;
} mqtt_conn_t;

typedef
union mqtt_var_head {
	struct {
//		mqtt_str_t proto_name; // { 4, "MQTT" }
		unsigned proto_version;// byte
		unsigned flags;        // byte
			// 128 - User Name
			//  64 - Password
			//  32 - Will Retain
			//  24 - Will QoS
			//   4 - Will Flag
			//   2 - Clean Start
			//   1 - Reserved
		unsigned keep_alive;   // word

	} conn;
	struct {
		unsigned flags;       // byte
			// 1 - Session Present
		unsigned reason;      // byte
	} conack;
	struct {
		mqtt_str_t topic_name;  // string
		unsigned packet_id;     // word // if Qos == 1 or == 2
	} publish;
	struct {
		unsigned packet_id;     // word
		unsigned reason;        // byte
	} puback; // pubrec, pubrel, pubcomp
	struct {
		unsigned packet_id;     // word
	} subscr; // suback, unsubscr, unsuback
	struct {
		unsigned packet_id;     // word
	} suback;
	struct {
		unsigned reason;      // byte
	} discon;
} mqtt_var_head_t;


#define OVER_VAR_INT 268435456

/* -------------------------------------------------------------------------- */
#define PROP(map, type, field) { map, MQTT_##type, (unsigned)&(((mqtt_props_t *)0)->field) }

typedef
struct mqtt_prop_info {
	unsigned in_packets;
	unsigned type;
	unsigned offset;
} mqtt_prop_info_t;



unsigned mqtt_get_var_int(uint8_t const **pp, uint8_t *end);
unsigned mqtt_get_var_int_size(unsigned n);


unsigned mqtt_get_num_size(unsigned type, unsigned n);

int mqtt_put_var_int(uint8_t *p, unsigned n);
int mqtt_rev_put_var_int(uint8_t *p, unsigned n);

unsigned mqtt_get_str(mqtt_str_t *s, uint8_t **pp, uint8_t *end);
unsigned mqtt_get_bin(mqtt_bin_t *s, uint8_t **pp, uint8_t *end);

int mqtt_unpack_props(unsigned pkt_type, mqtt_props_t *ps, uint8_t **pp, uint8_t *end);

int mqtt_get_num(unsigned *val, unsigned type, uint8_t const *p);

int mqtt_get_num_prop_size(unsigned prop, unsigned value);

int mqtt_put_num_prop(uint8_t *ptr, unsigned prop, unsigned value);

int mqtt_get_str_prop_size(unsigned prop, unsigned length);

int mqtt_put_str_prop(uint8_t **pp, unsigned prop, mqtt_cstr *ms);
int mqtt_put_bin_prop(uint8_t **pp, unsigned prop, mqtt_bin *mb);

mqtt_str_t *mqtt_str(mqtt_str_t *ms, char *s);
mqtt_cstr_t *mqtt_cstr(mqtt_cstr_t *ms, char const *s);


static inline unsigned mqtt_size(unsigned len)
{
	return mqtt_get_var_int_size(len) + len;
}

