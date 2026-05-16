
#ifndef _SRC_KERNEL_FS_EXT2_EXT2STRUCTS_H
#define _SRC_KERNEL_FS_EXT2_EXT2STRUCTS_H

#include <stdint.h>

//#define EXT2_BLOCK_SIZE			1024
#define EXT2_LOG_BLOCK_SIZE(bs)			__builtin_ctz((bs))
#define EXT2_BLOCKNUMS_PER_BLOCK(bs)		((bs) / sizeof(ext2_block_t))
#define EXT2_LOG_BLOCKNUMS_PER_BLOCK(bs)	__builtin_ctz(EXT2_BLOCKNUMS_PER_BLOCK(bs))
#define	EXT2_BITS_PER_BLOCK(bs)			((bs) * 8)

#define EXT2_MAX_FILENAME			256

#define EXT2_BLOCKNUMS_IN_INODE			15
#define EXT2_DIRECT_BLOCKNUMS_IN_INODE		12
#define EXT2_INDIRECT_BLOCKNUM_TIERS		3
#define EXT2_TIERS				EXT2_INDIRECT_BLOCKNUM_TIERS + 1

#define EXT2_BLOCKNUM_TABLE(block)		((ext2_block_t *) (block))

#define EXT2_FIRST_SUPERBLOCK_BYTE_OFFSET	1024
#define EXT2_SUPERBLOCK_SIZE			512
#define EXT2_ROOT_INODE_NUM			2

#define EXT2_NEXT_DIRENT(current)		((struct ext2_dirent *) (((char *) (current)) + le16toh(((struct ext2_dirent *) current)->entry_len)))
#define EXT2_DIRENT_FILENAME(current)		(((char *) (current)) + sizeof(struct ext2_dirent))

typedef uint32_t ext2_block_t;
typedef uint32_t ext2_inode_t;

struct ext2_group_descriptor {
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
	uint16_t free_block_count;
	uint16_t free_inode_count;
	uint16_t used_dirs_count;
	uint16_t _padding;
	uint32_t _reserved[3];
};

struct ext2_extended_superblock {
	uint32_t first_non_reserved_inode;
	uint16_t inode_size;
	uint16_t blockgroup_of_super;
	uint32_t compat_features;
	uint32_t incompat_features;
	uint32_t ro_compat_features;
};

struct ext2_superblock {
	uint32_t total_inodes;
	uint32_t total_blocks;
	uint32_t reserved_su_blocks;
	uint32_t total_unalloc_blocks;
	uint32_t total_unalloc_inodes;
	uint32_t superblock_block;
	uint32_t log_block_size;
	uint32_t log_fragment_size;

	uint32_t blocks_per_group;
	uint32_t fragments_per_block;
	uint32_t inodes_per_group;

	uint32_t last_mount_time;
	uint32_t last_write_time;

	uint16_t mounts_since_check;
	uint16_t mounts_before_check;
	uint16_t magic;
	uint16_t state;

	uint16_t errors;
	uint16_t minor_version;
	uint32_t last_check;
	uint32_t check_interval;
	uint32_t creator_os;
	uint32_t major_version;

	uint16_t reserved_uid;
	uint16_t reserved_gid;

	struct ext2_extended_superblock extended;
};

struct ext2_inode {
	uint16_t mode;
	uint16_t uid;
	uint32_t size;
	uint32_t atime;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t dtime;
	uint16_t gid;
	uint16_t nlinks;
	uint32_t nblocks;
	uint32_t flags;
	uint32_t _reserved1;
	ext2_block_t blocks[EXT2_BLOCKNUMS_IN_INODE];
	uint32_t file_generation;
	uint32_t file_acl;
	uint32_t dir_acl;
	uint32_t fragment_addr;
	uint8_t fragment_num;
	uint8_t fragment_size;
	uint16_t _pad1;
	uint16_t uid_high;
	uint16_t gid_high;
	uint32_t _reserved2;
};

struct ext2_dirent {
	ext2_inode_t inode;
	uint16_t entry_len;
	uint8_t name_len;
	uint8_t filetype;
};

#endif

