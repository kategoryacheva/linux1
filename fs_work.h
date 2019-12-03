#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "struct.h"

#define READY 1
#define DIR 'd'
#define FIL '-'
#define RECURSIVE 1
#define SIMPLE 2
#define FILE_LENGTH 100

const char MYFS_FILENAME[10] = "myFS";
const u32 MYFS_RECS_PER_BLOCK = MYFS_BLOCK_SIZE / sizeof(struct myFs_dir_file_rec);
const u32 MYFS_BLOCKS_PER_BLOCK = MYFS_BLOCK_SIZE / sizeof(u64);
struct myFs_super_block sb;
struct myFs_group_desc_table gdt;
FILE* myFs = NULL;

int read_block(u64 offset, u32 lenght, void* buffer) {
	if (fseeko(myFs, offset, SEEK_SET) != 0) {
		printf("seek error, offset=%llu, len=%u\n", offset, lenght);
		return 1;
	}
	if (fread(buffer, lenght, 1, myFs) != 1) {
		printf("reading error, offset=%llu, len=%u\n", offset, lenght);
		return 1;
	}
	return 0;
}

int write_block(u64 offset, u32 lenght, void* buffer) {
	if (fseeko(myFs, offset, SEEK_SET) != 0) {
		printf("seek error, offset=%llu, len=%u\n", offset, lenght);
		return 1;
	}
	if (fwrite(buffer, lenght, 1, myFs) != 1) {
		printf("writing error, offset=%llu, len=%u\n", offset, lenght);
		return 1;
	}
	fflush(myFs);
	return 0;
}

int read_inode(u32 inode_index, struct myFs_inode* inode) {
	struct myFs_group_desc desc;
	u32 group_index = inode_index / sb.inodes_per_group;
	u32 inode_group_index = inode_index % sb.inodes_per_group;
	desc = gdt.desc[group_index];
	u64 offset = desc.inode_table + inode_group_index*((u64)sizeof(struct myFs_inode));
	if (read_block(offset, (u64)sizeof(struct myFs_inode), inode)) {
		printf("Cannot read %u inode\n", inode_index);
		return 1;
	}
	return 0;
}

int write_inode(u32 inode_index, struct myFs_inode* inode) {
	struct myFs_group_desc desc;
	u64 group_index = inode_index / sb.inodes_per_group;
	u64 inode_group_index = inode_index % sb.inodes_per_group;
	desc = gdt.desc[group_index];
	u64 offset = desc.inode_table + inode_group_index*((u64)sizeof(struct myFs_inode));
	if (write_block(offset, (u64)sizeof(struct myFs_inode), inode)) {
		printf("Cannot write %u inode\n", inode_index);
		return 1;
	}
	return 0;
}

void create_empty_inode(struct myFs_inode* inode, char type) {
	memset(inode, 0, sizeof(struct myFs_inode));
	inode->size = 0;
	inode->num_blocks = 0;
	memset(inode->blocks, 0, 14*sizeof(u64));
	inode->type = type;
	inode->parent_inode = 0;
}

int free_block(u64 offset) {
	char block[MYFS_BLOCK_SIZE];
	memset(block, 0, MYFS_BLOCK_SIZE);
	return write_block(offset, MYFS_BLOCK_SIZE, &block);
}

int free_inode(u32 inode_index)	{
	struct myFs_inode inode;
	create_empty_inode(&inode, FIL);
	return write_inode(inode_index, &inode);
}

int read_sb() {
	return read_block(0, (u64)sizeof(struct myFs_super_block), &sb);
}

int check_sb() {
	if (sb.state == READY)
		return 1;
	else
		return 0;
}

int rewrite_sb() {
	return write_block(0, (u64)sizeof(struct myFs_super_block), &sb);
}

int read_group_desc_table() {
	u64 offset = (u64)sizeof(struct myFs_super_block);
	return read_block(offset, (u64)sizeof(struct myFs_group_desc_table), &gdt);
}

int rewrite_group_desc_table() {
	u64 offset = (u64)sizeof(struct myFs_super_block);
	return write_block(offset, (u64)sizeof(struct myFs_group_desc_table), &gdt);
}

