#if 0
#ifndef NANO_IO_KEY_H
#define NANO_IO_KEY_H

/* -------------------------------------------------------------------------- */
typedef
struct _key_command {
	char const seq[4];
	int code;
} key_command_t;

typedef void (seq_handler_t)(void *self, char const *seq, int code);

int io_hotkey_on(key_command_t const *cmds, seq_handler_t *onseq, void *data);
int io_hotkey_off(key_command_t const *cmds, seq_handler_t *onseq, void *data);


typedef void (char_handler_t)(void *self, int code);
int io_charkey_on(char_handler_t *onchar, void *data);

#endif
#endif
