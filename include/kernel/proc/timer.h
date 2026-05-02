
#ifndef _KERNEL_PROC_TIMER_H
#define _KERNEL_PROC_TIMER_H

#include <stdint.h>
#include <kernel/time.h>
#include <kernel/utils/queue.h>

struct timer;

typedef int (*timer_callback_t)(struct timer *timer, void *argp);

struct timer {
	struct queue_node node;
	nanos_t expires_time;
	void *argp;
	timer_callback_t callback;
};

void init_timer_list(void);
void init_timer(struct timer *timer);
int add_timer(struct timer *timer, int seconds, int microseconds);
int remove_timer(struct timer *timer);
void check_timers(nanos_t uptime);
void check_timers_bh(void *_unused);

#endif