void path_splitter(char* path, char* filename) {
	size_t len = strlen(path);
	int i = (int)len-1;
	while(path[i] != '/' && i >= 0) --i;
	if (i > 0) {
		memcpy(filename, path+i+1, len-i);
		filename[len-i] = '\0';
		path[i] = '\0';
	}
	if (i == 0) {
		memcpy(filename, path+i+1, len-i);
		filename[len-i] = '\0';
		path[0] = '/';
		path[1] = '\0';
	}
	if (i == -1) {
		memcpy(filename, path, len);
		filename[len] = '\0';
		path[0] = '.';
		path[1] = '/';
		path[2] = '\0';
	}
}

int get_inode_index_from_dirblock(u64 data_block_offset, char* filename, u32* answer, int deep) {
	u32 i = 0;
	int out;
	char data_block[MYFS_BLOCK_SIZE];
	if (read_block(data_block_offset, MYFS_BLOCK_SIZE, data_block)) return 2;
	if (deep == 1) {
		struct myFs_dir_file_rec* recs = (struct myFs_dir_file_rec*)data_block;
		for (i = 0; i < MYFS_RECS_PER_BLOCK; ++i) {
			if (recs[i].filename_len == 0) {
				continue;
			} else {
				if (!strcmp(recs[i].filename, filename)) {
					*answer = recs[i].inode;
					return 0;
				}
			}
		}
		return 1;
	} else {
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			if (blocks[i] == 0)
				continue;
			out = get_inode_index_from_dirblock(blocks[i], filename, answer, deep-1);
			if (out == 1) {
				continue;
			} else {
				return out;
			}
		}
		return 1;
	}
}

int get_inode_index_from_dir(struct myFs_inode* dir, u32 dir_inode_index, char* filename, u32* answer) {
	u32 i = 0;
	if (!strcmp(filename, "..")) {
		*answer = dir->parent_inode;
		return 0;
	}
	if (!strcmp(filename, ".")) {
		*answer = dir_inode_index;
		return 0;
	}
	if (dir->num_blocks <= 12) {
		for (i = 0; i < dir->num_blocks; ++i) {
			if (!get_inode_index_from_dirblock(dir->blocks[i],filename, answer, 1)) {
				return 0;
			}
		}
		return 1;
	}
	if (dir->num_blocks > 12){
		for (i = 0; i < 12; ++i) {
			if (!get_inode_index_from_dirblock(dir->blocks[i],filename, answer, 1)) {
				return 0;
			}
		}
		if (!get_inode_index_from_dirblock(dir->blocks[12],filename, answer, 2)) {
			return 0;
		}
	}
	if (dir->num_blocks > 12 + MYFS_BLOCKS_PER_BLOCK) {
		return get_inode_index_from_dirblock(dir->blocks[13],filename, answer, 3);
	}
	return 1;
}

int get_inode_index_from_relpath(u32 start_inode, char* path, u32* answer) {
	int i,j=0;
	char tmp_buffer[MYFS_FILENAME_LEN];
	u32 inode_index;
	struct myFs_inode current_inode;

	if (read_inode(start_inode, &current_inode)) return 1;
	inode_index = start_inode;
	while(1) {
		if (current_inode.type != DIR) {
			path[j-1] = '\0';
			printf("%s not such directory\n", path);
		}
		memset(tmp_buffer, 0, sizeof(tmp_buffer));
		for (i = 0; i < MYFS_FILENAME_LEN; ++i,++j) {
			tmp_buffer[i] = path[j];
			if (tmp_buffer[i] == '/' || tmp_buffer[i] == '\0')
				break;
		}
		if (i == 0) break;
		tmp_buffer[i] = '\0';
		if (get_inode_index_from_dir(&current_inode, inode_index, tmp_buffer, &inode_index)) {
			return 1;
		}
		if (read_inode(inode_index, &current_inode)) return 1;
		if (path[j] == '\0')
			break;
		else
			++j;
	}
	*answer = inode_index;
	return 0;
}

int get_inode_index_from_path(char* path, u32* answer) {
	if (path[0] == '/') {
		if (path[1] == '\0') {
			*answer = MYFS_ROOT_INODE;
			return 0;
		} else {
			return get_inode_index_from_relpath(MYFS_ROOT_INODE, path+1, answer);
		}
	} else {
		return get_inode_index_from_relpath(sb.current_inode, path, answer);
	}
}

int get_file_from_block(u64 data_block_offset, int deep, char* output) {
	u32 i = 0;
	int out;
	char data_block[MYFS_BLOCK_SIZE];
	if (data_block_offset == 0) return 0;
	if (read_block(data_block_offset, MYFS_BLOCK_SIZE, data_block)) {
		return 1;
	}
	if (deep == 1) {
		u32 len = strlen(output) + strlen(data_block) + 1;
		output = (char*) realloc(output, len);
		if (!output) {
			return 1;
		}
		strcat(output, data_block);
	} else {
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			if (blocks[i] == 0)
				break;
			out = get_file_from_block(blocks[i], deep-1, output);
			if (out == 0) {
				continue;
			} else {
				return out;
			}
		}
	}
	return 0;
}

