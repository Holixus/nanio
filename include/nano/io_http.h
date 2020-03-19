#ifndef NANO_HTTP_H
#define NANO_HTTP_H

typedef
enum {
	HTTP_ERROR = 0, HTTP_CONNECT, HTTP_DELETE, 
	HTTP_GET, HTTP_HEAD, HTTP_OPTIONS, 
	HTTP_PATCH, HTTP_POST, HTTP_PUT,
	HTTP_TRACE
} http_req_type_t;


typedef
struct io_http_req_info {
	int method;
	int version;
	char path[248];
} io_http_req_t;



char const *io_http_code(int code);

size_t io_url_decoded_length(char const *encoded_url);
size_t io_url_encoded_length(char const *url);

char *io_url_decode(char *out, size_t size, char const *encoded_url);
char *io_url_encode(char *to, size_t size, char const *url);

int io_http_req_parse(io_http_req_t *req, char const **text);
int io_http_header_parse(io_hmap_t *h, char const *header);

#endif
