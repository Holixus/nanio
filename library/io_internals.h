#ifndef NANO_IO_INTERNALS
#define NANO_IO_INTERNALS

enum { TMP_STR_SIZE = 48 };

char *_getTmpStr();

#ifndef countof
# define countof(a) (sizeof(a)/sizeof(a[0]))
#endif

#endif