int get_file_from_inode(u32 inode_index, char* output) {
	struct myFs_inode inode;
	u32 i = 0;
	if (read_inode(inode_index, &inode) != 0) return 1;
	if (inode.num_blocks <= 12) {
		for (i = 0; i < inode.num_blocks; ++i) {
			if (get_file_from_block(inode.blocks[i], 1, output)) return 1;
		}
		return 0;
	}
	if (inode.num_blocks > 12) {
		for (i = 0; i < 12; ++i) {
			if (get_file_from_block(inode.blocks[i], 1, output) != 0) return 1;
		}
		if (get_file_from_block(inode.blocks[12], 2, output)) return 1;
	}
	if (inode.num_blocks > 12 + MYFS_BLOCKS_PER_BLOCK) {
		return get_file_from_block(inode.blocks[13], 3, output);
	}
	return 0;
}

int icat(char* path) {
	u32 inode_index;
	char* output = (char*) malloc(sizeof(char));
	memset(output, 0, 1);
	if (get_inode_index_from_path(path, &inode_index)) {
		free(output);
		return 1;
	}
	if (!get_file_from_inode(inode_index, output)) {
		printf("%s\n", output);
		free(output);
		return 0;
	} else {
		free(output);
		return 1;
	}
}

int occupy_free_inode(u32* answer, u32 preferred_group) {
	u32 i,j,k,group_index;
	u64 bits;
	struct myFs_inode_bitmap bm;
	if (sb.num_free_inodes == 0) {
		return 1;
	}
	for (i = preferred_group; i < preferred_group + MYFS_NUM_GROUPS; ++i) {
		group_index = i % MYFS_NUM_GROUPS;
		read_block(gdt.desc[group_index].inode_bitmap, sizeof(struct myFs_inode_bitmap), &bm);
		for (j = 0; j < MYFS_INODEMAP_LL; ++j) {
			if (j*64 >= MYFS_INODES_PER_GROUP) break;
			bits = bm.bits[j];
			for (k = 0; k < 64; ++k) {
				if (j*64 + k >= MYFS_INODES_PER_GROUP) break;
				if (!(bits & 1)) {
					bits |= 1;
					bits <<= k;
					bm.bits[j] |= bits;
					*answer = i*MYFS_INODES_PER_GROUP + j*64 + k;
					--sb.num_free_inodes;
					write_block(gdt.desc[group_index].inode_bitmap, sizeof(struct myFs_inode_bitmap), &bm);
					free_inode(*answer);
					return 0;
				} else {
					bits >>= 1;
				}
			}
		}
	}
  	return 1;
}

int occupy_free_block(u64* offset, u32 preferred_group) {
	u64 i,j,k,group_index,bits;
	struct myFs_block_bitmap bm,bbm;
	if (sb.num_free_blocks == 0) {
		return 1;
	}
	u32 z =0;
	for (i = preferred_group; i < preferred_group + MYFS_NUM_GROUPS; ++i) {
		group_index = i % MYFS_NUM_GROUPS;
		read_block(gdt.desc[group_index].block_bitmap, sizeof(struct myFs_block_bitmap), &bm);
		z = 0;
		for(z = 0; z <MYFS_NUM_GROUPS; ++z) {
			read_block(gdt.desc[z].block_bitmap, sizeof(struct myFs_block_bitmap), &bbm);
		}
		for (j = 0; j < MYFS_BLOCKMAP_LL; ++j) {
			if (j*64 >= MYFS_BLOCKS_PER_GROUP) break;
			bits = bm.bits[j];
			for (k = 0; k < 64; ++k) {
				if (j*64 + k >= MYFS_BLOCKS_PER_GROUP) break;
				if (!(bits & 1)){
					*offset = (j*64 + k)*MYFS_BLOCK_SIZE + gdt.desc[group_index].data_table;
					bits |= 1;
					bits <<= k;
					bm.bits[j] |= bits;
					--sb.num_free_blocks;
					write_block(gdt.desc[group_index].block_bitmap, sizeof(struct myFs_block_bitmap), &bm);
					free_block(*offset);
					return 0;
				} else {
					bits >>= 1;
				}
			}
		}
	}
	return 1;
}

