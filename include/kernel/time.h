
#ifndef _KERNEL_TIME_H
#define _KERNEL_TIME_H

#include <time.h>
#include <stdint.h>

#define NANOS_PER_SECOND	1000000000

typedef uint64_t nanos_t;
typedef uint64_t cycles_t;

/// Represents a monotonic clock used to determine the current time inside the kernel
///
/// The `read` function will return a monotonic count of the number of cycles that have occurred
/// since the system/clock was started.  The count will wrap around to 0 at `max_cycles` - 1, and
/// the cycle count can be converted to nanoseconds using the `cycles_to_nanos` function
struct clocksource {
	/// Name of the clock source for logging
	char *name;
	/// A quality rating of this clock (higher is better, 100 is average/good)
	int rating;
	/// The cycle count at which it will roll over to 0 again (ie. the cycles
	/// return by `read` will always be between 0 and max_cycles - 1)
	cycles_t max_cycles;
	/// Read the current counter value of this clock source (must be monotonic)
	cycles_t (*read)(struct clocksource *clk);
	/// Convert a given number of cycles into an equivalent number of nanoseconds
	nanos_t (*cycles_to_nanos)(struct clocksource *clk, cycles_t cycles);
};

void init_time(void);
int register_clock(struct clocksource *clk);
void update_time(void);
void set_system_time(time_t t);
time_t get_system_time(void);
nanos_t get_monotonic_uptime(void);

#endif
