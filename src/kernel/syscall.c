
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
#include <kernel/utils/math.h>
#include <kernel/utils/iovec.h>
#include <kernel/utils/usercopy.h>
#include <kernel/utils/strarray.h>


#include <asm/syscall_table.h>

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
	tty_68681_set_leds(0x01);
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
	tty_68681_reset_leds(0x01);
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
}

pid_t do_fork(void)
{
	int error;
	struct process *proc;

	struct clone_args args = {
		.stack = NULL,
		.flags = 0,
	};

	error = clone_process(current_proc, &args, &proc);
	if (error < 0) {
		return error;
	}

	return proc->pid;
}

/// Clone a process (more precise than fork())
///
/// NOTE: this syscall may have an alternate entry to save the full context on
/// the stack before cloning it.  Because of that, this function uses the
/// `current_syscall` struct to access the arguments.
//	Actual Args: void *stack, int flags
pid_t do_clone(void)
{
	int error;
	struct process *proc;
	struct clone_args args;

	// TODO does the stack need to be translated?
	args.stack = (void *) current_syscall->arg1;
	args.flags = current_syscall->arg2;

	error = clone_process(current_proc, &args, &proc);
	if (error < 0) {
		return error;
	}

	return proc->pid;
}

int do_exec(const char __user *path, const char __user *const argv[], const char __user *const envp[])
{
	int error;
	struct string_array argv_buffer;
	struct string_array envp_buffer;

	COPY_USER_STRING(path, CONFIG_PATH_MAX);

	string_array_copy(&argv_buffer, argv, FROM_USER);
	string_array_copy(&envp_buffer, envp, FROM_USER);

	error = load_binary(path, current_proc, &argv_buffer, &envp_buffer);
	if (error == EKILL) {
		// An error occurred past the point of no return.  The memory maps have been irrepairably damaged, so kill the process
		log_error("Process terminated\n");
		exit_proc(current_proc, -1);
		return error;
	} else if (error) {
		return error;
	}

	return 0;
}

