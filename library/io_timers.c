#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>

#include "nano/io.h"
#include "nano/io_timers.h"


/* ------------------------------------------------------------------------ */
unsigned long long uptime_getmsl()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}


/* ------------------------------------------------------------------------ */
unsigned long io_now()
{
	static unsigned long long _zero_time = 0;
	if (!_zero_time)
		_zero_time = uptime_getmsl();
	return (unsigned long)(uptime_getmsl() - _zero_time);
}


/* -------------------------------------------------------------------------- */

#define IO_TIMERS_LIMIT 10

static io_timer_t io_timers[IO_TIMERS_LIMIT];
static io_timer_t *io_free_timers, *io_active_timers;


/* -------------------------------------------------------------------------- */
void io_timers_init()
{
	io_active_timers = NULL;
	io_free_timers = io_timers;
	auto io_timer_t *t = io_timers, *last = io_timers + IO_TIMERS_LIMIT - 1;
	for (; t < last; ++t) {
		t->next_time = -1;
		t->next = t+1;
	}
	t->next_time = -1;
	t->next = NULL;
}

/* -------------------------------------------------------------------------- */
void io_timers_free()
{
	while (io_active_timers)
		io_timer_free(io_active_timers);
}

/* -------------------------------------------------------------------------- */
io_timer_t *io_timer_alloc(timer_handler_t *handler, void *custom)
{
	if (!io_free_timers)
		return NULL;

	io_timer_t *self = io_free_timers;
	io_free_timers = self->next;
	self->next = io_active_timers;
	io_active_timers = self;
	self->next_time = -1;
	self->period = 0;
	self->handler = handler;
	self->custom = custom;
	return self;
}

/* -------------------------------------------------------------------------- */
void io_timer_free(io_timer_t *self)
{
	for (auto io_timer_t **t = &io_active_timers; t; t = &t[0]->next)
		if (*t == self) {
			*t = self->next;
			self->next = io_free_timers;
			io_free_timers = self;
			break;
		}
}

/* -------------------------------------------------------------------------- */
void io_timer_set_timeout(io_timer_t *self, int timeout)
{
	self->next_time = io_now() + timeout;
}

/* -------------------------------------------------------------------------- */
void io_timer_set_period(io_timer_t *self, int period)
{
	io_timer_set_timeout(self, period);
	self->period = period;
}

/* -------------------------------------------------------------------------- */
void io_timer_stop(io_timer_t *self)
{
	self->next_time = -1;
}

/* -------------------------------------------------------------------------- */
io_timer_t *io_on_timeout(timer_handler_t *handler, void *custom, int timeout)
{
	io_timer_t *t = io_timer_alloc(handler, custom);
	if (t)
		io_timer_set_timeout(t, timeout);
	return t;
}


/* -------------------------------------------------------------------------- */
io_timer_t *io_on_period(timer_handler_t *handler, void *custom, int period)
{
	io_timer_t *t = io_timer_alloc(handler, custom);
	if (t)
		io_timer_set_period(t, period);
	return t;
}


/* -------------------------------------------------------------------------- */
int io_has_active_timers()
{
	return io_active_timers != NULL;
}


/* -------------------------------------------------------------------------- */
io_timer_t *io_get_nearest_timer()
{
	auto io_timer_t *active = NULL;
	for (io_timer_t *t = io_active_timers; t; t = t->next) {
		if (t->next_time > 0) {
			active = t;
			for (t = t->next; t; t = t->next) {
				auto long long nt = t->next_time;
				if (nt > 0 && nt < active->next_time)
					active = t;
			}
			break;
		}
	}
	return active;
}


/* -------------------------------------------------------------------------- */
int io_get_timeout()
{
	long long now = io_now();
	auto io_timer_t *t;
	do {
		t = io_get_nearest_timer();
		if (!t)
			return -1;// no timers

		if (t->next_time > now)
			break;

		if (t->period)
			while ((t->next_time += t->period) <= now);
		else
			io_timer_free(t);  // timer will not be cleaned but only moved to free list
		t->handler(t->custom); // it's safe(yet) to handle free timer at the place
	} while (1);

	return (int)(t->next_time - now);
}