int free_inode_bm(u32 inode_index) {
	u32 group_index = inode_index / MYFS_INODES_PER_GROUP;
	u32 index = inode_index % MYFS_INODES_PER_GROUP;
	u32 j,k;
	u64 bits, delta;
	struct myFs_inode_bitmap bm;
	if (read_block(gdt.desc[group_index].inode_bitmap, sizeof(struct myFs_inode_bitmap), &bm))
		return 1;
	j = index / 64;
	k = index % 64;
	bits = bm.bits[j];
	bits >>= k;
	if (bits & 1) {
		delta = 0;
		delta >>= k;
		delta = 1;
		delta <<= k;
		bm.bits[j] -= delta;
		++sb.num_free_inodes;
		write_block(gdt.desc[group_index].inode_bitmap, sizeof(struct myFs_inode_bitmap), &bm);
	}
	return 0;
}

int free_block_bm(u64 offset) {
	u32 group_index = 0;
	u32 index;
	u32 i,j,k;
	u64 bits, delta;
	struct myFs_block_bitmap bm;
	for (i = 0; i < MYFS_NUM_GROUPS; ++i) {
		if (gdt.desc[i].data_table <= offset) ++group_index;
		else break;
	}
	--group_index;
	index = (u32)((offset-gdt.desc[group_index].data_table) / MYFS_BLOCK_SIZE);
	if (read_block(gdt.desc[group_index].block_bitmap, sizeof(struct myFs_block_bitmap), &bm))
		return 1;
	j = index / 64;
	k = index % 64;
	bits = bm.bits[j];
	bits >>= k;
	if (bits & 1) {
		delta = 0;
		delta >>= k;
		delta = 1;
		delta <<= k;
		bm.bits[j] -= delta;
		++sb.num_free_blocks;
		write_block(gdt.desc[group_index].block_bitmap, sizeof(struct myFs_block_bitmap), &bm);
	}
	return 0;
}

int add_dir_rec_to_block(u64 offset, struct myFs_dir_file_rec* rec, int deep) {
	u32 i = 0;
	char data_block[MYFS_BLOCK_SIZE];
	memset(data_block, 0, MYFS_BLOCK_SIZE);
	if (offset == 0) {
		return 1;
	}
	if (read_block(offset, MYFS_BLOCK_SIZE, data_block)) {
		return 1;
	}
	if (deep == 1) {
		struct myFs_dir_file_rec* recs = (struct myFs_dir_file_rec*)data_block;
		for (i = 0; i < MYFS_RECS_PER_BLOCK; ++i) {
			if (recs[i].filename_len == 0) {
				recs[i] = *rec;
				write_block(offset, MYFS_BLOCK_SIZE, data_block);
				return 0;
			}
		}
		return 1;
	} else {
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			if (!add_dir_rec_to_block(blocks[i], rec, deep-1)) return 0;
		}
		return 1;
	}
}

int add_dir_to_new_block(struct myFs_dir_file_rec* rec, u32 dir_inode_index, struct myFs_inode* dir) {
	u64 offset,offset1;
	u32 block_index = dir->num_blocks;
	u32 i = 0;
	char data_block[MYFS_BLOCK_SIZE];
	memset(data_block, 0, MYFS_BLOCK_SIZE);
	if (occupy_free_block(&offset, dir_inode_index / MYFS_NUM_GROUPS)) return 1;
	memcpy(data_block, rec, sizeof(struct myFs_dir_file_rec));
	if (write_block(offset, MYFS_BLOCK_SIZE, data_block)) return 1;
	if (block_index < 12) {
		dir->blocks[block_index] = offset;
		++dir->num_blocks;
		return 0;
	}
 	if (block_index < 12 + MYFS_BLOCKS_PER_BLOCK) {
		if (dir->blocks[12] == 0) {
			if (occupy_free_block(&offset1, rec->inode / MYFS_INODES_PER_GROUP)) return 1;
				dir->blocks[12] = offset1;
		} else {
			offset1 = dir->blocks[12];
		}
		if (write_block(offset1, MYFS_BLOCK_SIZE, data_block)) return 1;
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			if (blocks[i] == 0) blocks[i] = offset;
		}
		if (write_block(offset1, MYFS_BLOCK_SIZE, data_block)){
			return 1;
		}
		return 0;
	}
	return 1;
}