pid_t do_waitpid(pid_t pid, int __user *status, int options)
{
	struct process *proc;

	// Must be a valid pid, or -1 to wait for any process
	if (pid <= 0 && pid != -1) {
		return EINVAL;
	}

	// TODO should this wake up all parent procesess instead of just one?
	proc = find_exited_child(current_proc->pid, pid);
	if (!proc) {
		current_proc->bits |= PB_WAITING;
		suspend_current_syscall(0);
	} else {
		current_proc->bits &= ~PB_WAITING;
		put_user_uintptr(status, proc->exitcode);
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

int do_sigaction(int signum, const struct sigaction __user *act, struct sigaction __user *oldact)
{
	struct sigaction kernel_oldact;

	if (oldact) {
		get_signal_action(current_proc, signum, &kernel_oldact);
		memcpy_to_user(oldact, &kernel_oldact, sizeof(struct sigaction));
	}

	COPY_USER_STRUCT(struct sigaction, act);
	if (act) {
		return set_signal_action(current_proc, signum, act);
	}
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

int do_brk(void __user *addr)
{
	int diff;

	// Round up to the nearest page size
	addr = (void *) roundup((uintptr_t) addr, PAGE_SIZE);

	// TODO fix the address translation??  Maybe this isn't needed
	diff = (uintptr_t) addr - current_proc->map->sbrk;
	if (current_proc->map->heap_start + diff >= (uintptr_t) arch_get_user_stackp(current_proc)) {
		return ENOMEM;
	}

	return memory_map_move_sbrk(current_proc->map, diff);
}

void *do_sbrk(intptr_t diff)
{
	if (diff) {
		// Round up to the nearest page size
		diff = roundup(diff, PAGE_SIZE);

		if (current_proc->map->heap_start + diff >= (uintptr_t) arch_get_user_stackp(current_proc)) {
			return NULL;
		}

		if (memory_map_move_sbrk(current_proc->map, diff)) {
			return NULL;
		}
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

	if (!proc) {
		return ESRCH;
	}
	return proc->pgid;
}

int do_setpgid(pid_t pid, pid_t pgid)
{
	struct process *proc = pid ? get_proc(pid) : current_proc;

	if (pgid < 0) {
		return EINVAL;
	}
	if (!proc || (proc != current_proc && proc->pid != current_proc->parent)) {
		return ESRCH;
	}

	if (pgid == 0) {
		proc->pgid = proc->pid;
	} else {
		struct process *pg;

		pg = get_proc(pgid);
		if (!pg || pg->session != proc->session || proc->pid == proc->session) {
			return EPERM;
		}
		proc->pgid = pgid;
	}
	return 0;
}

pid_t do_getsid(pid_t pid)
{
	struct process *proc = pid ? get_proc(pid) : current_proc;

	if (!proc) {
		return ESRCH;
	}
	return current_proc->session;
}

pid_t do_setsid(void)
{
	struct process *proc;
	struct process_iter iter;

	proc_iter_start(&iter);
	while ((proc = proc_iter_next(&iter))) {
		if (proc != current_proc && proc->pgid == current_proc->pid) {
			return EPERM;
		}
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
	if (current_proc->uid != SU_UID) {
		return EPERM;
	}
	current_proc->uid = uid;
	return 0;
}



int do_mount(const char __user *source, const char __user *target, struct mount_opts __user *opts)
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
		if (!fsptr) {
			return EINVAL;
		}
	}
	#if defined(CONFIG_MINIX_FS)
	// Set the default file system if none was given
	if (!fsptr) {
		fsptr = &minix_mount_ops;
	}
	#endif

	COPY_USER_STRING(source, CONFIG_PATH_MAX);
	COPY_USER_STRING(target, CONFIG_PATH_MAX);
	COPY_USER_STRUCT(struct mount_opts, opts);
	if (vfs_lookup(current_proc->cwd, source, VLOOKUP_NORMAL, current_proc->uid, &vnode)) {
		return ENOENT;
	}
	return vfs_mount(current_proc->cwd, target, vnode->rdev, fsptr, opts ? opts->mountflags : 0, current_proc->uid);
}

int do_umount(const char __user *source)
{
	struct vnode *vnode;

	COPY_USER_STRING(source, CONFIG_PATH_MAX);
	if (vfs_lookup(current_proc->cwd, source, VLOOKUP_NORMAL, current_proc->uid, &vnode)) {
		return ENOENT;
	}

	return vfs_unmount(vnode->rdev, current_proc->uid);
}

int do_sync(void)
{
	return vfs_sync(0);
}

int do_mknod(const char __user *path, mode_t mode, device_t dev)
{
	int error;
	struct vnode *vnode;

	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	error = vfs_mknod(current_proc->cwd, path, mode, dev, current_proc->uid, &vnode);
	if (error) {
		return error;
	}
	vfs_release_vnode(vnode);
	return 0;
}

int do_link(const char __user *oldpath, const char __user *newpath)
{
	COPY_USER_STRING(oldpath, CONFIG_PATH_MAX);
	COPY_USER_STRING(newpath, CONFIG_PATH_MAX);
	return vfs_link(current_proc->cwd, oldpath, newpath, current_proc->uid);
}

int do_unlink(const char __user *path)
{
	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	return vfs_unlink(current_proc->cwd, path, current_proc->uid);
}

int do_rename(const char __user *oldpath, const char __user *newpath)
{
	COPY_USER_STRING(oldpath, CONFIG_PATH_MAX);
	COPY_USER_STRING(newpath, CONFIG_PATH_MAX);
	return vfs_rename(current_proc->cwd, oldpath, newpath, current_proc->uid);
}

int do_mkdir(const char __user *path, mode_t mode)
{
	int error;
	struct vfile *file = NULL;

	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	mode = mode & ~current_proc->umask & 0777;
	error = vfs_open(current_proc->cwd, path, O_CREAT, S_IFDIR | mode, current_proc->uid, &file);
	if (error < 0) {
		return error;
	}
	vfs_close(file);
	return 0;
}

char *do_getcwd(char __user *buf, size_t size)
{
	char buffer[CONFIG_PATH_MAX];

	if (vfs_reverse_lookup(current_proc->cwd, buffer, size, current_proc->uid)) {
		return NULL;
	}
	memcpy_to_user(buf, buffer, size);
	return buf;
}

int do_creat(const char __user *path, mode_t mode)
{
	return do_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int do_open(const char __user *path, int oflags, mode_t mode)
{
	int fd;
	int error;
	struct vfile *file = NULL;

	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	fd = find_unused_fd(current_proc->fd_table);
	if (fd < 0) {
		return fd;
	}

	mode = mode & ~current_proc->umask & 0777;
	error = vfs_open(current_proc->cwd, path, oflags, mode, current_proc->uid, &file);
	if (error) {
		return error;
	}

	set_fd(current_proc->fd_table, fd, file);

	return fd;
}

int do_close(int fd)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file) {
		return EBADF;
	}

	vfs_close(file);

	unset_fd(current_proc->fd_table, fd);
	//free_fd(current_proc->fd_table, fd);
	return 0;
}

size_t do_read(int fd, char __user *buf, size_t nbytes)
{
	struct vfile *file;
	struct iovec_iter iter;

	file = get_fd(current_proc->fd_table, fd);
	if (!file) {
		return EBADF;
	}
	iovec_iter_init_user_buf(&iter, buf, nbytes);
	return vfs_read(file, &iter);
}


size_t do_write(int fd, const char __user *buf, size_t nbytes)
{
	struct vfile *file;
	struct iovec_iter iter;

	file = get_fd(current_proc->fd_table, fd);
	if (!file) {
		return EBADF;
	}
	iovec_iter_init_user_buf(&iter, (char *) buf, nbytes);
	return vfs_write(file, &iter);
}

int do_lseek(int fd, offset_t offset, int whence)
{
	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file) {
		return EBADF;
	}
	return vfs_seek(file, offset, whence);
}

int do_readdir(int fd, struct dirent __user *dir)
{
	int result;
	struct dirent dir_kernel;

	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file) {
		return EBADF;
	}
	result = vfs_readdir(file, &dir_kernel);
	if (result < 0) {
		return result;
	}
	memcpy_to_user(dir, &dir_kernel, sizeof(struct dirent));
	return result;
}

