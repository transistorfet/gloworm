
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <endian.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <kernel/drivers.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/nop.h>
#include <kernel/fs/bufcache.h>
#include <kernel/fs/nop.h>

#include "ext2.h"
#include "ext2structs.h"
#include "bitmaps.h"
#include "inodes.h"
#include "vnodes.h"
#include "blocks.h"
#include "super.h"
#include "dir.h"


struct vfile_ops ext2_vfile_ops = {
	ext2_open,
	ext2_close,
	ext2_read,
	ext2_write,
	ext2_ioctl,
	nop_poll,
	ext2_seek,
	ext2_readdir,
};

struct vnode_ops ext2_vnode_ops = {
	&ext2_vfile_ops,
	ext2_create,
	ext2_mknod,
	ext2_lookup,
	ext2_link,
	ext2_unlink,
	ext2_rename,
	ext2_truncate,
	ext2_update,
	ext2_release,
};

struct mount_ops ext2_mount_ops = {
	"ext2fs",
	ext2_init,
	ext2_mount,
	ext2_unmount,
	ext2_sync,
};


int ext2_init(void)
{
	init_ext2_vnodes();
	return 0;
}


int ext2_mount(struct mount *mp, struct vnode *parent)
{
	int error;

	error = load_superblock(mp);
	if (error < 0)
		return error;

	struct vnode *root = get_vnode(mp, EXT2_ROOT_INODE_NUM);
	if (!root) {
		free_superblock((struct ext2_super *) mp->super);
		return ENOMEM;
	}

	mp->root_node = root;

	return 0;
}

int ext2_unmount(struct mount *mp)
{
	free_superblock((struct ext2_super *) mp->super);
	vfs_release_vnode(mp->root_node);
	mp->root_node = NULL;
	return 0;
}

int ext2_sync(struct mount *mp)
{
	sync_vnodes();
	sync_bufcache(&mp->bufcache);
	return 0;
}

int ext2_create(struct vnode *vnode, const char *filename, mode_t mode, uid_t uid, struct vnode **result)
{
	struct buf *buf = NULL;
	struct ext2_dirent *dir;
	struct vnode *newnode = NULL;

	if (strlen(filename) > EXT2_MAX_FILENAME)
		return ENAMETOOLONG;

	// The dirent wont be 'in use' until we set the vnode reference
	dir = dir_alloc_entry(vnode, filename, &buf);
	if (!dir)
		return ENOSPC;

	// TODO this needs the uid/gid from vfs
	newnode = (struct vnode *) alloc_vnode(vnode->mp, mode, uid, 0, 0);
	if (!newnode) {
		goto fail;
	}

	if (S_ISDIR(mode)) {
		dir->filetype = EXT2_FT_DIR;
		if (!dir_setup(newnode, vnode)) {
			goto fail;
		}
	} else {
		dir->filetype = EXT2_FT_REG_FILE;
	}

	dir->inode = htole32((ext2_inode_t) newnode->ino);

	release_block(buf, BCF_DIRTY);

	*result = newnode;
	return 0;

fail:
	if (newnode)
		vfs_release_vnode(newnode);
	if (buf)
		release_block(buf, 0);
	return ENOMEM;
}

int ext2_mknod(struct vnode *vnode, const char *filename, mode_t mode, device_t dev, uid_t uid, struct vnode **result)
{
	struct buf *buf;
	struct vnode *newnode;
	struct ext2_dirent *dir;

	if (strlen(filename) > EXT2_MAX_FILENAME)
		return ENAMETOOLONG;

	// The dirent wont be 'in use' until we set the vnode reference
	dir = dir_alloc_entry(vnode, filename, &buf);
	if (!dir)
		return ENOSPC;

	// TODO this needs the uid/gid from vfs
	newnode = (struct vnode *) alloc_vnode(vnode->mp, mode, uid, 0, dev);
	if (!newnode) {
		release_block(buf, 0);
		return EMFILE;
	}

	dir->inode = htole32((ext2_inode_t) newnode->ino);
	// TODO need a way to distinguish between block and character devices
	dir->filetype = EXT2_FT_CHRDEV;
	release_block(buf, BCF_DIRTY);

	*result = newnode;
	return 0;
}

int ext2_lookup(struct vnode *vnode, const char *filename, struct vnode **result)
{
	struct buf *buf;
	struct ext2_dirent *entry;

	if (strlen(filename) > EXT2_MAX_FILENAME)
		return ENAMETOOLONG;

	entry = dir_find_entry_by_name(vnode, filename, EXT2_AF_LOOKUP_BLOCK, &buf);
	if (!entry)
		return ENOENT;

	if (*result)
		vfs_release_vnode(*result);
	*result = (struct vnode *) get_vnode(vnode->mp, le32toh(entry->inode));
	release_block(buf, 0);
	return 0;
}

