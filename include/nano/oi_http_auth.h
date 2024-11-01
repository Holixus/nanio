#ifndef NANO_HTTP_AUTH_H
#define NANO_HTTP_AUTH_H

typedef
struct http_auth {
	enum { HTTP_AUTH_NONE, HTTP_AUTH_BASIC, HTTP_AUTH_DIGEST } type;

	char realm[64];
	char domain[128];
	char nonce[64];
	char sess_nonce[64];
	char nextnonce[64];
	char opaque[64];
	enum { AUTH_QOP_NONE = 0, AUTH_QOP_AUTH = 1, AUTH_QOP_AUTH_INT = 2, AUTH_QOP_ELSE = 4 } qop;
	enum { ALG_MD5, ALG_MD5_SESS, ALG_MD5_ELSE } algorithm;
	enum { STALE_FALSE, STALE_TRUE } stale;

	char username[64];
	char password[64];
	char method[16]; /* HTTP request type (GET|POST|HEAD...) */
	char uri[128]; /* encoded (from HTTP request) */
	char cnonce[64];
	char nc[16];
	char response[40];

	char const *entity;
} io_http_auth_t;

void io_http_auth_empty(io_http_auth_t *d);
int io_http_auth_parse(io_http_auth_t *d, char const *auth);

int io_http_auth_response_calc(io_http_auth_t *d, char *response);

int io_http_auth_response(io_http_auth_t *d, char *out, size_t size); /* 401 WWW-Authenticate */
int io_http_auth_request (io_http_auth_t *d, char *out, size_t size); /*     Authorization */



#endif
