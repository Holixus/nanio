#ifndef NANO_TIMERS_H
#define NANO_TIMERS_H

/* -------------------------------------------------------------------------- */
unsigned long io_now(void);
unsigned long long uptime_getmsl(void);


/* -------------------------------------------------------------------------- */
typedef struct io_timer io_timer_t;
typedef void (timer_handler_t)(void *custom);


/* -------------------------------------------------------------------------- */
struct io_timer {
	io_timer_t *next;
	long long next_time;
	int period;
	timer_handler_t *handler;
	void *custom;
};


/* -------------------------------------------------------------------------- */
io_timer_t *io_timer_alloc(timer_handler_t *handler, void *custom);

void io_timer_free(io_timer_t *self);

void io_timer_set_timeout(io_timer_t *self, int timeout);
void io_timer_set_period(io_timer_t *self, int period);
void io_timer_stop(io_timer_t *self);


/* -------------------------------------------------------------------------- */
/* internals */
void io_timers_init();
void io_timers_free();
io_timer_t *io_get_nearest_timer();
int io_get_timeout();

#endif
