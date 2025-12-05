
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>

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

int load_flat_binary(struct vfile *file, struct memory_map *map, void **entry);
int load_elf_binary(struct vfile *file, struct memory_map *map, void **entry);

int load_binary(const char *path, struct process *proc, const char *const argv[], const char *const envp[])
{
	int error;
	void *entry;
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

	error = load_elf_binary(file, map, &entry);
	// If the file was not a valid ELF binary, then execute it as a flat binary
	if (error == ENOEXEC) {
		error = vfs_seek(file, 0, SEEK_SET);
		if (!error) {
			error = load_flat_binary(file, map, &entry);
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

	// Initialize the stack pointer first, so that the check in memory_map_move_sbrk will pass
	exec_initialize_user_stack_with_args(proc, (char *) map->stack_end, entry, argv, envp);

	return error;
}

int load_flat_binary(struct vfile *file, struct memory_map *map, void **entry)
{
	size_t mem_size;
	int error = ENOMEM;
	uintptr_t user_mem_start;

	// The extra data is for the bss segment, which we don't know the proper size of
	mem_size = roundup(file->vnode->size + 0x200, PAGE_SIZE);

	#if defined(CONFIG_MMU)

	struct vfile *object = file;
	// TODO this is wrong, it should be some known address?
	user_mem_start = 0x40000;

	#else

	struct memory_region *object = NULL;
	object = memory_region_alloc_user_memory(mem_size, vfs_clone_fileptr(file));
	if (!object) {
		goto fail;
	}

	user_mem_start = memory_region_get_start_address(object);

	#endif

	error = memory_map_mmap(map, user_mem_start, mem_size, SEG_TYPE_CODE | SEG_EXECUTABLE, object, 0);
	if (error < 0) {
		goto fail;
	}
	// Since the region or file has now been added to the map, it will be freed when the map is freed
	// so we set the pointer to NULL again to avoid a double-free
	object = NULL;

	error = vfs_read(file, (void *) user_mem_start, mem_size);
	if (error <= 0) {
		goto fail;
	}

	*entry = (void *) user_mem_start;

	error = memory_map_insert_heap_stack(map, user_mem_start + mem_size, CONFIG_USER_STACK_SIZE);
	if (error < 0) {
		goto fail;
	}

	return 0;

fail:
	if (object) {
		MEMORY_OBJECT_FREE(object);
	}
	memory_map_free(map);
	return error;
}

#define PROG_HEADER_MAX	    6

int load_elf_binary(struct vfile *file, struct memory_map *map, void **entry)
{
	short num_ph;
	size_t mem_size;
	int error = ENOMEM;
	uintptr_t user_mem_start;
	uintptr_t memory_segment_start, memory_segment_end, file_segment_start, file_segment_end;
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
			mem_size += roundup(prog_headers[i].p_memsz, PAGE_SIZE);
		}
	}
	if (mem_size == 0)
		return ENOEXEC;

	#if defined(CONFIG_MMU)

	struct vfile *object = file;

	// TODO this is temporary until you get page faults working
	user_mem_start = (uintptr_t) page_alloc_contiguous(mem_size);
	if (!user_mem_start) {
		error = ENOMEM;
		goto fail;
	}

	// TODO this is wrong of course, but it's a placeholder for the time being
	// it should come from the elf?
	//user_mem_start = 0x40000;

	#else

	struct memory_region *object = NULL;
	object = memory_region_alloc_user_memory(mem_size, vfs_clone_fileptr(file));
	if (!object) {
		error = ENOMEM;
		goto fail;
	}

	user_mem_start = memory_region_get_start_address(object);

	#endif

	// Load all the program segments
	for (short i = 0; i < num_ph; i++) {
		if (prog_headers[i].p_type == PT_LOAD && prog_headers[i].p_filesz > 0) {
			file_segment_start = user_mem_start + prog_headers[i].p_vaddr;
			file_segment_end = file_segment_start + prog_headers[i].p_filesz;
			memory_segment_start = file_segment_start & ~(PAGE_SIZE - 1);
			memory_segment_end = memory_segment_start + roundup(prog_headers[i].p_memsz, PAGE_SIZE);

			if ((error = vfs_seek(file, prog_headers[i].p_offset, SEEK_SET)) < 0) {
				goto fail;
			}
			if ((error = vfs_read(file, (char *) file_segment_start, prog_headers[i].p_filesz)) < 0) {
				goto fail;
			}
			memset((char *) file_segment_end, '\0', prog_headers[i].p_memsz - prog_headers[i].p_filesz);

			int flags = 0;
			if (prog_headers[i].p_flags & PF_R) {
				flags |= SEG_READ;
			}
			if (prog_headers[i].p_flags & PF_W) {
				flags |= SEG_WRITE;
			}
			if (prog_headers[i].p_flags & PF_X) {
				flags |= SEG_EXECUTABLE | SEG_TYPE_CODE;
			}
			// If it's not a code area, then mark it as data
			if (!(flags & SEG_TYPE_CODE)) {
				flags |= SEG_TYPE_DATA;
			}

			if ((error = memory_map_mmap(map, memory_segment_start, memory_segment_end - memory_segment_start, flags, MEMORY_OBJECT_MAKE_REF(object), file_segment_start)) < 0) {
				goto fail;
			}
		} else if (prog_headers[i].p_type == PT_GNU_RELRO) {
			memory_segment_start = user_mem_start + prog_headers[i].p_vaddr;

			char **data = (char **) memory_segment_start;
			for (int entries = prog_headers[i].p_memsz >> 2; entries; entries--, data++) {
				*data = (char *) memory_segment_start + (size_t) *data;
			}
		}
	}

	*entry = (void *) user_mem_start + header.e_entry;

	#if !defined(CONFIG_MMU)
	// Free the extra reference that was used to create the individual segments
	memory_region_free(object);
	#endif

	error = memory_map_insert_heap_stack(map, roundup(user_mem_start + mem_size, PAGE_SIZE), CONFIG_USER_STACK_SIZE);
	if (error < 0) {
		goto fail;
	}

	return 0;

fail:
	#if !defined(CONFIG_MMU)
	if (object) {
		// Free the extra reference to the user memory area
		// NOTE: the caller will close the file and free the memory map
		memory_region_free(object);
	}
	#endif
	return error;
}
