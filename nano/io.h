/* -------------------------------------------------------------------------- */

typedef void (void_fn_t)();

void io_atexit(void_fn_t *fn);


extern char const *io_prog_name;

void start(int argc, char *argv[]);

