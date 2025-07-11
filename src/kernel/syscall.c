
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/sched.h>
#include <sys/ioc_tty.h>
#include <sys/socket.h>

#include <kconfig.h>
#include <asm/irqs.h>
#include <kernel/api.h>
#include <kernel/time.h>
#include <kernel/printk.h>
#include <kernel/drivers.h>
#include <kernel/fs/vfs.h>
#include <kernel/arch/context.h>
#include <kernel/proc/exec.h>
#include <kernel/proc/fork.h>
#include <kernel/proc/timer.h>
#include <kernel/proc/memory.h>
#include <kernel/proc/signal.h>
#include <kernel/proc/process.h>
#include <kernel/proc/binaries.h>
#include <kernel/proc/filedesc.h>
#include <kernel/proc/scheduler.h>
#include <kernel/net/socket.h>


#include <asm/syscall_table.h>

extern void enter_syscall(void);

void init_syscall(void)
{
	arch_set_irq_handler(IRQ_TRAP1, enter_syscall);
}

extern struct process *current_proc;
extern struct syscall_record *current_syscall;


#if defined(CONFIG_TTY_68681)
void tty_68681_set_leds(uint8_t bits);
void tty_68681_reset_leds(uint8_t bits);
#endif

/// Called on entry to any system call
///
/// Any code that should run before a syscall is processed should go here, such
/// as tracing or debugging info.  The `current_syscall` global will be set with
/// the syscall info that is about to be executed
void syscall_entry(void)
{
	#if defined(CONFIG_TTY_68681)
	tty_68681_set_leds(0x04);
	#endif
}

/// Called when exiting any system call
///
/// Any code that should be run after a syscall has run but before returning to
/// user code should go here.  This is mostly a place to close any tracing or
/// debugging info, as a counterpart to `syscall_entry()`
void syscall_exit(void)
{
	#if defined(CONFIG_TTY_68681)
	tty_68681_reset_leds(0x04);
	#endif
}

//
// Perform a system call and pass the return value to the calling process
//
void do_syscall(void)
{
	int ret;

	syscall_entry();
	ret = ((syscall_t) syscall_table[current_syscall->syscall])(current_syscall->arg1, current_syscall->arg2, current_syscall->arg3, current_syscall->arg4, current_syscall->arg5);
	return_to_current_proc(ret);
	syscall_exit();
}

void do_exit(int exitcode)
{
	exit_proc(current_proc, exitcode);
	resume_waiting_parent(current_proc);
}

pid_t do_fork(void)
{
	int error;
	struct process *proc;
	struct clone_args args;

	args.entry = NULL;
	args.stack = NULL;
	args.flags = 0;
	args.arg = NULL;

	error = clone_process(current_proc, &args, &proc);
	if (error < 0)
		return error;

	return proc->pid;
}

/// Clone a process (more precise than fork())
///
/// NOTE: this syscall may have an alternate entry to save the full context on
/// the stack before cloning it.  Because of that, this function uses the
/// `current_syscall` struct to access the arguments.
//	Actual Args: int (*fn)(void *), void *stack, int flags, void *arg
pid_t do_clone(void)
{
	int error;
	struct process *proc;
	struct clone_args args;

	args.entry = (int (*)(void *)) current_syscall->arg1;
	args.stack = (void *) current_syscall->arg2;
	args.flags = current_syscall->arg3;
	args.arg = (void *) current_syscall->arg4;

	error = clone_process(current_proc, &args, &proc);
	if (error < 0)
		return error;

	return proc->pid;
}

int do_exec(const char *path, const char *const argv[], const char *const envp[])
{
	int error;

	error = load_binary(path, current_proc, argv, envp);
	if (error == EKILL) {
		// An error occurred past the point of no return.  The memory maps have been irrepairably damaged, so kill the process
		log_error("Process terminated\n");
		exit_proc(current_proc, -1);
		resume_waiting_parent(current_proc);
		return error;
	} else if (error) {
		return error;
	}

	return 0;
}

