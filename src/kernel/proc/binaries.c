
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include <kconfig.h>
#include <kernel/printk.h>
#include <kernel/fs/vfs.h>
#include <kernel/proc/exec.h>
#include <kernel/proc/memory.h>
#include <kernel/proc/process.h>

#include <elf.h>


#if defined(CONFIG_M68K)
#define ELF_MACHINE	EM_68K
#else
#error "Unable to load binaries for the target machine"
#endif

int load_flat_binary(struct vfile *file, struct process *proc, struct memory_map *map, const char *const argv[], const char *const envp[]);
int load_elf_binary(struct vfile *file, struct process *proc, struct memory_map *map, const char *const argv[], const char *const envp[]);

int load_binary(const char *path, struct process *proc, const char *const argv[], const char *const envp[])
{
	int error;
	struct vfile *file;
	struct memory_map *map = NULL;

	if (vfs_access(proc->cwd, path, X_OK, proc->uid))
		return EPERM;

	if ((error = vfs_open(proc->cwd, path, O_RDONLY, 0, proc->uid, &file)) < 0)
		return error;

	if (!S_ISREG(file->vnode->mode)) {
		vfs_close(file);
		return EISDIR;
	}

	// Allocate the process memory and initialize the memory maps
	map = memory_map_alloc();
	if (!map)
		return ENOMEM;

	error = load_elf_binary(file, proc, map, argv, envp);
	// If the file was not a valid ELF binary, then execute it as a flat binary
	if (error == ENOEXEC) {
		error = vfs_seek(file, 0, SEEK_SET);
		if (!error) {
			error = load_flat_binary(file, proc, map, argv, envp);
		}
	}

	// Close the file regardless of whether it succeeded or failed
	vfs_close(file);

	if (error < 0) {
		if (map)
			memory_map_free(map);
		return error;
	}

	// Swap the existing memory map for the newly created one
	memory_map_free(proc->map);
	proc->map = map;

	return error;
}

int load_flat_binary(struct vfile *file, struct process *proc, struct memory_map *map, const char *const argv[], const char *const envp[])
{
	void *entry;
	int mem_size;
	int error = ENOMEM;
	struct memory_object *object = NULL;

	// The extra data is for the bss segment, which we don't know the proper size of
	mem_size = file->vnode->size + 0x200;

	object = memory_object_alloc_user_memory(mem_size, vfs_clone_fileptr(file));
	if (!object) {
		goto fail;
	}

	error = memory_map_mmap(map, object->anonymous.address, mem_size, AREA_TYPE_CODE | AREA_EXECUTABLE, object);
	if (error < 0) {
		goto fail;
	}
	// Since the object has now been added to the map, it will be freed when the map is freed
	// so we set the pointer to NULL again to avoid a double-free
	object = NULL;

	error = vfs_read(file, (void *) object->anonymous.address, mem_size);
	if (error <= 0) {
		goto fail;
	}

	entry = (void *) object->anonymous.address;

	error = exec_alloc_new_stack(proc, map, CONFIG_USER_STACK_SIZE, entry, argv, envp);
	if (error < 0)
		goto fail;

	return 0;

fail:
	if (object)
		memory_object_free(object);
	memory_map_free(map);
	return error;
}

#define PROG_HEADER_MAX	    6

