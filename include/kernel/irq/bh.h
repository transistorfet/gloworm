
#ifndef _KERNEL_IRQ_BH_H
#define _KERNEL_IRQ_BH_H

#define BH_MAX			4
#define BH_TTY68681		0
#define BH_TTY			1
#define BH_SLIP			2
#define BH_NET			3

typedef void (*bh_handler_t)(void *);

struct bh_handler {
	bh_handler_t fn;
	void *data;
};


void init_bh();
void register_bh(int bhnum, bh_handler_t fn, void *data);
void enable_bh(int bhnum);
void disable_bh(int bhnum);
void request_bh_run(int bhnum);
void run_bh_handlers();

#endif