int do_execbuiltin(void *addr, const char *const argv[], const char *const envp[])
{
	int error;

	// Reset sbrk to the start of the heap
	error = memory_map_reset_sbrk(current_proc->map);
	if (error < 0) {
		return error;
	}

	// NOTE no modification of the memory maps here, since the code should be in the same process
	exec_initialize_stack_with_args(current_proc, (void *) current_proc->map->stack_end, addr, argv, envp);
	return 0;
}


pid_t do_waitpid(pid_t pid, int *status, int options)
{
	struct process *proc;

	// Must be a valid pid, or -1 to wait for any process
	if (pid <= 0 && pid != -1)
		return EINVAL;

	// TODO should this wake up all parent procesess instead of just one?
	proc = find_exited_child(current_proc->pid, pid);
	if (!proc) {
		current_proc->bits |= PB_WAITING;
		suspend_current_syscall(0);
	} else {
		current_proc->bits &= ~PB_WAITING;
		*status = proc->exitcode;
		pid = proc->pid;
		cleanup_proc(proc);
		return pid;
	}

	return -1;
}

int do_kill(pid_t pid, int sig)
{
	if (pid < 0) {
		return send_signal_process_group(-pid, sig);
	} else {
		return send_signal(pid, sig);
	}
}

int do_sigreturn(void)
{
	cleanup_signal_handler();
	return 0;
}

int do_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	if (oldact)
		get_signal_action(current_proc, signum, oldact);

	if (act)
		return set_signal_action(current_proc, signum, act);
	return 0;
}

unsigned int do_alarm(unsigned int seconds)
{
	return set_proc_alarm(current_proc, seconds);
}

int do_pause(void)
{
	current_proc->bits |= PB_PAUSED;
	suspend_current_syscall(0);
	return 0;
}

int do_brk(void *addr)
{
	int diff;

	diff = (uintptr_t) addr - current_proc->map->sbrk;
	if (current_proc->map->heap_start + diff >= (uintptr_t) arch_get_user_stackp(current_proc)) {
		return ENOMEM;
	}

	return memory_map_move_sbrk(current_proc->map, diff);
}

void *do_sbrk(intptr_t diff)
{
	if (diff) {
		if (current_proc->map->heap_start + diff >= (uintptr_t) arch_get_user_stackp(current_proc)) {
			return NULL;
		}

		if (memory_map_move_sbrk(current_proc->map, diff))
			return NULL;
	}

	return (void *) current_proc->map->sbrk;
}

pid_t do_gettid(void)
{
	return current_proc->tid;
}

pid_t do_getpid(void)
{
	return current_proc->pid;
}

pid_t do_getppid(void)
{
	return current_proc->parent;
}

pid_t do_getpgid(pid_t pid)
{
	struct process *proc = pid ? get_proc(pid) : current_proc;

	if (!proc)
		return ESRCH;
	return proc->pgid;
}

int do_setpgid(pid_t pid, pid_t pgid)
{
	struct process *proc = pid ? get_proc(pid) : current_proc;

	if (pgid < 0)
		return EINVAL;
	if (!proc || (proc != current_proc && proc->pid != current_proc->parent))
		return ESRCH;

	if (pgid == 0) {
		proc->pgid = proc->pid;
	} else {
		struct process *pg;

		pg = get_proc(pgid);
		if (!pg || pg->session != proc->session || proc->pid == proc->session)
			return EPERM;
		proc->pgid = pgid;
	}
	return 0;
}

pid_t do_getsid(pid_t pid)
{
	struct process *proc = pid ? get_proc(pid) : current_proc;

	if (!proc)
		return ESRCH;
	return current_proc->session;
}

pid_t do_setsid(void)
{
	struct process *proc;
	struct process_iter iter;

	proc_iter_start(&iter);
	while ((proc = proc_iter_next(&iter))) {
		if (proc != current_proc && proc->pgid == current_proc->pid)
			return EPERM;
	}

	current_proc->session = current_proc->pid;
	current_proc->pgid = current_proc->pid;
	return current_proc->session;
}

