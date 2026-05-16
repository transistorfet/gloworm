
#ifndef _SRC_KERNEL_FS_EXT2_DIR_H
#define _SRC_KERNEL_FS_EXT2_DIR_H

#include <endian.h>
#include <kernel/fs/vfs.h>

#include "ext2.h"
#include "inodes.h"
#include "blocks.h"


/*
static struct vnode *dir_setup(struct vnode *vnode, struct vnode *parent)
{
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;

	block = ext2_alloc_block(vnode->mp->super);
	buf = get_block(&vnode->mp->bufcache, block);
	if (!buf)
		return 0;

	current_entry = (struct ext2_dirent *) buf->block;
	current_entry->inode = htole32((ext2_inode_t) vnode->ino);
	current_entry->entry_len = htole16(sizeof(struct ext2_dirent) + 4);
	current_entry->name_len = 1;
	strcpy((char *) (current_entry + 1), ".");

	current_entry = (struct ext2_dirent *) (((char *) current_entry) + current_entry->entry_len);
	current_entry->inode = htole32(parent ? (ext2_inode_t) parent->ino : 1);
	current_entry->entry_len = htole16(sizeof(struct ext2_dirent) + 4);
	current_entry->name_len = 2;
	strcpy((char *) (current_entry + 1), "..");

	release_block(buf, BCF_DIRTY);

	EXT2_DATA(vnode).blocks[0] = htole32(block);
	// Set the initial directory size to include the two default entries
	vnode->size = sizeof(struct ext2_dirent) << 1;
	mark_vnode_dirty(vnode);

	return vnode;
}

static short dir_is_empty(struct vnode *vnode)
{
	char *data;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;
	int block_size = vnode->mp->block_size;

	for (block_t znum = 0; (block = block_lookup(vnode, znum, MFS_LOOKUP_BLOCK)) != 0; znum++) {
		buf = get_block(&vnode->mp->bufcache, block);
		if (!buf)
			return EIO;
		data = buf->block;
		for (int i = 0; i < block_size; i += le16toh(current_entry->entry_len)) {
			current_entry = (struct ext2_dirent *) &data[i];
			if (current_entry->inode && strcmp(EXT2_DIRENT_FILENAME(current_entry), ".") && strcmp(EXT2_DIRENT_FILENAME(current_entry), "..")) {
				release_block(buf, 0);
				return 0;
			}
		}
		release_block(buf, 0);
	}
	return 1;
}

static struct ext2_dirent *dir_find_entry_by_inode(struct vnode *dir, inode_t ino, char create, struct buf **result)
{
	char *data;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;
	int block_size = dir->mp->block_size;

	for (block_t znum = 0; (block = block_lookup(dir, znum, create)) != 0; znum++) {
		buf = get_block(&dir->mp->bufcache, block);
		if (!buf)
			return NULL;
		data = buf->block;
		for (int i = 0; i < block_size; i += le16toh(current_entry->entry_len)) {
			current_entry = (struct ext2_dirent *) &data[i];
			if (le32toh(current_entry->inode) == ino) {
				if (dir->size < i) {
					// TODO this is sort of a hack, isn't it?  Also what about shrinking in size
					dir->size = i;
					mark_vnode_dirty(dir);
				}
				*result = buf;
				return current_entry;
			}
		}
		release_block(buf, 0);
	}
	return NULL;
}
*/

static struct ext2_dirent *dir_find_entry_by_name(struct vnode *dir, const char *filename, char create, struct buf **result)
{
	char *data;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;
	int block_size = dir->mp->block_size;

	for (block_t znum = 0; (block = block_lookup(dir, znum, create)) != 0; znum++) {
		buf = get_block(&dir->mp->bufcache, block);
		if (!buf)
			return NULL;
		data = buf->block;
		for (int i = 0; i < block_size; i += le16toh(current_entry->entry_len)) {
			current_entry = (struct ext2_dirent *) &data[i];
			if (current_entry->inode && !strcmp(filename, EXT2_DIRENT_FILENAME(current_entry))) {
				*result = buf;
				return current_entry;
			}
		}
		release_block(buf, 0);
	}
	return NULL;
}


/*
static struct ext2_dirent *dir_alloc_entry(struct vnode *vnode, const char *filename, struct buf **result)
{
	char *entry_filename;
	struct ext2_dirent *dir;

	dir = dir_find_entry_by_inode(vnode, 0, MFS_CREATE_BLOCK, result);
	if (!dir)
		return NULL;

	// TODO this is wrong now because the name length is variable so you can't recycle entries the same way
	entry_filename = EXT2_DIRENT_FILENAME(dir);
	strncpy(entry_filename, filename, EXT2_MAX_FILENAME);
	entry_filename[EXT2_MAX_FILENAME - 1] = '\0';

	return dir;
}
*/

#endif
