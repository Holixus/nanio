#define debug(f, ...) fprintf(stderr, "   " f "\n", __VA_ARGS__)
#define error(f, ...) fprintf(stderr, ":( " f ": %s\n", __VA_ARGS__, strerror(errno))
