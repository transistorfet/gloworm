
#ifndef _SRC_KERNEL_FS_EXT2_DIR_H
#define _SRC_KERNEL_FS_EXT2_DIR_H

#include <endian.h>
#include <kernel/fs/vfs.h>

#include "ext2.h"
#include "inodes.h"
#include "blocks.h"


void dir_set_filetype(struct ext2_dirent *dir, short mode)
{
	if (S_ISREG(mode)) {
		dir->filetype = EXT2_FT_REG_FILE;
	} else if (S_ISDIR(mode)) {
		dir->filetype = EXT2_FT_DIR;
	} else if (S_ISCHR(mode)) {
		dir->filetype = EXT2_FT_CHRDEV;
	} else if (S_ISBLK(mode)) {
		dir->filetype = EXT2_FT_BLKDEV;
	} else if (S_ISFIFO(mode)) {
		dir->filetype = EXT2_FT_FIFO;
	} else if (S_ISSOCK(mode)) {
		dir->filetype = EXT2_FT_SOCK;
	} else {
		dir->filetype = EXT2_FT_UNKNOWN;
	}
}

static struct vnode *dir_setup(struct vnode *vnode, struct vnode *parent)
{
	char *data;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;

	block = ext2_alloc_block(vnode->mp);
	buf = get_block(&vnode->mp->bufcache, block);
	if (!buf)
		return 0;
	data = buf->block;

	const uint16_t dot_entry_len = sizeof(struct ext2_dirent) + 4;
	current_entry = (struct ext2_dirent *) data;
	current_entry->inode = htole32((ext2_inode_t) vnode->ino);
	current_entry->filetype = EXT2_FT_DIR;
	current_entry->entry_len = htole16(dot_entry_len);
	current_entry->name_len = 1;
	memcpy(EXT2_DIRENT_FILENAME(current_entry), ".", current_entry->name_len);

	current_entry = (struct ext2_dirent *) &data[dot_entry_len];
	current_entry->inode = htole32(parent ? (ext2_inode_t) parent->ino : 1);
	current_entry->filetype = EXT2_FT_DIR;
	current_entry->entry_len = htole16(vnode->mp->block_size - dot_entry_len);
	current_entry->name_len = 2;
	memcpy(EXT2_DIRENT_FILENAME(current_entry), "..", current_entry->name_len);

	release_block(buf, BF_DIRTY);

	EXT2_DATA(vnode).blocks[0] = htole32(block);
	// Set the initial directory size, which is always one whole block
	vnode->size = vnode->mp->block_size;
	mark_vnode_dirty(vnode);

	return vnode;
}

static short dir_is_empty(struct vnode *vnode)
{
	char *data;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;
	const int block_size = vnode->mp->block_size;

	for (block_t znum = 0; (block = block_lookup(vnode, znum, EXT2_AF_LOOKUP_BLOCK)) != 0; znum++) {
		buf = get_block(&vnode->mp->bufcache, block);
		if (!buf)
			return EIO;
		data = buf->block;
		for (int i = 0; i < block_size; i += le16toh(current_entry->entry_len)) {
			current_entry = (struct ext2_dirent *) &data[i];
			if (current_entry->inode
			    && !(current_entry->name_len == 1 && !strcmp(EXT2_DIRENT_FILENAME(current_entry), "."))
			    && !(current_entry->name_len == 2 && !strcmp(EXT2_DIRENT_FILENAME(current_entry), ".."))) {
				release_block(buf, 0);
				return 0;
			}
		}
		release_block(buf, 0);
	}
	return 1;
}

static struct ext2_dirent *dir_find_entry_by_name(struct vnode *dir, const char *filename, char create, struct buf **result)
{
	char *data;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;
	const int block_size = dir->mp->block_size;
	const uint8_t filename_len = strlen(filename);