int load_elf_binary(struct vfile *file, struct process *proc, struct memory_map *map, const char *const argv[], const char *const envp[])
{
	void *entry;
	short num_ph;
	size_t mem_size;
	int error = ENOMEM;
	uintptr_t segment_start, segment_end;
	struct memory_object *object = NULL;
	Elf32_Ehdr header;
	Elf32_Phdr prog_headers[PROG_HEADER_MAX];

	if (!(error = vfs_read(file, (char *) &header, sizeof(Elf32_Ehdr))))
		return error;

	// Look for the ELF signature, 32-bit Big Endian ELF Version 1
	if (memcmp(header.e_ident, "\x7F\x45\x4C\x46\x01\x02\x01", 7))
		return ENOEXEC;

	// Make sure it's an executable for the the current machine
	if (header.e_type != ET_EXEC || header.e_machine != ELF_MACHINE || header.e_phentsize != sizeof(Elf32_Phdr))
		return ENOEXEC;

	// Load the program headers from the ELF file
	num_ph = header.e_phnum <= PROG_HEADER_MAX ? header.e_phnum : PROG_HEADER_MAX;
	if (!(error = vfs_seek(file, header.e_phoff, SEEK_SET)))
		return error;
	if (!(error = vfs_read(file, (char *) prog_headers, sizeof(Elf32_Phdr) * num_ph)))
		return error;

	// Calculate the total size of memory to allocate (not including the stack)
	mem_size = 0;
	for (short i = 0; i < num_ph; i++) {
		if (prog_headers[i].p_type == PT_LOAD) {
			// If we're not mapping addressing, and our start address is really high, we'd try to
			// to allocate all of memory if we didn't check.  This binary has accidentally been
			// compiled without a proper --entry start -Ttext=0x0000
			#if !defined(CONFIG_MMU)
			if (prog_headers[i].p_vaddr >= 0x80000000)
				return EFAULT;
			#endif

			if (prog_headers[i].p_vaddr > mem_size)
				mem_size = prog_headers[i].p_vaddr;
			mem_size += prog_headers[i].p_memsz;
		}
	}
	if (mem_size == 0)
		return ENOEXEC;

	object = memory_object_alloc_user_memory(mem_size, vfs_clone_fileptr(file));
	if (!object) {
		goto fail;
	}

	// Load all the program segments
	for (short i = 0; i < num_ph; i++) {
		if (prog_headers[i].p_type == PT_LOAD && prog_headers[i].p_filesz > 0) {
			segment_start = object->anonymous.address + prog_headers[i].p_vaddr;
			segment_end = object->anonymous.address + prog_headers[i].p_vaddr + prog_headers[i].p_memsz;

			if ((error = vfs_seek(file, prog_headers[i].p_offset, SEEK_SET)) < 0) {
				goto fail;
			}
			if ((error = vfs_read(file, (char *) segment_start, prog_headers[i].p_filesz)) < 0) {
				goto fail;
			}
			memset((char *) segment_start + prog_headers[i].p_filesz, '\0', prog_headers[i].p_memsz - prog_headers[i].p_filesz);

			int flags = 0;
			if (prog_headers[i].p_flags & PF_R) {
				flags |= AREA_READ;
			}
			if (prog_headers[i].p_flags & PF_W) {
				flags |= AREA_WRITE;
			}
			if (prog_headers[i].p_flags & PF_X) {
				flags |= AREA_EXECUTABLE | AREA_TYPE_CODE;
			}
			if (!(flags & AREA_TYPE_CODE)) {
				flags |= AREA_TYPE_DATA;
			}

			if ((error = memory_map_mmap(map, segment_start, prog_headers[i].p_memsz, flags, memory_object_make_ref(object))) < 0) {
				goto fail;
			}
		} else if (prog_headers[i].p_type == PT_GNU_RELRO) {
			segment_start = object->anonymous.address + prog_headers[i].p_vaddr;

			char **data = (char **) segment_start;
			for (int entries = prog_headers[i].p_memsz >> 2; entries; entries--, data++) {
				*data = (char *) segment_start + (size_t) *data;
			}
		}
	}

	entry = (void *) object->anonymous.address + header.e_entry;

	// Free the extra reference that was used to create the individual segments
	memory_object_free(object);

	error = exec_alloc_new_stack(proc, map, CONFIG_USER_STACK_SIZE, entry, argv, envp);
	if (error < 0)
		goto fail;

	return 0;

fail:
	if (object)
		memory_object_free(object);
	return error;
}
