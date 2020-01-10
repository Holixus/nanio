#ifndef NANO_DEBUG_H
#define NANO_DEBUG_H

void trace(char const *fmt, ...) __attribute__ ((format (printf, 1, 2)));

void die(char const *fmt, ...) __attribute__ ((format (printf, 1, 2)));

void trace_clean();


#endif