	for (block_t znum = 0; (block = block_lookup(dir, znum, create)) != 0; znum++) {
		buf = get_block(&dir->mp->bufcache, block);
		if (!buf)
			return NULL;
		data = buf->block;
		for (int i = 0; i < block_size; i += le16toh(current_entry->entry_len)) {
			current_entry = (struct ext2_dirent *) &data[i];
			if (current_entry->inode && filename_len == current_entry->name_len && !memcmp(filename, EXT2_DIRENT_FILENAME(current_entry), current_entry->name_len)) {
				*result = buf;
				return current_entry;
			}
		}
		release_block(buf, 0);
	}
	return NULL;
}

static struct ext2_dirent *dir_find_empty_entry(struct vnode *dir, uint16_t filename_len, struct buf **result)
{
	char *data;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;
	const int block_size = dir->mp->block_size;
	const short new_entry_len = roundup(sizeof(struct ext2_dirent) + filename_len, 4);

	for (block_t znum = 0; (block = block_lookup(dir, znum, EXT2_AF_CREATE_BLOCK)) != 0; znum++) {
		buf = get_block(&dir->mp->bufcache, block);
		if (!buf)
			return NULL;
		data = buf->block;
		for (int i = 0; i < block_size; i += le16toh(current_entry->entry_len)) {
			current_entry = (struct ext2_dirent *) &data[i];
			if (current_entry->entry_len == 0) {
				// A new block was created (initialized to 0s) so set the size
				current_entry->entry_len = htole16(block_size);

				if (dir->size < i) {
					dir->size = i;
					mark_vnode_dirty(dir);
				}

				*result = buf;
				return current_entry;
			}

			const uint16_t entry_len = le16toh(current_entry->entry_len);
			const uint16_t entry_min_len = roundup(sizeof(struct ext2_dirent) + current_entry->name_len, 4);
			if (entry_len >= entry_min_len + new_entry_len) {
				// Split the directory entry in two
				current_entry->entry_len = htole16(entry_min_len);
				current_entry = (struct ext2_dirent *) &data[i + entry_min_len];
				current_entry->entry_len = htole16(entry_len - entry_min_len);

				*result = buf;
				return current_entry;
			}
		}
		release_block(buf, 0);
	}
	return NULL;
}

static struct ext2_dirent *dir_alloc_entry(struct vnode *vnode, const char *filename, struct buf **result)
{
	char *entry_filename;
	struct ext2_dirent *dir;
	const short filename_len = strlen(filename);

	dir = dir_find_empty_entry(vnode, filename_len, result);
	if (!dir)
		return NULL;

	entry_filename = EXT2_DIRENT_FILENAME(dir);
	memcpy(entry_filename, filename, filename_len);
	entry_filename[filename_len] = '\0';
	dir->name_len = filename_len;

	return dir;
}

static int dir_free_entry(struct vnode *parent, struct vnode *vnode, const char *filename)
{
	char *data;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *cur, *prev;
	const int block_size = parent->mp->block_size;
	const uint8_t filename_len = strlen(filename);

	for (block_t znum = 0; (block = block_lookup(parent, znum, EXT2_AF_LOOKUP_BLOCK)) != 0; znum++) {
		buf = get_block(&parent->mp->bufcache, block);
		if (!buf)
			return ENOENT;
		data = buf->block;
		prev = NULL;
		for (int i = 0; i < block_size; i += le16toh(cur->entry_len)) {
			cur = (struct ext2_dirent *) &data[i];
			if (cur->inode && filename_len == cur->name_len && !memcmp(filename, EXT2_DIRENT_FILENAME(cur), cur->name_len)) {
				cur->inode = 0;
				if (prev) {
					// If there's a previous entry, merge the deleted entry into that one
					prev->entry_len += cur->entry_len;
				} else if (i + cur->entry_len == block_size && vnode->size == (znum + 1) * block_size) {
					// If it's both the first entry in the block, and the last block in the file, then delete the block
					// TODO how do you actually truncate that block?
					vnode->size = znum;
					mark_vnode_dirty(vnode);
				} else {
					// If it's the first entry in the block, but there are other entries after it
					// TODO then what?  Do I have to move the previous entry?
				}

				release_block(buf, BF_DIRTY);
				return 0;
			}
			prev = cur;
		}
		release_block(buf, 0);
	}
	return ENOENT;
}

#endif