int ext2_link(struct vnode *oldvnode, struct vnode *newparent, const char *filename)
{
	struct buf *buf;
	struct ext2_dirent *dir;

	if (strlen(filename) > EXT2_MAX_FILENAME)
		return ENAMETOOLONG;

	dir = dir_alloc_entry(newparent, filename, &buf);
	if (!dir)
		return ENOSPC;

	oldvnode->nlinks += 1;
	dir->inode = htole32((ext2_inode_t) oldvnode->ino);
	release_block(buf, BCF_DIRTY);
	write_inode(oldvnode, oldvnode->ino);

	return 0;
}

int ext2_unlink(struct vnode *parent, struct vnode *vnode, const char *filename)
{
	struct buf *buf;
	struct ext2_dirent *dir;

	if (S_ISDIR(vnode->mode) && !dir_is_empty(vnode))
		return ENOTEMPTY;

	dir = dir_find_entry_by_name(parent, filename, EXT2_AF_LOOKUP_BLOCK, &buf);
	if (!dir)
		return ENOENT;

	dir->inode = 0;
	release_block(buf, BCF_DIRTY);

	vnode->nlinks -= 1;
	if (vnode->nlinks <= 0) {
		block_free_all(vnode);
		delete_vnode(vnode);
	} else {
		write_inode(vnode, vnode->ino);
	}
	return 0;
}

int ext2_rename(struct vnode *vnode, struct vnode *oldparent, const char *oldname, struct vnode *newparent, const char *newname)
{
	struct buf *oldbuf, *newbuf;
	struct ext2_dirent *olddir, *newdir;

	newdir = dir_find_entry_by_name(newparent, newname, EXT2_AF_LOOKUP_BLOCK, &newbuf);
	if (newdir) {
		// TODO delete existing file instead of error??
		release_block(newbuf, 0);
		return EEXIST;
	} else {
		// TODO this won't work now because the dir entry length is variable

		newdir = dir_alloc_entry(newparent, newname, &newbuf);
		if (!newdir)
			return ENOSPC;
	}

	olddir = dir_find_entry_by_name(oldparent, oldname, EXT2_AF_LOOKUP_BLOCK, &oldbuf);
	if (!olddir) {
		release_block(newbuf, 0);
		return ENOENT;
	}

	newdir->inode = htole32((ext2_inode_t) vnode->ino);
	olddir->inode = htole16(0);
	release_block(newbuf, BCF_DIRTY);
	release_block(oldbuf, BCF_DIRTY);
	vfs_update_time(oldparent, MTIME);
	vfs_update_time(newparent, MTIME);
	return 0;
}

int ext2_truncate(struct vnode *vnode)
{
	if (S_ISDIR(vnode->mode))
		return EISDIR;
	block_free_all(vnode);
	vnode->size = 0;
	vfs_update_time(vnode, MTIME);
	mark_vnode_dirty(vnode);
	return 0;
}

int ext2_update(struct vnode *vnode)
{
	write_inode(vnode, vnode->ino);
	return 0;
}

int ext2_release(struct vnode *vnode)
{
	// NOTE we only free vnodes who's inode has been deleted.  The vnode cache will release other vnodes when they are pushed out by newer nodes
	if (vnode->ino == 0) {
		release_vnode(vnode);
	} else if (vnode->bits & VBF_DIRTY) {
		write_inode(vnode, vnode->ino);
	}
	return 0;
}


int ext2_open(struct vfile *file, int flags)
{
	return 0;
}

int ext2_close(struct vfile *file)
{
	return 0;
}

int ext2_read(struct vfile *file, struct iovec_iter *iter)
{
	if (S_ISDIR(file->vnode->mode))
		return EISDIR;

	short zpos;
	short zlen;
	block_t znum;
	block_t block;
	struct buf *buf;
	size_t nbytes;
	size_t wbytes = 0;
	struct vnode *vnode = file->vnode;
	const int block_size = vnode->mp->bufcache.block_size;

	nbytes = iovec_iter_remaining(iter);
	if (nbytes > vnode->size - file->position)
		nbytes = vnode->size - file->position;

	znum = file->position >> EXT2_LOG_BLOCK_SIZE(block_size);
	zpos = file->position & (block_size - 1);

	do {
		block = block_lookup(vnode, znum, EXT2_AF_LOOKUP_BLOCK);
		if (!block)
			break;
		buf = get_block(&vnode->mp->bufcache, block);
		if (!buf)
			break;

		zlen = block_size - zpos;
		if (zlen > nbytes)
			zlen = nbytes;

		memcpy_into_iter(iter, &(((char *) buf->block)[zpos]), zlen);
		release_block(buf, 0);

		wbytes += zlen;
		nbytes -= zlen;
		znum += 1;
		zpos = 0;
	} while (nbytes > 0);

	file->position += wbytes;
	return wbytes;
}