int add_dir_rec(u32 dir_inode_index, char* filename, u32 inode_index) {
	u32 i = 0;
	struct myFs_dir_file_rec rec;
	memset(&rec, 0, sizeof(struct myFs_dir_file_rec));
	struct myFs_inode dir;
	if (read_inode(dir_inode_index, &dir)) {
		return 1;
	}
	rec.inode = inode_index;
	rec.filename_len = (u32)strlen(filename);
	memcpy(rec.filename, filename, strlen(filename));
	for(i = 0; i < 12; ++i) {
		if (!add_dir_rec_to_block(dir.blocks[i], &rec, 1))
			return 0;
	}
	if (!add_dir_rec_to_block(dir.blocks[12], &rec, 2))
		return 0;
	if (!add_dir_rec_to_block(dir.blocks[13], &rec, 3))
		return 0;
	if (!add_dir_to_new_block(&rec, dir_inode_index, &dir)) {
		return write_inode(dir_inode_index, &dir);
	}
	return 1;
}

int delete_dir_rec_from_block(u64 offset, char* filename, int deep){
	u32 i = 0;
	char data_block[MYFS_BLOCK_SIZE];
	if (offset == 0) {
		return 1;
	}
	if (read_block(offset, MYFS_BLOCK_SIZE, data_block)) {
		return 1;
	}
	if (deep == 1) {
		struct myFs_dir_file_rec* recs = (struct myFs_dir_file_rec*)data_block;
		for (i = 0; i < MYFS_RECS_PER_BLOCK; ++i) {
			if (recs[i].filename_len != 0 && !strcmp(recs[i].filename,filename)) {
				recs[i].filename[0] = '\0';
				recs[i].filename_len = 0;
				recs[i].inode = 0;
				return write_block(offset, MYFS_BLOCK_SIZE, data_block);
			}
		}
		return 1;
	} else {
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			if (!delete_dir_rec_from_block(blocks[i], filename, deep-1)) return 0;
		}
		return 1;
	}
}

int delete_dir_rec(u32 dir_inode_index, char* filename) {
	u32 i = 0;
	struct myFs_inode dir;
	if (read_inode(dir_inode_index, &dir)) return 1;
	for(i = 0; i < 12; ++i) {
	if (!delete_dir_rec_from_block(dir.blocks[i], filename, 1))
		return 0;
	}
	if (!delete_dir_rec_from_block(dir.blocks[12], filename, 2))
		return 0;
	if (!delete_dir_rec_from_block(dir.blocks[13], filename, 3))
		return 0;
	return 1;
}

int add_external_file(char* path_ext, char* dir) {
	FILE* ext_file;
	if((ext_file = fopen(path_ext, "r+b")) == NULL) {
		printf("Cannot open myFs file\n");
		return 1;
	}
	return 0;
}

int empty_block(u64 offset, int deep) {
	u32 i = 0;
	int out;
	char data_block[MYFS_BLOCK_SIZE];
	if (offset == 0) return 0;
	if (read_block(offset, MYFS_BLOCK_SIZE, data_block)) return 1;
	if (deep == 1) {
		if (free_block_bm(offset)) return 1;
	} else {
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			if (blocks[i] == 0) {
				if (free_block_bm(offset)) return 1;
				return 0;
			}
			out = empty_block(blocks[i], deep-1);
			if (out == 0) {
				continue;
			} else {
				return out;
			}
		}
		if (free_block_bm(offset)) return 1;
	}
	return 0;
}

int empty_blocks(struct myFs_inode* inode) {
	u32 i;
	for (i = 0; i < 12; ++i) {
		if (inode->blocks[i] == 0) return 0;
		if (empty_block(inode->blocks[i], 1)) return 1;
	}
	if (inode->blocks[12] == 0) return 0;
	if (empty_block(inode->blocks[12], 2)) return 1;

	if (inode->blocks[13] == 0) return 0;
	if (empty_block(inode->blocks[13], 3)) return 1;
	return 0;
}

