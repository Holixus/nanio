#ifndef NANO_IO_LOG_H
#define NANO_IO_LOG_H

#define IO_LOG_FN(name) \
void io_##name(char const *fmt, ...) __attribute__ ((format (printf, 1, 2)));

IO_LOG_FN(debug)
IO_LOG_FN(info)
IO_LOG_FN(warn)
IO_LOG_FN(err)
IO_LOG_FN(fail)

#undef IO_LOG_FN

void io_log_open(char const *prog);

#endif
