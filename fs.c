#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>

#include "fs.h"
#include "disk.h"

#define MAX_FILES 64
#define MAX_FILE_LENGTH 15
#define MAX_FILE_DESCRIPTOR 32
#define MAX_FILESIZE (4 << 22)
#define END_OF_FILE -1
#define NOT_IN_USE -2

struct Superblock
{
	int used_block_bitmap_count;
	int used_block_bitmap_offset;
	int inode_metadata_blocks;
	int inode_metadata_offset;
	int directory_length;
	int directory_offset;
	int data_offset;
};

struct Inode
{
	char file_type;
	int direct_offset;
};

struct Directory
{
	bool is_used;
	char file_names[MAX_FILE_LENGTH + 1];
	int inode_num;
	int d_size;
	int count;
};

struct File_Descriptor
{
	bool isused;
	int inode_num;
	int offset;
};

struct Superblock * super_block;
struct Inode * inode_table;
struct Directory * directory;
struct File_Descriptor fd[MAX_FILE_DESCRIPTOR];
char * used_block_bitmap;

static bool is_mounted = false;

int make_fs(const char * disk_name)
{
	if(make_disk(disk_name) == -1)
	{
		return -1;
	}
	if(open_disk(disk_name) == -1)
	{
		return -1;
	}

	struct Superblock sb;
	struct Directory drct;
	struct Inode inode;

	sb.used_block_bitmap_offset = 1;
	sb.used_block_bitmap_count = (sizeof(char) * DISK_BLOCKS) / BLOCK_SIZE;
	sb.inode_metadata_offset = sb.used_block_bitmap_count + sb.used_block_bitmap_offset;
	sb.inode_metadata_blocks = (sizeof(struct Inode) * DISK_BLOCKS) / BLOCK_SIZE;
	sb.directory_offset = sb.inode_metadata_offset + sb.inode_metadata_blocks;
	sb.directory_length = (sizeof(struct Directory) * MAX_FILES) / BLOCK_SIZE + 1;
	sb.data_offset = sb.directory_offset + sb.directory_length;

	drct.is_used = false;
	drct.file_names[0] = '\0';
	drct.inode_num = -1;
	drct.d_size = 0;
	drct.count = 0;

	inode.direct_offset = NOT_IN_USE;
	
	char bitmap = '0';
	char buffer[BLOCK_SIZE * 10];
	memset(buffer, 0, sizeof(buffer));

	memcpy((void*) buffer,(void*) &sb, sizeof(struct Superblock));
	if(block_write(0, buffer) == -1) 
	{
		return -1;	
	}
	
	memset(buffer, 0, sizeof(buffer));
	size_t size = sb.used_block_bitmap_count * sizeof(char);
	memcpy((void *) buffer, (void *) &bitmap, size);
	if(block_write(sb.used_block_bitmap_offset, buffer) == -1)
	{	
		return -1;
	}

	memset(buffer, 0, sizeof(buffer));
	
	for(int i = 0; i < sb.inode_metadata_blocks * (BLOCK_SIZE / sizeof(struct Inode)); i += sizeof(struct Inode))
	{
		memcpy((void *) (buffer + i), (void *) &inode, sizeof(struct Inode));
	}
	if(block_write(sb.inode_metadata_offset, buffer) == -1) 
	{
		return -1;
	}

	memset(buffer, 0, sizeof(buffer));

	memcpy((void *) buffer,(void *) &drct, sizeof(struct Directory) * MAX_FILES);
	if(block_write(sb.directory_offset, buffer) == -1) 
	{
		return -1;
	}

	if(close_disk(disk_name) == -1) 
	{
		return -1;
	}

	return 0;	
}

