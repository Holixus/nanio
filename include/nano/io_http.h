#ifndef NANO_HTTP_H
#define NANO_HTTP_H

typedef
enum {
	HTTP_ERROR = 0, HTTP_CONNECT, HTTP_DELETE,
	HTTP_GET, HTTP_HEAD, HTTP_OPTIONS,
	HTTP_PATCH, HTTP_POST, HTTP_PUT,
	HTTP_TRACE
} http_req_method_t;

typedef
enum {
	HTTP_SWITCHING_PROTOCOL = 101,
	HTTP_DATA_FOLLOWS = 200,
	HTTP_NO_CONTENT = 204,
	HTTP_NO_REDIRECT = 301,
	HTTP_TEMPORARY_REDIRECT = 307,
	HTTP_BAD_REQUEST = 400,
	HTTP_UNAUTHORIZED = 401,
	HTTP_FORBIDDEN = 403,
	HTTP_PAGE_NOT_FOUND = 404,
	HTTP_ACCESS_DENIED = 405,
	HTTP_UNSUPPORTED_MEDIA_TYPE = 415,
	HTTP_PROCESS_FAILED = 420,
	HTTP_UPGRADE_REQUIRED = 426,
	HTTP_WEB_ERROR = 500,
	HTTP_NOT_IMPLEMENTED = 501,
	HTTP_SERVICE_UNAVAILABLE = 503
} http_response_type_t;



typedef
struct io_http_req_info {
	int method;
	int version;
	char path[248];
} io_http_req_t;



char const *io_http_code(int code);
char const *io_http_req_method(int method);

size_t io_url_decoded_length(char const *encoded_url);
size_t io_url_encoded_length(char const *url);

char *io_url_decode(char *out, size_t size, char const *encoded_url);
char *io_url_encode(char *to, size_t size, char const *url);

int io_http_req_parse(io_http_req_t *req, char const **text);
int io_http_header_parse(io_hmap_t *h, char const **header);

#endif
