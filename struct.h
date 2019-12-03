#include <sys/types.h>

typedef unsigned long long u64;
typedef unsigned int u32;

#define MYFS_FILENAME_LEN 255
#define MYFS_BLOCK_SIZE 4096
#define MYFS_BLOCKS_PER_GROUP 16
#define MYFS_INODES_PER_GROUP 8 //
#define MYFS_NUM_GROUPS 8
#define MYFS_NUM_BLOCKS (MYFS_NUM_GROUPS*MYFS_BLOCKS_PER_GROUP)
#define MYFS_NUM_INODES (MYFS_NUM_GROUPS*MYFS_INODES_PER_GROUP)
#define MYFS_ROOT_INODE 0
#define MYFS_INODEMAP_LL MYFS_INODES_PER_GROUP / 64 + 1
#define MYFS_BLOCKMAP_LL MYFS_BLOCKS_PER_GROUP / 64 + 1

struct myFs_super_block {
	u32 num_blocks; // blocks in fs
	u32 num_inodes; // inodes in fs
	u32 num_free_blocks; // free blocks in fs
	u32 num_free_inodes; // free inodes in fs
	u32 num_groups;
	u32 blocks_per_group;
	u32 inodes_per_group; // inodes in group
	u32 block_size;
	u32 inode_size;
	u32 last_dir_group_index; // for uniform distribution of dirs between groups
	u32 current_inode;
	int state; // READY - 1 or others
};

// addresses for random access
struct myFs_group_desc {
	u64 block_bitmap;
	u64 inode_bitmap;
	u64 inode_table;
	u64 data_table;
};

struct myFs_group_desc_table{
	struct myFs_group_desc desc[MYFS_NUM_GROUPS];
};

struct myFs_inode {
	u32 size; // in bytes
	char type; // only two types
	u32 num_blocks; // number of blocks occupied by this file
	u32 parent_inode; // number of parent directory inode
	u64 blocks[14]; // addresses of data blocks
};

struct myFs_dir_file_rec { // structure that describe files in directory
	u32 inode;
	u32 filename_len;
	char filename[MYFS_FILENAME_LEN];
};

struct myFs_inode_bitmap {
	u64 bits[MYFS_INODEMAP_LL];
};

struct myFs_block_bitmap {
	u64 bits[MYFS_BLOCKMAP_LL];
};