int mount_fs(const char * disk_name)
{
	if(open_disk(disk_name) == -1)
	{
		return -1;
	}

	if(is_mounted == true)
	{
		return -1;
	}

	if(disk_name == NULL)
	{
		return -1;
	}

	char buffer[BLOCK_SIZE * 10];
	memset(buffer, 0, sizeof(buffer));
	super_block = (struct Superblock *) malloc(sizeof(struct Superblock));
	if(block_read(0, buffer) == -1)
	{
		free(super_block);
		return -1;
	}
	memcpy((void*) super_block,(void*) buffer, sizeof(struct Superblock));	
		
	memset(buffer, 0, sizeof(buffer));
	used_block_bitmap = (char*) malloc(DISK_BLOCKS);
	if(block_read(super_block->used_block_bitmap_offset, buffer) == -1)
	{
		free(used_block_bitmap);
		return -1;
	}
	memcpy((void *) used_block_bitmap,(void *) buffer, sizeof(char) * super_block->used_block_bitmap_count);
	
	memset(buffer, 0, sizeof(buffer));
	inode_table = (struct Inode*) malloc(sizeof(struct Inode) * DISK_BLOCKS);
	for(int i = 0; i < super_block->inode_metadata_blocks; i++)
	{
		if(block_read(super_block->inode_metadata_offset + i, buffer) == -1)
		{
			free(inode_table);
			return -1;
		}
		memcpy((void *) (inode_table + i * BLOCK_SIZE / sizeof(struct Inode)), (void *) buffer, BLOCK_SIZE);
	}

	memset(buffer, 0, sizeof(buffer));
	directory =  (struct Directory *) malloc(sizeof(struct Directory) * MAX_FILES);
	if(block_read(super_block->directory_offset, buffer) == -1) 
	{
		free(directory);
		return -1;
	}
	memcpy((void *) directory, (void *) buffer, sizeof(struct Directory) * MAX_FILES);

	for(int i = 0; i < MAX_FILE_DESCRIPTOR; i++)
	{
		fd[i].isused = false;
	}

	is_mounted = true;

	return 0;
}

int umount_fs(const char * disk_name)
{
	if(disk_name == NULL)
	{
		return -1;
	}
	
	if(is_mounted == false)
	{
		return -1;
	}

	char buffer[BLOCK_SIZE * 10];
	memset(buffer, 0, sizeof(buffer));

	memcpy((void*) buffer , (void*) super_block, sizeof(struct Superblock));
	if(block_write(0, buffer) == -1) 
	{
		return -1;
	}

	memset(buffer, 0, sizeof(buffer));

	for(int i = 0; i < super_block->inode_metadata_blocks; i++)
	{
		memcpy((void *) buffer, (void *) (inode_table + i * BLOCK_SIZE / sizeof(struct Inode)), BLOCK_SIZE); 
		if(block_write(super_block->inode_metadata_offset + i, buffer) == -1)
		{
			return -1;
		}
	}
	
	memset(buffer, 0, sizeof(buffer));
	memcpy((void *) buffer, (void *) directory, sizeof(struct Directory) * MAX_FILES);
	if(block_write(super_block->directory_offset, buffer) == -1)
	{
		return -1;
	}

	int i = 0;
	while(i < MAX_FILE_DESCRIPTOR)
	{
		if(fd[i].isused == true)
		{
			fd[i].isused = false;
			fd[i].inode_num = -1;
			fd[i].offset = 0;
		}
		i++;
	}

	if(close_disk() == -1)
	{
		return -1;
	}

	is_mounted = false;

	return 0;
}

int fs_open(const char * name)
{
	int file_num;
	for(file_num = 0; file_num < MAX_FILE_DESCRIPTOR; file_num++)
	{
		if(!fd[file_num].isused)
		{
			break;
		}
	}

	if(file_num == MAX_FILE_DESCRIPTOR)
	{
		return -1;
	}
	
	int i = 0;
	while(i < MAX_FILES)
	{
		if(strcmp(name, directory[i].file_names) == 0)
		{
			fd[file_num].isused = true;
			fd[file_num].inode_num = directory[i].inode_num;
			fd[file_num].offset = 0;
			directory[i].count = directory[i].count + 1;
			return file_num;
		}
		i++;
	}

	return -1;
}	

int check_File(int fildes)
{
	if(fildes < 0 || fildes > MAX_FILE_DESCRIPTOR || !fd[fildes].isused)
	{
		return -1;
	}
	return 0;
}

int find_Index(int fildes)
{
	int i;
	for(i = 0; i < MAX_FILES; i++)
	{
		if(directory[i].is_used && directory[i].inode_num == fd[fildes].inode_num)
		{
			return i;
		}
	}
	return -1;
}

int fs_close(int fildes)
{
	int drct_num = find_Index(fildes);

	if(drct_num == -1 || !fd[fildes].isused)
	{
		return -1;
	}

	fd[fildes].isused = false;
	fd[fildes].inode_num = -1;
	fd[fildes].offset = 0;
	directory[drct_num].count = directory[drct_num].count - 1;
	
	return 0;
}