int do_ioctl(int fd, unsigned int request, void __user *argp)
{
	struct iovec_iter iter;

	struct vfile *file = get_fd(current_proc->fd_table, fd);
	if (!file) {
		return EBADF;
	}
	iovec_iter_init_user_buf(&iter, argp, 256);
	return vfs_ioctl(file, request, &iter, current_proc->uid);
}

extern int do_fcntl(int fd, int cmd, void __user *argp)
{
	// TODO what are the arguments it can take?
	return -1;
}

int do_chdir(const char __user *path)
{
	int error;
	struct vnode *vnode;

	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	if ((error = vfs_lookup(current_proc->cwd, path, VLOOKUP_NORMAL, current_proc->uid, &vnode))) {
		return error;
	}

	if (!S_ISDIR(vnode->mode)) {
		return ENOTDIR;
	}

	current_proc->cwd = vfs_clone_vnode(vnode);
	return 0;
}

int do_access(const char __user *path, int mode)
{
	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	return vfs_access(current_proc->cwd, path, mode, current_proc->uid);
}

int do_chmod(const char __user *path, int mode)
{
	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	return vfs_chmod(current_proc->cwd, path, mode, current_proc->uid);
}

int do_chown(const char __user *path, uid_t owner, gid_t group)
{
	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	return vfs_chown(current_proc->cwd, path, owner, group, current_proc->uid);
}

int do_stat(const char __user *path, struct stat __user *statbuf)
{
	int error;
	struct vnode *vnode;
	struct stat kernel_statbuf;

	COPY_USER_STRING(path, CONFIG_PATH_MAX);
	if ((error = vfs_lookup(current_proc->cwd, path, VLOOKUP_NORMAL, current_proc->uid, &vnode))) {
		return error;
	}

	kernel_statbuf.st_dev = vnode->mp->dev;
	kernel_statbuf.st_mode = vnode->mode;
	kernel_statbuf.st_nlinks = vnode->nlinks;
	kernel_statbuf.st_uid = vnode->uid;
	kernel_statbuf.st_gid = vnode->gid;
	kernel_statbuf.st_rdev = vnode->rdev;
	kernel_statbuf.st_ino = vnode->ino;
	kernel_statbuf.st_size = vnode->size;
	kernel_statbuf.st_atime = vnode->atime;
	kernel_statbuf.st_mtime = vnode->mtime;
	kernel_statbuf.st_ctime = vnode->ctime;

	memcpy_to_user(statbuf, &kernel_statbuf, sizeof(struct stat));

	vfs_release_vnode(vnode);
	return 0;
}

