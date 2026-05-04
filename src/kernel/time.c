
#include <time.h>
#include <asm/irqs.h>
#include <kernel/time.h>
#include <kernel/printk.h>
#include <kernel/irq/bh.h>


struct clocksource *current_clock;
static time_t system_seconds;
static time_t system_subseconds_nanos;
static cycles_t current_clock_last_cycle;
static nanos_t monotonic_uptime_nanos;

static char clock_warning_given = 0;

void init_time(void)
{
	system_seconds = 0;
	system_subseconds_nanos = 0;
	current_clock_last_cycle = 0;
	monotonic_uptime_nanos = 0;
	current_clock = NULL;
	clock_warning_given = 0;
}

int register_clock(struct clocksource *clock)
{
	if (!current_clock || clock->rating > current_clock->rating) {
		log_debug("clock: switching to clock %s\n", clock->name);
		current_clock = clock;
		return 1;
	} else {
		log_info("clock: the rating of clock %s is not better than %s\n", clock->name, current_clock->name);
		return 0;
	}
}

void update_time(void)
{
	nanos_t diff;
	cycles_t cycles;

	short saved_status;
	LOCK(saved_status);

	if (current_clock) {
		cycles = current_clock->read(current_clock);

		if (cycles == current_clock_last_cycle) {
			// If no change, then do nothing and return
			UNLOCK(saved_status);
			return;
		} else if (cycles > current_clock_last_cycle) {
			diff = cycles - current_clock_last_cycle;
		} else {
			// If the clock has overflowed
			diff = (current_clock->max_cycles - current_clock_last_cycle) + cycles;
		}
		current_clock_last_cycle = cycles;

		// Convert to nanoseconds
		diff = current_clock->cycles_to_nanos(current_clock, diff);
	} else {
		if (!clock_warning_given) {
			clock_warning_given = 1;
			log_warning("clock: no clock was set, defaulting to rough counting\n");
		}
		// If there's no current clock, just increase the clock by a bit
		// This function should be called periodically whenever an irq occurs, which could
		// be any length of time.  A fixed amount is better than nothing
		diff = 1000000;
	}

	// Update monotonic uptime
	monotonic_uptime_nanos += diff;

	// Update epoch time
	system_subseconds_nanos += diff;
	while (system_subseconds_nanos >= NANOS_PER_SECOND) {
		system_subseconds_nanos -= NANOS_PER_SECOND;
		system_seconds += 1;
	}

	request_bh_run(BH_TIMER);

	UNLOCK(saved_status);
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

