
#include <time.h>
#include <kernel/time.h>
#include <kernel/printk.h>
#include <kernel/irq/bh.h>


static time_t system_seconds;
static time_t system_subseconds_nanos;
static cycles_t current_clock_last_cycle;
static nanos_t monotonic_uptime_nanos;

struct clocksource *current_clock;

void init_time(void)
{
	system_seconds = 0;
	system_subseconds_nanos = 0;
	current_clock_last_cycle = 0;
	monotonic_uptime_nanos = 0;
	current_clock = NULL;
}

int register_clock(struct clocksource *clock)
{
	if (!current_clock || clock->rating > current_clock->rating) {
		log_debug("%s: switching to clock %s\n", __func__, clock->name);
		current_clock = clock;
		return 1;
	} else {
		log_info("%s: the rating of clock %s is not better than %s\n", __func__, clock->name, current_clock->name);
		return 0;
	}
}

void update_time(void)
{
	nanos_t diff;
	cycles_t cycles;

	request_bh_run(BH_TIMER);

	if (!current_clock)
		return;

	cycles = current_clock->read(current_clock);

	// Check if the clock has overflowed
	if (cycles == current_clock_last_cycle) {
		return;
	} else if (cycles < current_clock_last_cycle) {
		diff = (current_clock->max_cycles - current_clock_last_cycle) + cycles;
	} else {
		diff = cycles;
	}
	current_clock_last_cycle = cycles;

	// Convert to nanoseconds
	diff = current_clock->cycles_to_nanos(current_clock, diff);

	// Update monotonic uptime
	monotonic_uptime_nanos += diff;

	// Update epoch time
	system_subseconds_nanos += diff;
	while (system_subseconds_nanos >= NANOS_PER_SECOND) {
		system_subseconds_nanos -= NANOS_PER_SECOND;
		system_seconds += 1;
	}
}

void set_system_time(time_t t)
{
	system_seconds = t;
}

time_t get_system_time(void)
{
	return system_seconds;
}

nanos_t get_monotonic_uptime(void)
{
	return monotonic_uptime_nanos;
}

