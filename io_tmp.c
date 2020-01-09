#include "nano/io.h"
#include "io_internals.h"

/* ------------------------------------------------------------------------ */
char *_getTmpStr()
{
	static char buf[8][TMP_STR_SIZE];
	static int index = 0;
	return buf[index++ & 7];
}