int create_file(char* path, char type) {
	char filename[MYFS_FILENAME_LEN];
	u32 dir_inode_index, group_index = 0, new_inode_index;
	struct myFs_inode new_inode;
	if (sb.num_free_inodes == 0) {
		printf("There is no free memory\n");
		return 2;
	}
	if (!get_inode_index_from_path(path, &new_inode_index)) {
		printf("%s File exists\n", path);
		return 1;
	}
	path_splitter(path, filename);
	if (get_inode_index_from_path(path, &dir_inode_index)) {
		return 1;
	}
	if (type == FIL)
		group_index = dir_inode_index / MYFS_NUM_GROUPS;
	if (type == DIR) {
		group_index = sb.last_dir_group_index;
		sb.last_dir_group_index = (sb.last_dir_group_index+1)%MYFS_NUM_GROUPS;
	}
	if (occupy_free_inode(&new_inode_index, group_index)) {
		return 1;
	}
	create_empty_inode(&new_inode, type);
	add_dir_rec(dir_inode_index, filename, new_inode_index);
	new_inode.parent_inode = dir_inode_index;
	if (write_inode(new_inode_index, &new_inode)) {
		return 1;
	}
	return 0;
}

int fill_block(FILE* ext_file, u32 inode_index, u64* offset, u32* num_remaining_blocks, int deep) {
	u32 i = 0;
	size_t len;
	int out;
	char data_block[MYFS_BLOCK_SIZE];
	memset(data_block, 0, MYFS_BLOCK_SIZE);
	if (*num_remaining_blocks == 0) return 0;
	if (occupy_free_block(offset, inode_index / MYFS_INODES_PER_GROUP)) return 1;
	if (deep == 1) {
		data_block[MYFS_BLOCK_SIZE-1]='\0';
		len = fread(data_block, 1, MYFS_BLOCK_SIZE-1, ext_file);
		--(*num_remaining_blocks);
		return write_block(*offset, (u32)len, data_block);
	} else {
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			out = fill_block(ext_file, inode_index, blocks + i, num_remaining_blocks, deep - 1);
			if (out == 0) continue;
			else return out;
		}
		return write_block(*offset, MYFS_BLOCK_SIZE, data_block);
  	}
}

u32 blocks_req(u32 num) {
	if (num <= 12) return num;
	if (num <= 12 + MYFS_BLOCKS_PER_BLOCK) return num+1;
	return num + 2 + ceil((double)(num - 12 - MYFS_BLOCKS_PER_BLOCK)/ (double)MYFS_BLOCKS_PER_BLOCK);
}

int fill_file(u32 inode_index, char* external_path) {
	FILE* external_file;
	u32 i = 0;
	struct myFs_inode inode;
	u32 num_blocks;
	if (read_inode(inode_index, &inode)) return 1;
	if((external_file = fopen(external_path, "r+b")) == NULL) {
		printf("Cannot open external file\n");
		return 1;
	}
	fseeko(external_file, 0, SEEK_END);
	inode.size = (u32)ftell(external_file) + 1;
	inode.num_blocks = ceil((double)inode.size/ (double)(MYFS_BLOCK_SIZE-1));
	if (blocks_req(inode.num_blocks) > sb.num_free_blocks) {
		printf("There is not enough memory\n");
		fclose(external_file);
		return 1;
	}
	num_blocks = inode.num_blocks;
	rewind(external_file);
	if (inode.num_blocks <= 12) {
		for (i = 0; i < inode.num_blocks; ++i) {
			if (fill_block(external_file, inode_index, &(inode.blocks[i]), &num_blocks, 1)) return 1;
		}
	}
	if (inode.num_blocks > 12) {
		for (i = 0; i < 12; ++i) {
			if (fill_block(external_file, inode_index, &(inode.blocks[i]), &num_blocks, 1)) return 1;
		}
		if (fill_block(external_file, inode_index, &(inode.blocks[12]), &num_blocks, 2)) return 1;
	}
	if (inode.num_blocks > 12 + MYFS_BLOCKS_PER_BLOCK) {
		return fill_block(external_file, inode_index, &(inode.blocks[13]), &num_blocks, 3);
	}
	fclose(external_file);
	return write_inode(inode_index, &inode);
}

int delete_file_with_inode(u32 inode_index, char* filename, char type) {
	struct myFs_inode inode;
	u32 dir_inode_index;
	if (read_inode(inode_index, &inode)) return 1;
	if (inode.type != type) return 2;
	dir_inode_index = inode.parent_inode;
	if (empty_blocks(&inode)) return 1;
	if (free_inode(inode_index)) return 1;
	if (free_inode_bm(inode_index)) return 1;
	if (delete_dir_rec(dir_inode_index, filename)) return 1;
	return 0;
}

int delete_file(char* path) {
	u32 inode_index;
	char filename[MYFS_FILENAME_LEN];
	if (get_inode_index_from_path(path, &inode_index)) return 1;
	path_splitter(path, filename);
	return delete_file_with_inode(inode_index, filename, FIL);
}