int do_fstat(int fd, struct stat __user *statbuf)
{
	struct vfile *file;
	struct stat kernel_statbuf;

	file = get_fd(current_proc->fd_table, fd);
	if (!file) {
		return EBADF;
	}

	kernel_statbuf.st_dev = 0;
	kernel_statbuf.st_mode = file->vnode->mode;
	kernel_statbuf.st_nlinks = file->vnode->nlinks;
	kernel_statbuf.st_uid = file->vnode->uid;
	kernel_statbuf.st_gid = file->vnode->gid;
	kernel_statbuf.st_rdev = file->vnode->rdev;
	kernel_statbuf.st_ino = file->vnode->ino;
	kernel_statbuf.st_size = file->vnode->size;
	kernel_statbuf.st_atime = file->vnode->atime;
	kernel_statbuf.st_mtime = file->vnode->mtime;
	kernel_statbuf.st_ctime = file->vnode->ctime;

	memcpy_to_user(statbuf, &kernel_statbuf, sizeof(struct stat));

	return 0;
}

#define PIPE_READ_FD	0
#define PIPE_WRITE_FD	1

int do_pipe(int pipefd[2])
{
	int error;
	int kernel_pipefd[2];
	struct vfile *file[2] = { NULL, NULL };

	if (!pipefd) {
		return EINVAL;
	}

	// Find two unused file descriptor slots in the current process's fd table (no need to free them until set_fd())
	kernel_pipefd[PIPE_READ_FD] = find_unused_fd(current_proc->fd_table);
	// TODO this is terrible and I shouldn't do this, but at least it works for the time being
	// the reason for this hack is because without it, the same fd slot will be found by find_usused_fd, so
	// temporarily setting it to 1 will bypass this and give a new slot
	set_fd(current_proc->fd_table, kernel_pipefd[PIPE_READ_FD], (struct vfile *) 1);
	kernel_pipefd[PIPE_WRITE_FD] = find_unused_fd(current_proc->fd_table);
	set_fd(current_proc->fd_table, kernel_pipefd[PIPE_READ_FD], NULL);
	// TODO part of the hack, we write NULL in case there's an error and we exit early
	set_fd(current_proc->fd_table, kernel_pipefd[PIPE_WRITE_FD], NULL);

	if (kernel_pipefd[PIPE_READ_FD] < 0 || kernel_pipefd[PIPE_WRITE_FD] < 0) {
		return EMFILE;
	}

	error = vfs_create_pipe(&file[PIPE_READ_FD], &file[PIPE_WRITE_FD]);
	if (error) {
		return error;
	}

	set_fd(current_proc->fd_table, kernel_pipefd[PIPE_READ_FD], file[PIPE_READ_FD]);
	set_fd(current_proc->fd_table, kernel_pipefd[PIPE_WRITE_FD], file[PIPE_WRITE_FD]);

	put_user_uintptr(&pipefd[PIPE_READ_FD], kernel_pipefd[PIPE_READ_FD]);
	put_user_uintptr(&pipefd[PIPE_WRITE_FD], kernel_pipefd[PIPE_WRITE_FD]);

	return 0;
}