uid_t do_getuid(void)
{
	return current_proc->uid;
}

int do_setuid(uid_t uid)
{
	// TODO this isn't entirely accurate according to the standards
	if (current_proc->uid != SU_UID)
		return EPERM;
	current_proc->uid = uid;
	return 0;
}



int do_mount(const char *source, const char *target, struct mount_opts *opts)
{
	extern struct mount_ops *filesystems[];
	extern struct mount_ops minix_mount_ops;

	struct vnode *vnode;
	struct mount_ops *fsptr = NULL;

	if (opts && opts->fstype) {
		for (short i = 0; filesystems[i]; i++) {
			if (!strcmp(filesystems[i]->fstype, opts->fstype)) {
				fsptr = filesystems[i];
				break;
			}
		}
		if (!fsptr)
			return EINVAL;
	}
	if (!fsptr)
		fsptr = &minix_mount_ops;

	if (vfs_lookup(current_proc->cwd, source, VLOOKUP_NORMAL, current_proc->uid, &vnode))
		return ENOENT;
	return vfs_mount(current_proc->cwd, target, vnode->rdev, fsptr, opts ? opts->mountflags : 0, current_proc->uid);
}

int do_umount(const char *source)
{
	struct vnode *vnode;

	if (vfs_lookup(current_proc->cwd, source, VLOOKUP_NORMAL, current_proc->uid, &vnode))
		return ENOENT;

	return vfs_unmount(vnode->rdev, current_proc->uid);
}

int do_sync(void)
{
	return vfs_sync(0);
}

int do_mknod(const char *path, mode_t mode, device_t dev)
{
	int error;
	struct vnode *vnode;

	error = vfs_mknod(current_proc->cwd, path, mode, dev, current_proc->uid, &vnode);
	if (error)
		return error;
	vfs_release_vnode(vnode);
	return 0;
}

int do_link(const char *oldpath, const char *newpath)
{
	return vfs_link(current_proc->cwd, oldpath, newpath, current_proc->uid);
}

int do_unlink(const char *path)
{
	return vfs_unlink(current_proc->cwd, path, current_proc->uid);
}

int do_rename(const char *oldpath, const char *newpath)
{
	return vfs_rename(current_proc->cwd, oldpath, newpath, current_proc->uid);
}

int do_mkdir(const char *path, mode_t mode)
{
	int error;
	struct vfile *file = NULL;

	mode = mode & ~current_proc->umask & 0777;
	error = vfs_open(current_proc->cwd, path, O_CREAT, S_IFDIR | mode, current_proc->uid, &file);
	if (error < 0)
		return error;
	vfs_close(file);
	return 0;
}

char *do_getcwd(char *buf, size_t size)
{
	if (vfs_reverse_lookup(current_proc->cwd, buf, size, current_proc->uid))
		return NULL;
	return buf;
}

int do_creat(const char *path, mode_t mode)
{
	return do_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int do_open(const char *path, int oflags, mode_t mode)
{
	int fd;
	int error;
	struct vfile *file = NULL;

	fd = find_unused_fd(current_proc->fd_table);
	if (fd < 0)
		return fd;

	mode = mode & ~current_proc->umask & 0777;
	error = vfs_open(current_proc->cwd, path, oflags, mode, current_proc->uid, &file);
	if (error)
		return error;

	set_fd(current_proc->fd_table, fd, file);

	return fd;
}

int do_close(int fd)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file)
		return EBADF;

	vfs_close(file);

	unset_fd(current_proc->fd_table, fd);
	//free_fd(current_proc->fd_table, fd);
	return 0;
}

size_t do_read(int fd, char *buf, size_t nbytes)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file)
		return EBADF;
	return vfs_read(file, buf, nbytes);
}


size_t do_write(int fd, const char *buf, size_t nbytes)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file)
		return EBADF;
	return vfs_write(file, buf, nbytes);
}

