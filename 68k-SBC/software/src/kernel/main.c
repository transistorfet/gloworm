
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>

#include <kernel/filedesc.h>
#include <kernel/syscall.h>
#include <kernel/driver.h>
#include <kernel/vfs.h>

#include "api.h"
#include "process.h"
#include "interrupts.h"


extern void sh_task();


extern void init_syscall();
extern void init_vnode();
extern void init_mallocfs();

extern struct driver tty_68681_driver;
extern struct vnode_ops mallocfs_vnode_ops;

struct driver *drivers[] = {
	&tty_68681_driver,
	NULL	// Null Termination of Driver List
};

struct vnode *tty_vnode;
extern struct process *current_proc;
extern void *current_proc_stack;


#define TASK_SIZE	600
const char hello_task[TASK_SIZE] = {
#include "../test.txt"
};


void *create_context(void *user_stack, void *entry);

struct process *run_task()
{
	int error = 0;

	struct process *proc = new_proc();
	if (!proc) {
		puts("Ran out of procs\n");
		return NULL;
	}

	current_proc = proc;

	int fd = do_open("tty", 0);
	if (fd < 0) {
		printf("Error opening file tty %d\n", error);
		return NULL;
	}
	printf("FD: %d\n", fd);

	do_write(fd, "Hey\n", 4);


	//for (int i = 0; i < 0x4000; i++)
	//	asm volatile("");

	int task_size = 0x1800;
	char *task = malloc(task_size);
	char *task_stack_p = task + task_size;
	printf("Task Address: %x\n", task);
	printf("Task Stack: %x\n", task_stack_p);

	//memset_s(task, 0, 0xC00);		// With memset doing nothing, this value will not cause a fatal
	//memset_s(task, 0, 0xD00);		// With memset doing nothing, this value will sometimes cause a fatal
	//memset_s(task, 0, 0xF00);		// With memset doing nothing, this value will mostly cause a fatal

	//memset_s(task + 0x400, 0, 0x1000);	// TODO this will cause the fatal and the string glitch
	//memset_s(task + 0x800, 0, 0xB00);	// Works but causes the string glitch
	//memset_s(task + 0x400, 0, 0xB00);	// Works but causes the string glitch

	//memset_s(task, 0, task_size);
	memcpy_s(task, hello_task, TASK_SIZE);
	//dump(task, task_size);

	//print_stack();

 	task_stack_p = create_context(task_stack_p, task);
 	//task_stack_p = create_context(task_stack_p, exit2);

	//dump(task_stack_p, 0x40);

	proc->segments[S_TEXT].base = task;
	proc->segments[S_TEXT].length = task_size;
	proc->sp = task_stack_p;

	printf("After: %x\n", task_stack_p);

	return proc;
}

struct process *run_sh()
{
	int error = 0;

	struct process *proc = new_proc();
	if (!proc) {
		puts("Ran out of procs\n");
		return NULL;
	}

	current_proc = proc;

	int fd = do_open("tty", 0);
	if (fd < 0) {
		printf("Error opening file tty %d\n", error);
		return NULL;
	}
	printf("FD: %d\n", fd);


	int stack_size = 0x800;
	char *stack = malloc(stack_size);
	char *stack_p = stack + stack_size;
	printf("Sh Bottom: %x\n", stack);
	printf("Sh Stack: %x\n", stack_p);


 	stack_p = create_context(stack_p, sh_task);

	proc->segments[S_TEXT].base = NULL;
	proc->segments[S_TEXT].length = 0x10000;
	proc->segments[S_STACK].base = stack;
	proc->segments[S_STACK].length = stack_size;
	proc->sp = stack_p;

	printf("After: %x\n", stack_p);

	//dump(task, task_size);

	return proc;
}


void file_test()
{
	int error;
	struct vfile *file;

	if ((error = vfs_open("dir/test", 0, &file))) {
		printf("Error at open %d\n", error);
		return;
	}

	if ((error = vfs_write(file, "This is a file test\n", 20)) <= 0) {
		printf("Error when writing %d\n", error);
		return;
	}

	vfs_seek(file, 0, 0);

	char buffer[256];

	error = vfs_read(file, buffer, 256);
	if (error < 0) {
		printf("Error when reading\n");
		return;
	}
	printf("Read: %d\n", error);
	buffer[error] = '\0';

	puts(buffer);

	vfs_close(file);


	if ((error = vfs_open("/hello", O_CREAT, &file))) {
		printf("Error at open new file %d\n", error);
		return;
	}

	if ((error = vfs_write(file, hello_task, 256)) <= 0) {
		printf("Error when writing %d\n", error);
		return;
	}

	vfs_close(file);

	puts("done");

}

void dir_test()
{
	int error;
	struct vfile *file;
	struct vdir dir;

	if ((error = vfs_open("/", 0, &file))) {
		printf("Error at open %d\n", error);
		return;
	}

	while (1) {
		error = vfs_readdir(file, &dir);
		if (error < 0) {
			printf("Error at readdir %d\n", error);
			return;
		}
		else if (error == 0)
			break;
		else {
			printf("File: %d:%s\n", error, dir.name);
		}
	}


}


int main()
{
	DISABLE_INTS();


	init_heap((void *) 0x110000, 0xD0000);

	init_interrupts();
	init_syscall();
	init_proc();

	init_vnode();
	init_fileptr_table();
	init_mallocfs();

	// Initialize drivers
	for (char i = 0; drivers[i]; i++) {
		drivers[i]->init();
	}

	file_test();
	dir_test();

	struct process *task = run_task();
	run_sh();

	//for (int i = 0; i < 0x2800; i++)
	//	asm volatile("");

	//printf("THINGS %x\n", current_proc);

	current_proc = task;
	current_proc_stack = task->sp;

	//panic("Panicking for good measure\n");

	// Start Multitasking
	asm("bra restore_context\n");
	//asm("stop #0x2700\n");


	// Force an address error
	//volatile uint16_t *data = (uint16_t *) 0x100001;
	//volatile uint16_t value = *data;

	//char *str = "Hey syscall, %d";
	//asm(
	//"move.l	#2, %%d0\n"
	//"move.l	%0, %%d1\n"
	//"move.l	#125, %%a0\n"
	//"trap	#1\n"
	//: : "r" (str)
	//);

}