int do_dup2(int oldfd, int newfd)
{
	struct vfile *fileptr, *exfileptr;

	fileptr = get_fd(current_proc->fd_table, oldfd);

	if (!fileptr) {
		return EBADF;
	}

	if (newfd >= OPEN_MAX) {
		return EBADF;
	}

	exfileptr = get_fd(current_proc->fd_table, newfd);
	if (exfileptr) {
		vfs_close(exfileptr);
	}

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

int do_select(int nfds, fd_set __user *readfds, fd_set __user *writefds, fd_set __user *exceptfds, struct timeval __user *timeout)
{
	int error;

	if (nfds <= 0 || nfds > OPEN_MAX) {
		return EINVAL;
	}

	extern int enter_select(struct process *proc, int max, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

	#if defined(CONFIG_MMU)

	fd_set *ptr_readfds, *ptr_writefds, *ptr_exceptfds;
	fd_set kernel_readfds, kernel_writefds, kernel_exceptfds;

	if (readfds) {
		memcpy_from_user(&kernel_readfds, readfds, sizeof(fd_set));
		ptr_readfds = &kernel_readfds;
	} else {
		ptr_readfds = NULL;
	}

	if (writefds) {
		memcpy_from_user(&kernel_writefds, writefds, sizeof(fd_set));
		ptr_writefds = &kernel_writefds;
	} else {
		ptr_writefds = NULL;
	}

	if (exceptfds) {
		memcpy_from_user(&kernel_exceptfds, exceptfds, sizeof(fd_set));
		ptr_exceptfds = &kernel_exceptfds;
	} else {
		ptr_exceptfds = NULL;
	}

	COPY_USER_STRUCT(struct timeval, timeout);
	error = enter_select(current_proc, nfds, ptr_readfds, ptr_writefds, ptr_exceptfds, timeout);

	if (readfds) {
		memcpy_to_user(readfds, &kernel_readfds, sizeof(fd_set));
	}
	if (writefds) {
		memcpy_to_user(writefds, &kernel_writefds, sizeof(fd_set));
	}
	if (exceptfds) {
		memcpy_to_user(exceptfds, &kernel_exceptfds, sizeof(fd_set));
	}

	#else

	error = enter_select(current_proc, nfds, readfds, writefds, exceptfds, timeout);

	#endif

	return error;
}


time_t do_time(time_t __user *t)
{
	time_t current = get_system_time();
	if (t) {
		put_user_uintptr(t, current);
	}
	return current;
}

int do_stime(const time_t __user *t)
{
	if (current_proc->uid != SU_UID) {
		return EPERM;
	}
	set_system_time(get_user_uintptr(t));
	return 0;
}

#if defined(CONFIG_NET)
int do_socket(int domain, int type, int protocol)
{
	int fd;
	int error;
	struct vfile *file = NULL;

	fd = find_unused_fd(current_proc->fd_table);
	if (fd < 0) {
		return fd;
	}

	error = net_socket_create(domain, type, protocol, current_proc->uid, &file);
	if (error) {
		return error;
	}

	set_fd(current_proc->fd_table, fd, file);

	return fd;
}

int do_socketpair(int domain, int type, int protocol, int fds[2])
{
	// TODO implement
	return -1;
}

int do_connect(int fd, const struct sockaddr __user *addr, socklen_t len)
{
	struct vfile *file;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}
	COPY_USER_STRUCT(struct sockaddr, addr);
	return net_socket_connect(file, addr, len);
}

int do_bind(int fd, const struct sockaddr __user *addr, socklen_t len)
{
	struct vfile *file;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}
	COPY_USER_STRUCT(struct sockaddr, addr);
	return net_socket_bind(file, addr, len);
}

int do_listen(int fd, int n)
{
	struct vfile *file;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}
	return net_socket_listen(file, n);
}

int do_accept(int fd, struct sockaddr __user *addr, socklen_t __user *addr_len)
{
	int newfd;
	int error;
	socklen_t length;
	char *buffer[256];
	struct vfile *file;
	struct vfile *newfile;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}

	newfd = find_unused_fd(current_proc->fd_table);
	if (newfd < 0) {
		return newfd;
	}

	length = get_user_uintptr(addr_len);
	if (length > 256)
		length = 256;
	error = net_socket_accept(file, (struct sockaddr *) buffer, &length, current_proc->uid, &newfile);
	if (error) {
		return error;
	}

	memcpy_to_user(addr, buffer, length);
	put_user_uintptr(addr_len, length);

	set_fd(current_proc->fd_table, newfd, newfile);

	return newfd;
}

int do_shutdown(int fd, int how)
{
	struct vfile *file;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}
	return net_socket_shutdown(file, how);
}


