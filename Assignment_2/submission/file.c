#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>


/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/

void free_file_object(struct file *filep)
{
	if(filep)
	{
		os_page_free(OS_DS_REG ,filep);
		stats->file_objects--;
	}
}

struct file *alloc_file()
{
	struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
	file->fops = (struct fileops *) (file + sizeof(struct file)); 
	bzero((char *)file->fops, sizeof(struct fileops));
	file->ref_count = 1;
	file->offp = 0;
	stats->file_objects++;
	return file; 
}

void *alloc_memory_buffer()
{
	return os_page_alloc(OS_DS_REG); 
}

void free_memory_buffer(void *ptr)
{
	os_page_free(OS_DS_REG, ptr);
}

/* STDIN,STDOUT and STDERR Handlers */

/* read call corresponding to stdin */

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
	kbd_read(buff);
	return 1;
}

/* write call corresponding to stdout */

static int do_write_console(struct file* filep, char * buff, u32 count)
{
	struct exec_context *current = get_current_ctx();
	return do_write(current, (u64)buff, (u64)count);
}

long std_close(struct file *filep)
{
	filep->ref_count--;
	if(!filep->ref_count)
		free_file_object(filep);
	return 0;
}
struct file *create_standard_IO(int type)
{
	struct file *filep = alloc_file();
	filep->type = type;
	if(type == STDIN)
		filep->mode = O_READ;
	else
		filep->mode = O_WRITE;
	if(type == STDIN){
		filep->fops->read = do_read_kbd;
	}else{
		filep->fops->write = do_write_console;
	}
	filep->fops->close = std_close;
	return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
	int fd = type;
	struct file *filep = ctx->files[type];
	if(!filep){
		filep = create_standard_IO(type);
	}else{
		filep->ref_count++;
		fd = 3;
		while(ctx->files[fd])
			fd++; 
	}
	ctx->files[fd] = filep;
	return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/

/* File exit handler */
void do_file_exit(struct exec_context *ctx)
{
	/*TODO the process is exiting. Adjust the refcount
	of files*/
	for(unsigned int fd=0; fd<MAX_OPEN_FILES; ++fd){
		struct file* fileptr = ctx->files[fd];
		if(fileptr){
			if(fileptr->ref_count==1){
				free_file_object(fileptr);
			}else{
				fileptr->ref_count--;
			}
			ctx->files[fd]=NULL;
		}
	}
}

/*Regular file handlers to be written as part of the assignmemnt*/


static int do_read_regular(struct file *filep, char * buff, u32 count)
{
	/** 
	*  TODO Implementation of File Read, 
	*  You should be reading the content from File using file system read function call and fill the buf
	*  Validate the permission, file existence, Max length etc
	*  Incase of Error return valid Error code 
	**/
	u32 read_bytes = flat_read(filep->inode, buff, count, &(filep->offp));
	
	if(read_bytes>=0)
		filep->offp += read_bytes;
	
	return read_bytes;
}

/*write call corresponding to regular file */

static int do_write_regular(struct file *filep, char * buff, u32 count)
{
	/** 
	*   TODO Implementation of File write, 
	*   You should be writing the content from buff to File by using File system write function
	*   Validate the permission, file existence, Max length etc
	*   Incase of Error return valid Error code 
	* */
	u32 written_bytes = flat_write(filep->inode, buff, count, &(filep->offp));
	
	if(written_bytes>=0)
		filep->offp += written_bytes;

	return written_bytes;
}

long do_file_close(struct file *filep)
{
	/** TODO Implementation of file close  
	*   Adjust the ref_count, free file object if needed
	*   Incase of Error return valid Error code 
	*/
	if(filep==NULL)
		return -EINVAL;
	
	if(filep->ref_count==1){
		free_file_object(filep);
		return 0;
	}else{
		filep->ref_count--;
		return filep->ref_count;
	}
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
	/** 
	*   TODO Implementation of lseek 
	*   Set, Adjust the ofset based on the whence
	*   Incase of Error return valid Error code 
	* */
	unsigned int max_pos = filep->inode->file_size;
	if(whence==SEEK_SET){
		if(offset>max_pos)
			return -EINVAL;
		filep->offp = offset;
	}else if(whence==SEEK_CUR){
		if(filep->offp + offset > max_pos)
			return -EINVAL;
		filep->offp += offset;
	}else if(whence==SEEK_END){
		if(offset>0)
			return -EINVAL;
		filep->offp = max_pos;
	}else{
		return -EINVAL;
	}

	return filep->offp;
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{

	/**  
	*  TODO Implementation of file open, 
	*  You should be creating file(use the alloc_file function to creat file), 
	*  To create or Get inode use File system function calls, 
	*  Handle mode and flags 
	*  Validate file existence, Max File count is 16, Max Size is 4KB, etc
	*  Incase of Error return valid Error code 
	* */
	struct inode* file_inode = lookup_inode(filename);
	if(file_inode==NULL){ //if file does not exist
		if(flags<O_CREAT)
			return -EINVAL;
		file_inode = create_inode(filename, mode);
		
	}
	//check permissions
	u64 file_mode = flags&(file_inode->mode);
	
	//printk("~O_CREAT = %x\n file_mode = %x\n inode_mode = %x\n", ~O_CREAT, file_mode, file_inode->mode);	

	if(file_mode!=(flags & ~O_CREAT))//requested permissions must be a subset or equal to the inode mode.
		return -EACCES;
	
	//printk("Mode just after file creation %x\n", O_WRONLY);
	
	//if file size greater than 4KB
	if(file_inode->file_size>FILE_SIZE)
		return -EINVAL;

	//search for free file descriptor
	int file_descr;
	for(file_descr=3; file_descr<MAX_OPEN_FILES; ++file_descr){
		if(ctx->files[file_descr]==NULL){
			break;
		}
	}
	//if no file descriptor available
	if(file_descr==MAX_OPEN_FILES)
		return -EINVAL;
	
	struct file* fileptr = alloc_file();	//all checks performed, now allocating file object
	
	if(fileptr==NULL)
		return -ENOMEM;
		
	ctx->files[file_descr]=fileptr;

	fileptr->inode = file_inode;

	fileptr->mode = file_mode;	//set permissions
	fileptr->type = fileptr->inode->type; //assign type 	
	
	fileptr->fops->read = do_read_regular;//set function pointers
	fileptr->fops->write = do_write_regular;
	fileptr->fops->lseek = do_lseek_regular;
	fileptr->fops->close = do_file_close;

	return file_descr;
}

/**
 * Implementation dup 2 system call;
 */
int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
	/** 
	*  TODO Implementation of the dup2 
	*  Incase of Error return valid Error code 
	**/
	
	if(current->files[oldfd]==NULL)
		return -EINVAL;
	if(oldfd>=MAX_OPEN_FILES || newfd>=MAX_OPEN_FILES)
		return -EINVAL;

	if(current->files[newfd]->ref_count==1)
		free_file_object(current->files[newfd]);
	else
		current->files[newfd]->ref_count--;
	
	current->files[newfd] = current->files[oldfd];
	
	return newfd;
}

int do_sendfile(struct exec_context *ctx, int outfd, int infd, long *offset, int count) {
	/** 
	*  TODO Implementation of the sendfile 
	*  Incase of Error return valid Error code 
	**/
	if(outfd>=MAX_OPEN_FILES || infd>=MAX_OPEN_FILES)
		return -EINVAL;
	
	struct file* infileptr = ctx->files[infd];
	struct file* outfileptr = ctx->files[outfd]; 
	
	if(infileptr==NULL || outfileptr==NULL)

		return -EINVAL;
	
	if((infileptr->mode & O_READ)==0 || (outfileptr->mode & O_WRITE)==0)
		return -EACCES;

	char* buff = alloc_memory_buffer();
	if(buff==NULL)
		return -ENOMEM;
	
	int read_bytes;
	int write_bytes;
	if(offset){
		if(*offset > infileptr->inode->max_pos)
			return -EINVAL;

		read_bytes = flat_read(infileptr->inode, buff, count, (int*)offset);
		
		if(read_bytes<0)
			return read_bytes;
		
		*offset += read_bytes;
		write_bytes = flat_write(outfileptr->inode, buff, count, &(outfileptr->offp));

		if(write_bytes<0)
			return write_bytes;
	}else{
		read_bytes = flat_read(infileptr->inode, buff, count, &(infileptr->offp));
		
		if(read_bytes<0)
			return read_bytes;
		
		infileptr->offp += read_bytes;
		write_bytes = flat_write(outfileptr->inode, buff, count, &(outfileptr->offp));
		
		if(write_bytes<0)
			return write_bytes;
	}
	free_memory_buffer(buff);
	return write_bytes;
}