int rec_delete_file_with_inode(u32, char*);

int rec_delete_from_block(u64 offset, int deep) {
	u32 i = 0;
	char data_block[MYFS_BLOCK_SIZE];
	if (offset == 0) return 0;
	if (read_block(offset, MYFS_BLOCK_SIZE, data_block)) return 1;
	if (deep == 1) {
		struct myFs_dir_file_rec* recs = (struct myFs_dir_file_rec*)data_block;
		for (i = 0; i < MYFS_RECS_PER_BLOCK; ++i) {
			if (recs[i].filename_len != 0) {
				if (rec_delete_file_with_inode(recs[i].inode, recs[i].filename)) return 1;
			}
		}
	} else {
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			if (rec_delete_from_block(blocks[i], deep - 1)) return 1;
		}
	}
  return 0;
}

int rec_delete_file_with_inode(u32 inode_index, char* filename) {
	struct myFs_inode inode;
	u32 i;
	if (read_inode(inode_index, &inode)) return 1;
	if (inode.type == DIR) {
		for (i = 0; i < 12; ++i) {
			if (rec_delete_from_block(inode.blocks[i], 1)) return 1;
		}
		if (rec_delete_from_block(inode.blocks[12], 2)) return 1;
		if (rec_delete_from_block(inode.blocks[13], 3)) return 1;
	}
	return delete_file_with_inode(inode_index, filename, inode.type);
}

int rec_delete(char* path) {
	u32 inode_index;
	char filename[MYFS_FILENAME_LEN];
	if (get_inode_index_from_path(path, &inode_index)) return 1;
	path_splitter(path, filename);
	return rec_delete_file_with_inode(inode_index, filename);
}

int ls_from_block(u64 offset, int deep) {
	u32 i = 0;
	int out;
	char data_block[MYFS_BLOCK_SIZE];
	if (read_block(offset, MYFS_BLOCK_SIZE, data_block)) {
		return 1;
	}
	if (deep == 1) {
		struct myFs_dir_file_rec* recs = (struct myFs_dir_file_rec*)data_block;
		for (i = 0; i < MYFS_RECS_PER_BLOCK; ++i) {
			if (recs[i].filename_len != 0) printf("%-10s\n", recs[i].filename);
		}
	} else {
		u64* blocks = (u64*)data_block;
		for (i = 0; i < MYFS_BLOCKS_PER_BLOCK; ++i) {
			if (blocks[i] == 0)
				break;
			out = ls_from_block(blocks[i], deep-1);
			if (out == 0) {
				continue;
			} else {
				return out;
			}
		}
	}
	return 0;
}

void ils(char* dir_path) {
	u32 i = 0;
	struct myFs_inode dir_inode;
	u32 dir_inode_index;
	if (get_inode_index_from_path(dir_path, &dir_inode_index)) {
		printf("ls: no such file or directory\n");
		return;
	}
	if (read_inode(dir_inode_index, &dir_inode)) return;
	if (dir_inode.num_blocks <= 12) {
		for (i = 0; i < dir_inode.num_blocks; ++i) {
			if (ls_from_block(dir_inode.blocks[i], 1)) {
				printf("ls error\n");
				return;
			}
		}
	}
	if (dir_inode.num_blocks > 12) {
		for (i = 0; i < 12; ++i) {
			if (ls_from_block(dir_inode.blocks[i], 1)) {
				printf("ls error\n");
				return;
			}
		}
		if (ls_from_block(dir_inode.blocks[12], 2)) {
			printf("ls error\n");
			return;
		}
	}
	if (dir_inode.num_blocks > 12 + MYFS_BLOCKS_PER_BLOCK) {
		if (ls_from_block(dir_inode.blocks[13], 3)) {
			printf("ls error\n");
			return;
		}
	}
}

int icd(char* dir_path) {
	u32 dir_inode_index;
	if(get_inode_index_from_path(dir_path, &dir_inode_index)) {
		return 1;
	}
	sb.current_inode = dir_inode_index;
	return 0;
}

int irm(char* path, int mode) {
	if (mode == SIMPLE) {
		return delete_file(path);
	}
	if (mode == RECURSIVE) {
		return rec_delete(path);
	}
	return 1;
}