int ext2_write(struct vfile *file, struct iovec_iter *iter)
{
	if (S_ISDIR(file->vnode->mode))
		return EISDIR;

	short zpos;
	short zlen;
	int error = 0;
	block_t znum;
	block_t block;
	struct buf *buf;
	size_t nbytes;
	size_t wbytes = 0;
	struct vnode *vnode = file->vnode;
	const int block_size = vnode->mp->bufcache.block_size;

	nbytes = iovec_iter_remaining(iter);
	znum = file->position >> EXT2_LOG_BLOCK_SIZE(block_size);
	zpos = file->position & (block_size - 1);

	do {
		block = block_lookup(vnode, znum, EXT2_AF_CREATE_BLOCK);
		if (!block) {
			error = ENOSPC;
			break;
		}
		buf = get_block(&vnode->mp->bufcache, block);
		if (!buf) {
			error = EIO;
			break;
		}

		zlen = block_size - zpos;
		if (zlen > nbytes)
			zlen = nbytes;

		memcpy_out_of_iter(iter, &(((char *) buf->block)[zpos]), zlen);
		release_block(buf, BCF_DIRTY);

		wbytes += zlen;
		nbytes -= zlen;
		znum += 1;
		zpos = 0;
	} while (nbytes > 0);

	file->position += wbytes;
	if (file->position > vnode->size) {
		vnode->size = file->position;
		mark_vnode_dirty(vnode);
	}

	if (wbytes)
		vfs_update_time(vnode, MTIME);

	if (error)
		return error;
	return wbytes;
}

int ext2_ioctl(struct vfile *file, unsigned int request, struct iovec_iter *iter, uid_t uid)
{
	return EINVAL;
}

offset_t ext2_seek(struct vfile *file, offset_t position, int whence)
{
	struct vnode *vnode = file->vnode;

	switch (whence) {
	    case SEEK_SET:
		break;
	    case SEEK_CUR:
		position = file->position + position;
		break;
	    case SEEK_END:
		position = vnode->size + position;
		break;
	    default:
		return EINVAL;
	}

	// TODO this is a hack for now so I don't have to deal with gaps in files
	if (position > vnode->size)
		position = vnode->size;

	file->position = position;
	return file->position;
}

int ext2_readdir(struct vfile *file, struct dirent *dir)
{
	int max;
	short zpos;
	block_t znum;
	block_t block;
	struct buf *buf;
	struct ext2_dirent *current_entry;
	struct vnode *vnode = file->vnode;
	const int block_size = vnode->mp->bufcache.block_size;

	if (!S_ISDIR(vnode->mode)) {
		return ENOTDIR;
	}

	znum = file->position >> EXT2_LOG_BLOCK_SIZE(block_size);
	zpos = file->position & (block_size - 1);

	while (1) {
		block = block_lookup(vnode, znum, EXT2_AF_LOOKUP_BLOCK);
		if (!block) {
			return 0;
		}
		buf = get_block(&vnode->mp->bufcache, block);
		if (!buf) {
			return EIO;
		}

		current_entry = NULL;
		do {
			current_entry = (struct ext2_dirent *) &((char *) buf->block)[zpos];
			zpos += le16toh(current_entry->entry_len);
		} while (current_entry->inode == 0 && zpos < block_size);

		if (!current_entry || current_entry->inode != 0) {
			break;
		}

		release_block(buf, 0);
		znum++;
	}

	file->position = znum << EXT2_LOG_BLOCK_SIZE(block_size) | zpos;

	max = current_entry->name_len;
	if ((int) current_entry->name_len > EXT2_MAX_FILENAME)
		max = EXT2_MAX_FILENAME;
	if ((int) current_entry->name_len > VFS_FILENAME_MAX)
		max = VFS_FILENAME_MAX;

	dir->d_ino = le32toh(current_entry->inode);
	memcpy(dir->d_name, EXT2_DIRENT_FILENAME(current_entry), max);
	dir->d_name[current_entry->name_len] = '\0';
	release_block(buf, BCF_DIRTY);

	return 1;
}