int fs_create(const char * name)
{
	if(strlen(name) > MAX_FILE_LENGTH) 
	{
		return -1;
	}

	int i = 0, count = 0;
	do {
		if(directory[i].is_used == true)
		{
			
			if(strcmp(name, directory[i].file_names) == 0)
			{
				return -1;
			}
			count++;
		}
		i++;

	} while(i < MAX_FILES);

	if(count == MAX_FILES)
	{
		return -1;
	}

	i = super_block->data_offset;
	do {
		if(used_block_bitmap[i] == 0)
		{
			used_block_bitmap[i] = 1;
			inode_table[i].direct_offset = END_OF_FILE;
			inode_table[i].file_type = 1;
		}

		int j = 0;
		do {
			if(directory[j].is_used == false)
			{
				directory[j].is_used = true;
				directory[j].inode_num = i;
				directory[j].d_size = 0;
				directory[j].count = 0;
				strcpy(directory[j].file_names, name);
				return 0;
			}
			j++;

		} while(j < MAX_FILES);

		i++;

	} while(i < DISK_BLOCKS);

	return -1;
}	

int fs_delete(const char * name)
{
	int drct_num = 0;

	while(drct_num < MAX_FILES)
	{
		if(strcmp(directory[drct_num].file_names, name) == 0)
		{
			if(directory[drct_num].count >= 1)
			{
				return -1;
			}		
			break;
			
		}
		drct_num++;
	}
	
	if(drct_num == MAX_FILES)
	{
		return -1;
	}

	int inode_number;

	for(inode_number = directory[drct_num].inode_num; inode_table[inode_number].direct_offset != END_OF_FILE; inode_number = inode_table[inode_number].direct_offset)
	{
		used_block_bitmap[inode_number] = 0;
		inode_table[inode_number].direct_offset = NOT_IN_USE;
	}
	used_block_bitmap[inode_number] = 0;
	inode_table[inode_number].direct_offset = NOT_IN_USE;

	directory[drct_num].is_used = false;
	directory[drct_num].inode_num = END_OF_FILE;
	directory[drct_num].d_size = 0; 
	directory[drct_num].count = 0;
	memset(directory[drct_num].file_names, '\0', MAX_FILE_LENGTH);

	return 0;
}

int fs_read(int fildes, void * buf, size_t nbyte)
{
	if(check_File(fildes) == -1 || nbyte <= 0)
	{
		return -1;
	}

	int drct_num = find_Index(fildes);

	if(drct_num == -1)
	{
		return -1;
	}

	int inode_number = directory[drct_num].inode_num;
	
	int new_offset = fd[fildes].offset + nbyte;

	int bytes_to_read = nbyte;

	if(new_offset > directory[drct_num].d_size)
	{
		bytes_to_read = directory[drct_num].d_size - fd[fildes].offset;
	}
	int bytes_read = 0;
	int read_offset = fd[fildes].offset;
	int read_inode = inode_number;

	char buffer[BLOCK_SIZE];

	while(bytes_to_read > 0)
	{
		if(block_read(read_inode, buffer) == -1)
		{
			return -1;
		}
		
		int start_pos = read_offset % BLOCK_SIZE;
		int end_pos = start_pos + bytes_to_read;
		if(end_pos > BLOCK_SIZE)
		{
			end_pos = BLOCK_SIZE;
		}

		memcpy((char *) buf + bytes_read, buffer + start_pos, end_pos - start_pos);

		bytes_to_read -= (end_pos - start_pos);
		bytes_read += (end_pos - start_pos);
		read_offset += (end_pos - start_pos);
		read_inode = inode_table[read_inode].direct_offset;
	}

	if(fd[fildes].offset + nbyte <= directory[drct_num].d_size)
	{
		fd[fildes].offset += nbyte;
	}
	else
	{
		fd[fildes].offset = directory[drct_num].d_size;
	}

	return bytes_read;
}