ssize_t do_send(int fd, const void __user *buf, size_t nbytes, int flags)
{
	struct vfile *file;
	struct iovec_iter iter;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}
	iovec_iter_init_user_buf(&iter, (char *) buf, nbytes);
	return net_socket_send(file, &iter, flags);
}

// Unistd Declaration:ssize_t do_sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len)
ssize_t do_sendto(int fd, const void __user *buf, size_t nbytes, int flags, int opts[2])
{
	socklen_t addr_len;
	struct vfile *file;
	struct iovec_iter iter;
	const struct sockaddr *addr;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}

	addr = (const struct sockaddr *) get_user_uintptr(&opts[0]);
	addr_len = (socklen_t) get_user_uintptr(&opts[1]);

	#if defined(CONFIG_MMU)
	if (addr_len > 256)
		addr_len = 256;
	char addr_buffer[256];
	memcpy_from_user(addr_buffer, addr, addr_len);
	addr = (const struct sockaddr *) addr_buffer;
	#endif

	iovec_iter_init_user_buf(&iter, (char *) buf, nbytes);
	return net_socket_sendto(file, &iter, flags, addr, addr_len);
}

ssize_t do_sendmsg(int fd, const struct msghdr __user *message, int flags)
{
	// TODO implement
	return -1;
}


ssize_t do_recv(int fd, void __user *buf, size_t nbytes, int flags)
{
	struct vfile *file;
	struct iovec_iter iter;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}
	iovec_iter_init_user_buf(&iter, (char *) buf, nbytes);
	return net_socket_recv(file, &iter, flags);
}

// Unistd Declaration:ssize_t do_recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr *addr, socklen_t *addr_len)
ssize_t do_recvfrom(int fd, void __user *buf, size_t nbytes, int flags, int opts[2])
{
	int error;
	struct vfile *file;
	socklen_t *addr_len;
	struct sockaddr *addr;
	struct iovec_iter iter;
	#if defined(CONFIG_MMU)
	socklen_t kernel_len = 0;
	#endif

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}

	addr = (struct sockaddr *) get_user_uintptr(&opts[0]);
	addr_len = (socklen_t *) get_user_uintptr(&opts[1]);

	#if defined(CONFIG_MMU)

	char addr_buffer[256];
	if (addr_len) {
		kernel_len = get_user_uintptr(addr_len);
		if (kernel_len > 256) {
			return EINVAL;
		}
	}

	iovec_iter_init_user_buf(&iter, (char *) buf, nbytes);
	error = net_socket_recvfrom(file, &iter, flags, addr ? (struct sockaddr *) addr_buffer : NULL, &kernel_len);
	if (error < 0)
		return error;

	if (addr) {
		memcpy_to_user((char *) addr, addr_buffer, kernel_len);
	}
	if (addr_len) {
		put_user_uintptr((socklen_t *) addr_len, kernel_len);
	}

	#else

	error = net_socket_recvfrom(file, &iter, flags, addr, addr_len);
	if (error < 0)
		return error;

	#endif
	return error;
}

ssize_t do_recvmsg(int fd, struct msghdr __user *message, int flags)
{
	// TODO implement
	return -1;
}

int do_getsockopt(int fd, int level, int optname, void __user *optval, socklen_t __user *optlen)
{
	int error;
	socklen_t length;
	char buffer[256];
	struct vfile *file;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}

	length = get_user_uintptr(optlen);
	if (length > 256)
		length = 256;
	error = net_socket_get_options(file, level, optname, buffer, &length);
	if (error < 0)
		return error;

	memcpy_to_user(optval, buffer, length);
	put_user_uintptr(optlen, length);
	return error;
}

int do_setsockopt(int fd, int level, int optname, const void __user *optval, socklen_t optlen)
{
	char buffer[256];
	struct vfile *file;

	file = get_fd(current_proc->fd_table, fd);
	if (!file || !S_ISSOCK(file->vnode->mode)) {
		return EBADF;
	}
	memcpy_from_user(buffer, optval, optlen < 256 ? optlen : 256);
	return net_socket_set_options(file, level, optname, buffer, optlen);
}
#endif