int do_lseek(int fd, offset_t offset, int whence)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file)
		return EBADF;
	return vfs_seek(file, offset, whence);
}

int do_readdir(int fd, struct dirent *dir)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file)
		return EBADF;
	return vfs_readdir(file, dir);
}

int do_ioctl(int fd, unsigned int request, void *argp)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file)
		return EBADF;
	return vfs_ioctl(file, request, argp, current_proc->uid);
}

extern int do_fcntl(int fd, int cmd, void *argp)
{
	// TODO what are the arguments it can take?
	return -1;
}

int do_chdir(const char *path)
{
	int error;
	struct vnode *vnode;

	if ((error = vfs_lookup(current_proc->cwd, path, VLOOKUP_NORMAL, current_proc->uid, &vnode)))
		return error;

	if (!S_ISDIR(vnode->mode))
		return ENOTDIR;

	current_proc->cwd = vfs_clone_vnode(vnode);
	return 0;
}

int do_access(const char *path, int mode)
{
	return vfs_access(current_proc->cwd, path, mode, current_proc->uid);
}

int do_chmod(const char *path, int mode)
{
	return vfs_chmod(current_proc->cwd, path, mode, current_proc->uid);
}

int do_chown(const char *path, uid_t owner, gid_t group)
{
	return vfs_chown(current_proc->cwd, path, owner, group, current_proc->uid);
}

int do_stat(const char *path, struct stat *statbuf)
{
	int error;
	struct vnode *vnode;

	if ((error = vfs_lookup(current_proc->cwd, path, VLOOKUP_NORMAL, current_proc->uid, &vnode)))
		return error;

	statbuf->st_dev = vnode->mp->dev;
	statbuf->st_mode = vnode->mode;
	statbuf->st_nlinks = vnode->nlinks;
	statbuf->st_uid = vnode->uid;
	statbuf->st_gid = vnode->gid;
	statbuf->st_rdev = vnode->rdev;
	statbuf->st_ino = vnode->ino;
	statbuf->st_size = vnode->size;
	statbuf->st_atime = vnode->atime;
	statbuf->st_mtime = vnode->mtime;
	statbuf->st_ctime = vnode->ctime;

	vfs_release_vnode(vnode);
	return 0;
}

int do_fstat(int fd, struct stat *statbuf)
{
	struct vfile *file;

	file = get_fd(current_proc->fd_table, fd);
	if (!file)
		return EBADF;

	statbuf->st_dev = 0;
	statbuf->st_mode = file->vnode->mode;
	statbuf->st_nlinks = file->vnode->nlinks;
	statbuf->st_uid = file->vnode->uid;
	statbuf->st_gid = file->vnode->gid;
	statbuf->st_rdev = file->vnode->rdev;
	statbuf->st_ino = file->vnode->ino;
	statbuf->st_size = file->vnode->size;
	statbuf->st_atime = file->vnode->atime;
	statbuf->st_mtime = file->vnode->mtime;
	statbuf->st_ctime = file->vnode->ctime;

	return 0;
}

#define PIPE_READ_FD	0
#define PIPE_WRITE_FD	1

int do_pipe(int pipefd[2])
{
	int error;
	struct vfile *file[2] = { NULL, NULL };

	if (!pipefd)
		return EINVAL;

	// Find two unused file descriptor slots in the current process's fd table (no need to free them until set_fd())
	pipefd[PIPE_READ_FD] = find_unused_fd(current_proc->fd_table);
	// TODO this is terrible and I shouldn't do this, but at least it works for the time being
	set_fd(current_proc->fd_table, pipefd[PIPE_READ_FD], (struct vfile *) 1);
	pipefd[PIPE_WRITE_FD] = find_unused_fd(current_proc->fd_table);
	set_fd(current_proc->fd_table, pipefd[PIPE_READ_FD], NULL);

	if (pipefd[PIPE_READ_FD] < 0 || pipefd[PIPE_WRITE_FD] < 0)
		return EMFILE;

	error = vfs_create_pipe(&file[PIPE_READ_FD], &file[PIPE_WRITE_FD]);
	if (error)
		return error;

	set_fd(current_proc->fd_table, pipefd[PIPE_READ_FD], file[PIPE_READ_FD]);
	set_fd(current_proc->fd_table, pipefd[PIPE_WRITE_FD], file[PIPE_WRITE_FD]);

	return 0;
}