int iimport(char* ext_file, char* path) {
	u32 inode_index;
	char filename[MYFS_FILENAME_LEN];
	path_splitter(path, filename);
	if (path[strlen(path)-1] != '/') strcat(path,"/");
	strcat(path, filename);
	if (create_file(path, FIL)) return 2;
	if (path[strlen(path)-1] != '/') strcat(path,"/");
	strcat(path, filename);
	if (get_inode_index_from_path(path, &inode_index)) return 1;
	if (fill_file(inode_index, ext_file)) {
		delete_file(path);
		return 2;
	}
	return 0;
}

int iexport(char* ext_file, char* outer_path) {
	char* file_path = (char*) malloc(sizeof(char) * (strlen(outer_path) + 3));
	memset(file_path, 0, strlen(outer_path) + 3);
	strncpy(file_path, "./", 2);
	strncpy(&file_path[2], outer_path, strlen(outer_path) + 1);

	FILE *file = fopen(file_path, "w");

	if (file == NULL) {
		printf("wrong outer name\n");
		return 1;
	}
	u32 inode_index;
	if (get_inode_index_from_path(ext_file, &inode_index)) return 1;
	char* output = (char*) malloc(sizeof(char));
	memset(output, 0, 1);
	if (!get_file_from_inode(inode_index, output)) {
		fwrite(output, sizeof(char), strlen(output), file);
		fclose(file);

		free(output);
		free(file_path);
		return 0;
	} else {
		free(output);
		free(file_path);
		return 1;
	}
}

int myFs_install(){
	if((myFs = fopen(MYFS_FILENAME, "w+b")) == NULL) {
		printf("Cannot open myFs file\n");
		return 1;
	}
	u64 offset = sizeof(struct myFs_super_block) + sizeof(struct myFs_group_desc_table);
	u32 i,j;
	sb.blocks_per_group = MYFS_BLOCKS_PER_GROUP;
	sb.inodes_per_group = MYFS_INODES_PER_GROUP;
	sb.block_size = MYFS_BLOCK_SIZE;
	sb.inode_size = sizeof(struct myFs_inode);
	sb.num_groups = MYFS_NUM_GROUPS;
	sb.num_blocks = MYFS_NUM_BLOCKS;
	sb.num_inodes = MYFS_NUM_INODES;
	sb.num_free_blocks = MYFS_NUM_BLOCKS;
	sb.num_free_inodes = MYFS_NUM_INODES;
	sb.current_inode = 0;
	sb.state = READY;
	sb.last_dir_group_index = 1;
  
	fwrite(&sb, sizeof(struct myFs_super_block), 1, myFs);
	memset(&gdt, 0, sizeof(struct myFs_group_desc_table));
	fwrite(&gdt, sizeof(struct myFs_group_desc_table), 1, myFs);
	struct myFs_block_bitmap bbm;
	memset(bbm.bits, 0, MYFS_BLOCKMAP_LL*sizeof(u64));
	struct myFs_inode_bitmap ibm;
	memset(ibm.bits, 0, MYFS_INODEMAP_LL*sizeof(u64));
	struct myFs_inode in;
	create_empty_inode(&in, FIL);
	char tmp_block[MYFS_BLOCK_SIZE];
	memset(tmp_block, 0, MYFS_BLOCK_SIZE);
	for (i = 0; i < MYFS_NUM_GROUPS; ++i) {
		fwrite(&bbm, sizeof(struct myFs_block_bitmap), 1, myFs);
		gdt.desc[i].block_bitmap = offset;
		offset += sizeof(struct myFs_block_bitmap);
		fwrite(&ibm, sizeof(struct myFs_inode_bitmap), 1, myFs);
		gdt.desc[i].inode_bitmap = offset;
		offset += sizeof(struct myFs_inode_bitmap);
		gdt.desc[i].inode_table = offset;
		for (j = 0; j < MYFS_INODES_PER_GROUP; ++j) {
			fwrite(&in, sizeof(struct myFs_inode), 1, myFs);
			offset += sizeof(struct myFs_inode);
		}
		gdt.desc[i].data_table = offset;
		for (j = 0; j < MYFS_BLOCKS_PER_GROUP; ++j){
			fwrite(tmp_block, MYFS_BLOCK_SIZE, 1, myFs);
			offset += MYFS_BLOCK_SIZE;
		}
	}

	ibm.bits[0] = 1;
	write_block(gdt.desc[0].inode_bitmap, sizeof(struct myFs_inode_bitmap), &ibm);
	in.type = DIR;
	in.parent_inode = 0;
	write_inode(0, &in);
	if (rewrite_sb() || rewrite_group_desc_table()) {
		fclose(myFs);
		return 1;
	}
	fclose(myFs);
	return 0;
}
