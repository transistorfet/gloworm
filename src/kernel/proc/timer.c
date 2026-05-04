
#include <kernel/time.h>
#include <kernel/irq/bh.h>
#include <kernel/proc/timer.h>
#include <kernel/proc/signal.h>
#include <kernel/proc/scheduler.h>
#include <kernel/utils/queue.h>


static struct queue timer_list;

void init_timer_list(void)
{
	_queue_init(&timer_list);
	register_bh(BH_TIMER, check_timers_bh, NULL);
}

void init_timer(struct timer *timer)
{
	_queue_node_init(&timer->node);
	timer->expires_time = 0;
}

int add_timer(struct timer *timer, int seconds, int microseconds)
{
	nanos_t uptime;

	if (timer->expires_time)
		_queue_remove(&timer_list, &timer->node);
	uptime = get_monotonic_uptime();
	timer->expires_time = uptime + (seconds * NANOS_PER_SECOND) + (microseconds * 1000);
	_queue_insert(&timer_list, &timer->node);
	return 0;
}

int remove_timer(struct timer *timer)
{
	if (!timer->expires_time)
		return -1;
	_queue_remove(&timer_list, &timer->node);
	timer->expires_time = 0;
	return 0;
}

void check_timers(nanos_t uptime)
{
	struct timer *next = NULL;

	for (struct timer *cur = (struct timer *) _queue_head(&timer_list); cur; cur = next) {
		next = (struct timer *) _queue_next(&cur->node);
		if (uptime > cur->expires_time) {
			remove_timer(cur);
			cur->callback(cur, cur->argp);
		}
	}
}

void check_timers_bh(void *_unused)
{
	nanos_t uptime;

	uptime = get_monotonic_uptime();
	check_timers(uptime);
	check_reschedule(uptime);
}

