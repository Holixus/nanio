#define debug(f, ...) fprintf(stderr, "   " f "\n", __VA_ARGS__)
#define error(f, ...) fprintf(stderr, ":( " f ": %s\n", __VA_ARGS__, strerror(errno))

#define io_debug(f, d, ...) fprintf(stderr, "   %s " f "\n", (d)->vmt->name, __VA_ARGS__)

#define io_stream_debug(s, f, ...) \
	fprintf(stderr, "   %s <%s> " f "\n", (s)->d.vmt->name, io_sock_stoa(&(s)->sa), ##__VA_ARGS__)

#define io_stream_error(s, f, ...) \
	fprintf(stderr, ":( %s <%s> " f ": %s\n", (s)->d.vmt->name, io_sock_stoa(&(s)->sa), ##__VA_ARGS__, strerror(errno))