int do_dup2(int oldfd, int newfd)
{
	struct vfile *fileptr, *exfileptr;

	fileptr = get_fd(current_proc->fd_table, oldfd);

	if (!fileptr)
		return EBADF;

	if (newfd >= OPEN_MAX)
		return EBADF;

	exfileptr = get_fd(current_proc->fd_table, newfd);
	if (exfileptr)
		vfs_close(exfileptr);

	dup_fd(current_proc->fd_table, newfd, fileptr);
	return 0;
}

mode_t do_umask(mode_t mask)
{
	mode_t previous;

	previous = current_proc->umask;
	current_proc->umask = mask & 0777;
	return previous;
}

int do_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	if (nfds <= 0 || nfds > OPEN_MAX)
		return EINVAL;

	extern int enter_select(struct process *proc, int max, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	return enter_select(current_proc, nfds, readfds, writefds, exceptfds, timeout);
}


time_t do_time(time_t *t)
{
	time_t current = get_system_time();
	if (t)
		*t = current;
	return current;
}

int do_stime(const time_t *t)
{
	if (current_proc->uid != SU_UID)
		return EPERM;
	set_system_time(*t);
	return 0;
}


int do_socket(int domain, int type, int protocol)
{
	int fd;
	int error;
	struct vfile *file = NULL;

	fd = find_unused_fd(current_proc->fd_table);
	if (fd < 0)
		return fd;

	error = net_socket_create(domain, type, protocol, current_proc->uid, &file);
	if (error)
		return error;

	set_fd(current_proc->fd_table, fd, file);

	return fd;
}

int do_socketpair(int domain, int type, int protocol, int fds[2])
{
	// TODO implement
	return -1;
}

int do_connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_connect(file, addr, len);
}

int do_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_bind(file, addr, len);
}

int do_listen(int fd, int n)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_listen(file, n);
}

int do_accept(int fd, struct sockaddr *addr, socklen_t *addr_len)
{
	int newfd;
	int error;
	struct vfile *file;
	struct vfile *newfile;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;

	newfd = find_unused_fd(current_proc->fd_table);
	if (newfd < 0)
		return newfd;

	error = net_socket_accept(file, addr, addr_len, current_proc->uid, &newfile);
	if (error)
		return error;

	set_fd(current_proc->fd_table, newfd, newfile);

	return newfd;
}

int do_shutdown(int fd, int how)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_shutdown(file, how);
}


ssize_t do_send(int fd, const void *buf, size_t n, int flags)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_send(file, buf, n, flags);
}

// Unistd Declaration:ssize_t do_sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len)
ssize_t do_sendto(int fd, const void *buf, size_t n, int flags, int opts[2])
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_sendto(file, buf, n, flags, (const struct sockaddr *) opts[0], (socklen_t) opts[1]);
}

ssize_t do_sendmsg(int fd, const struct msghdr *message, int flags)
{
	// TODO implement
	return -1;
}


ssize_t do_recv(int fd, void *buf, size_t n, int flags)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_recv(file, buf, n, flags);
}

// Unistd Declaration:ssize_t do_recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len)
ssize_t do_recvfrom(int fd, void *buf, size_t n, int flags, int opts[2])
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_recvfrom(file, buf, n, flags, (struct sockaddr *) opts[0], (socklen_t *) opts[1]);
}

ssize_t do_recvmsg(int fd, struct msghdr *message, int flags)
{
	// TODO implement
	return -1;
}

int do_getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_get_options(file, level, optname, optval, optlen);
}

int do_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode))
		return EBADF;
	return net_socket_set_options(file, level, optname, optval, optlen);
}

