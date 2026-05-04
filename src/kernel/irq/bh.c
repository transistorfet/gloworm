
#include <stddef.h>
#include <stdint.h>

#include <kernel/irq/bh.h>


uint16_t bh_enabled;
uint16_t bh_requested;
static struct bh_handler bh_handlers[BH_MAX];

void init_bh(void)
{
	bh_enabled = 0;
	bh_requested = 0;
	for (short i = 0; i < BH_MAX; i++) {
		bh_handlers[i].fn = NULL;
		bh_handlers[i].data = NULL;
	}
}

void register_bh(int bhnum, bh_handler_t fn, void *data)
{
	bh_handlers[bhnum].fn = fn;
	bh_handlers[bhnum].data = data;
	bh_enabled |= (1 << bhnum);
}

void enable_bh(int bhnum)
{
	bh_enabled |= (1 << bhnum);
}

void disable_bh(int bhnum)
{
	bh_enabled &= ~(1 << bhnum);
}

void request_bh_run(int bhnum)
{
	bh_requested |= (1 << bhnum);
}

void run_bh_handlers(void)
{
	int bit = 0x0001;
	uint16_t requested = bh_enabled & bh_requested;

	for (short i = 0; i < BH_MAX; i++, bit <<= 1) {
		if (requested & bit) {
			bh_requested &= ~bit;
			if (!bh_handlers[i].fn)
				continue;
			bh_handlers[i].fn(bh_handlers[i].data);
		}
	}
}