int fs_write(int fildes, void * buf, size_t nbyte)
{
	if(check_File(fildes) == -1 || nbyte <= 0)
	{
		return -1;
	}

	int drct_num = find_Index(fildes);
	
	if(drct_num == -1)
	{
		return -1;
	}

	int inode_number = directory[drct_num].inode_num;
	
	int new_offset = fd[fildes].offset + nbyte;

	if(new_offset > MAX_FILESIZE)
	{
		nbyte = (MAX_FILESIZE) - fd[fildes].offset;
		new_offset = fd[fildes].offset + nbyte;
	}
	
	int next_inode = inode_number;

	int i = 0, j = super_block->data_offset;
	while(i < ((new_offset - 1) / BLOCK_SIZE))
	{
		if(inode_table[next_inode].direct_offset == END_OF_FILE)
		{
			while(j < DISK_BLOCKS)
			{
				if(used_block_bitmap[j] == 0)
				{
					used_block_bitmap[j] = 1;
					inode_table[next_inode].direct_offset = j;
					inode_table[j].direct_offset = END_OF_FILE;
					break;
				}
				else if(j == DISK_BLOCKS - 1)
				{
					return -1;
				}
				j++;
			}
		}
		i++;
		next_inode = inode_table[next_inode].direct_offset;
	}
	
	if(new_offset > directory[drct_num].d_size)
	{
		directory[drct_num].d_size = new_offset;
	}

	int read_inode = inode_number;
	int write_inode = inode_number;
	int bytes_remaining = nbyte;
	int bytes_written = 0;
	int read_offset = fd[fildes].offset;
	int write_offset = fd[fildes].offset;

	char buffer[BLOCK_SIZE];

	while(bytes_remaining > 0)
	{

		if(block_read(read_inode, buffer) == -1)
		{
			return -1;
		}

		int start_pos = read_offset % BLOCK_SIZE;
		int end_pos = start_pos + bytes_remaining;
		if(end_pos > BLOCK_SIZE)
		{
			end_pos = BLOCK_SIZE;
		}

		memcpy(buffer + start_pos, (char *) buf + bytes_written, end_pos - start_pos);
		
		if(block_write(write_inode, buffer) == -1)
		{
			return -1;
		}

		bytes_remaining -= (end_pos - start_pos);
		bytes_written += (end_pos - start_pos);
		read_offset += (end_pos - start_pos);
		write_offset += (end_pos - start_pos);

		read_inode = inode_table[read_inode].direct_offset;
		write_inode = inode_table[write_inode].direct_offset;
	}


	fd[fildes].offset += nbyte;

	return nbyte;
}

int fs_get_filesize(int fildes)
{
	int i = 0;
	while(i < MAX_FILES)
	{
		if(directory[i].is_used && directory[i].inode_num == fd[fildes].inode_num)
		{
			return directory[i].d_size;
		}
		i++;
	}
	return -1;	
}

int fs_listfiles(char *** files)
{
	int i = 0, file_count = 0;
	while(i < MAX_FILES)
	{
		if(directory[i].is_used == true)
		{
			file_count++;
		}
		i++;
	}

	*files = (char **) malloc((file_count + 1) * sizeof(char*));
	if(*files == NULL)
	{
		return -1;
	}

	i = 0;
	while(i < file_count)
	{
		(*files)[i] = (char *) malloc((MAX_FILE_LENGTH + 1) * sizeof(char));
		if(files[0][i] == NULL)
		{
			int j = 0;
			while(j < i)
			{
				free((*files)[j]);
				j++;
			}
			free(*files);
			return -1;
		}
		i++;
	}

	(*files)[file_count] = (char *) malloc(sizeof(char));

	int index = 0;
	i = 0;
	while(i < MAX_FILES && index < file_count)
	{
		if(directory[i].is_used == true)
		{
			strncpy((*files)[index], directory[i].file_names, sizeof(char[MAX_FILE_LENGTH + 1]));
			index++;
		}
		i++;
	}

	(*files)[file_count] = NULL;

	return 0;		
}


int fs_lseek(int fildes, off_t offset)
{
	if(check_File(fildes) == -1)
	{
		return -1;
	}
	int i = 0;
	while(i < MAX_FILES)
	{
		if(directory[i].is_used == true && directory[i].inode_num == fd[fildes].inode_num)
		{
			if(offset < 0 || offset > directory[i].d_size)
				return -1;
			else
			{
				fd[fildes].offset = offset;
				return 0;
			}
		}
		i++;
	}
	return -1;
}

int fs_truncate(int fildes, off_t length)
{
	if(check_File(fildes) == -1)
	{
		return -1;
	}
	int drct_num = find_Index(fildes);
	if(length > directory[drct_num].d_size)
	{
		return -1;
	}

	int inode_number = directory[drct_num].inode_num;
	directory[drct_num].d_size = length;

	int i = 0, next_inode = inode_number; 
	while(i < (length - 1) / BLOCK_SIZE)
	{
		next_inode = inode_table[next_inode].direct_offset;
		i++;
	}

	int old_inode, truncate = next_inode;
	while(next_inode != END_OF_FILE)
	{
		old_inode = next_inode;
		next_inode = inode_table[next_inode].direct_offset;
		inode_table[old_inode].direct_offset = NOT_IN_USE;
	}

	inode_table[truncate].direct_offset = END_OF_FILE;

	return 0;

}
