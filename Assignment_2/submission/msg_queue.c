#include <msg_queue.h>
#include <context.h>
#include <memory.h>
#include <file.h>
#include <lib.h>
#include <entry.h>



/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/

struct msg_queue_info *alloc_msg_queue_info()
{
	struct msg_queue_info *info;
	info = (struct msg_queue_info *)os_page_alloc(OS_DS_REG);
	
	if(!info){
		return NULL;
	}
	return info;
}

void free_msg_queue_info(struct msg_queue_info *q)
{
	os_page_free(OS_DS_REG, q);
}

struct message *alloc_buffer()
{
	struct message *buff;
	buff = (struct message *)os_page_alloc(OS_DS_REG);
	if(!buff)
		return NULL;
	return buff;	
}

void free_msg_queue_buffer(struct message *b)
{
	os_page_free(OS_DS_REG, b);
}

/**********************************************************************************/
/****************************HELPER FUNCTIONS**************************************/
/**********************************************************************************/

int get_member_pos(struct msg_queue_info* msg_queue, int pid){
	/*This helper funtion returns the position of the passed pid
	in the member_pid array.*/
	
	int member_pos;

	for(member_pos=0; member_pos<msg_queue->member_count; ++member_pos){
		if(msg_queue->member_pid[member_pos]==pid)
			break;
	}

	return member_pos;
}

int is_blocked(struct msg_queue_info* msg_queue, int from_pid, int to_pid){
	/*Checks whether process 'to_pid' has blocked process
	 'from_pid' or not*/

	int member_pos = get_member_pos(msg_queue, to_pid);
	
	for(int i=0; i<msg_queue->blocked_count[member_pos]; ++i){
		if(msg_queue->blocked[member_pos][i]==from_pid){
			//printk("BLOCKED\n");
			return 1;
		}
	}
	return 0;
}

int push_message(struct msg_queue_info* msg_queue, struct message* msg){
	/*Pushes a message to the message buffer(implemented as a queue)*/

	if(msg_queue->rear==MAX_MEMBERS-1 || msg==NULL)    //if buffer full or msg is null
		return 0;
	
	msg_queue->buffer[msg_queue->rear+1].from_pid = msg->from_pid;		//deep copy the message object msg into the buffer queue and
	msg_queue->buffer[msg_queue->rear+1].to_pid = msg->to_pid;		//increase the counts
	int ptr=0;
	while(ptr<MAX_TXT_SIZE){	
		msg_queue->buffer[msg_queue->rear+1].msg_txt[ptr] = msg->msg_txt[ptr];
		ptr++;
	}
		
	msg_queue->rear++;
	msg_queue->msg_count++;
	return 1;
}

void get_message(struct msg_queue_info* msg_queue, u32 pid, struct message* msg){
                /*Gets the earliest message sent to the process 'pid'*/

		if(msg_queue->rear<0){ //if buffer empty
			msg=NULL;
			return;
		}
                        

                int msg_ptr;
                for(msg_ptr=0; msg_ptr<=msg_queue->rear; ++msg_ptr){
                        if(msg_queue->buffer[msg_ptr].to_pid==pid)
                                break;
                }
    
                if(msg_ptr>msg_queue->rear){ //if no message to process 'pid' available
                        msg = NULL;
    			return;
		}	
		
		msg->from_pid = msg_queue->buffer[msg_ptr].from_pid;		//deep copy from the buffer into msg
		msg->to_pid = msg_queue->buffer[msg_ptr].to_pid;
                int txt_ptr=0;
		while(txt_ptr<MAX_TXT_SIZE){
			msg->msg_txt[txt_ptr] = msg_queue->buffer[msg_ptr].msg_txt[txt_ptr];
			++txt_ptr;
		}

                for(int i=msg_ptr; i<msg_queue->rear; ++i){	//shift the buffer and decrease counts
                        msg_queue->buffer[i] = msg_queue->buffer[i+1];
                }
                
		msg_queue->rear--;
                msg_queue->msg_count--;
                return;
}



/*********************************************************************************/
/*********************************************************************************/
/*********************************************************************************/

int do_create_msg_queue(struct exec_context *ctx)
{
	/** 
	 * TODO Implement functionality to
	 * create a message queue
	 **/
	int fd;
	for(fd=3; fd<MAX_OPEN_FILES; ++fd){		//get the smallest closed file descriptor
		if(ctx->files[fd]==NULL)
			break;
	}
	struct file* fileptr = alloc_file();		//allocate and initialize file object 
	//printk("file descriptor(sys call) = %d\n", fd);
	if(fileptr==NULL || fd == MAX_OPEN_FILES)
		return -ENOMEM;

	fileptr->fops->read=NULL;		
	fileptr->fops->write=NULL;
	fileptr->fops->lseek=NULL;
	fileptr->fops->close=NULL;
	
	struct msg_queue_info* queue_info = alloc_msg_queue_info();	//allocate and initialize message queue info object
	//initialization of message queue remaining
	if(queue_info==NULL)
		return -ENOMEM;
	
	queue_info->buffer = alloc_buffer();
	
	if(queue_info->buffer==NULL)
		return -ENOMEM;

	queue_info->rear=-1;
	queue_info->msg_count = 0;
	
	queue_info->member_count=1;
	queue_info->member_pid[0]=ctx->pid;
			
	
	for(int i=0; i<MAX_MEMBERS; ++i)
		queue_info->blocked[i]=NULL;
	
	for(int i=0; i<MAX_MEMBERS; ++i)
		queue_info->blocked_count[i]=0;
	
	fileptr->msg_queue = queue_info;

	ctx->files[fd] = fileptr;
	return fd;
}


int do_msg_queue_rcv(struct exec_context *ctx, struct file *filep, struct message *msg)
{
	/** 
	 * TODO Implement functionality to
	 * recieve a message
	 **/
	
	if(filep==NULL || msg==NULL)
		return -EINVAL;
	
	if(get_member_pos(filep->msg_queue, ctx->pid)>=MAX_MEMBERS)
		return -EINVAL;
	
	u32 pid = ctx->pid;
	

	get_message(filep->msg_queue, pid, msg);
	
	if(msg==NULL)		//if no message for the process in the queue return 0
		return 0;
	//printk("msg recieved by pid %d, msg from %d to %d = %s\n", ctx->pid, msg->from_pid, msg->to_pid, msg->msg_txt);	
	return 1;
}


int do_msg_queue_send(struct exec_context *ctx, struct file *filep, struct message *msg)
{
	/** 
	 * TODO Implement functionality to
	 * send a message
	 **/
	
	if(filep==NULL || msg==NULL)		
		return -EINVAL;
	
	if(ctx->pid != msg->from_pid)
		return -EINVAL;

	int member_pos = get_member_pos(filep->msg_queue, ctx->pid);
	
	if(member_pos>=filep->msg_queue->member_count){		//check process membership
		return -EINVAL;
	}
	
	struct msg_queue_info* msg_queue = filep->msg_queue;
	
	if(msg->to_pid==BROADCAST_PID){			//if a broadcast message then push a message into the queue
		int count=0;				//for every process which is a member and has not blocked this process
		for(member_pos=0; member_pos<msg_queue->member_count; ++member_pos){
			if(msg_queue->member_pid[member_pos]!=ctx->pid && !is_blocked(msg_queue, msg->from_pid, msg_queue->member_pid[member_pos])){
				msg->to_pid = msg_queue->member_pid[member_pos];
	
				if(push_message(msg_queue, msg)==0)
					return -EOTHERS;
				++count;			//counting the number of messages pushed
			}
		}
		
		return count;
	}else{						//else push one message if allowed
		if(is_blocked(msg_queue, msg->from_pid, msg->to_pid))
			return -EINVAL;

		if(push_message(filep->msg_queue, msg)==0)
			return -EOTHERS;

		//printk("message sent form %d to %d, mesg_count = %d\n", ctx->pid,  msg->to_pid);
		//	
		return 1;
	}
}

void do_add_child_to_msg_queue(struct exec_context *child_ctx)
{
	/** 
	 * TODO Implementation of fork handler 
	 **/
	//Assuming that member_count<MAX_MEMBERS
	for(int fd=3; fd<MAX_OPEN_FILES; ++fd){
		if(child_ctx->files[fd]!=NULL && child_ctx->files[fd]->msg_queue!=NULL){
			child_ctx->files[fd]->ref_count++;					
			struct msg_queue_info* queue_info = child_ctx->files[fd]->msg_queue;
			queue_info->member_pid[queue_info->member_count] = child_ctx->pid;
			queue_info->member_count++;
			
			//printk("fork! member count = %d\n", queue_info->member_count);
			
		}
	}
	
}

void do_msg_queue_cleanup(struct exec_context *ctx)
{
	/** 
	 * TODO Implementation of exit handler 
	 **/
	for(int i=3; i<MAX_OPEN_FILES; ++i){
		if(ctx->files[i]!=NULL && ctx->files[i]->msg_queue!=NULL)
			do_msg_queue_close(ctx, i);
	}
	
}

int do_msg_queue_get_member_info(struct exec_context *ctx, struct file *filep, struct msg_queue_member_info *info)
{
	/** 
	 * TODO Implementation of exit handler 
	 **/
	if(filep==NULL || info==NULL)
		return -EINVAL;
	
	if(filep->msg_queue==NULL)
		return -EINVAL;

	info->member_count = filep->msg_queue->member_count;
	for(int i=0; i<info->member_count; ++i){
		info->member_pid[i] = filep->msg_queue->member_pid[i];
	}
	
	return 0;
}


int do_get_msg_count(struct exec_context *ctx, struct file *filep)
{
	/** 
	 * TODO Implement functionality to
	 * return pending message count to calling process
	 **/

	if(filep==NULL || filep->msg_queue==NULL)
		return -EINVAL;
	
	int pid = ctx->pid;
	struct msg_queue_info* msg_queue = filep->msg_queue;
	
	int count=0;
	for(int i=0; i<msg_queue->msg_count; ++i){
		struct message* buff = msg_queue->buffer;
		if(buff[i].to_pid==pid)
			++count;
	} 
	//printk("query, msg_count = %d, returned count = %d pid = %d\n", filep->msg_queue->msg_count, count, ctx->pid);
	return count;
}

int do_msg_queue_block(struct exec_context *ctx, struct file *filep, int pid)
{
	/** 
	 * TODO Implement functionality to
	 * block messages from another process 
	 **/
	
	if(filep==NULL || filep->msg_queue==NULL)
		return -EINVAL;
	
	struct msg_queue_info* msg_queue = filep->msg_queue;
	
	int member_pos = get_member_pos(msg_queue, ctx->pid);
	
	if(member_pos==MAX_MEMBERS)		//check membership
		return -EINVAL;
	
	if(msg_queue->blocked[member_pos]==NULL){	//allocate array to store blocked processes if not allocated
		msg_queue->blocked[member_pos] = alloc_memory_buffer();
		
		if(msg_queue->blocked[member_pos]==NULL)
			return -ENOMEM;

		msg_queue->blocked[member_pos][0] = pid;
		msg_queue->blocked_count[member_pos] = 1;
	}else{
		int block_count = msg_queue->blocked_count[member_pos];		//add to 'pid' to the blocked processes
		msg_queue->blocked[member_pos][block_count] = pid;
		msg_queue->blocked_count[member_pos]++;
	}
		
	return 0;
}

int do_msg_queue_close(struct exec_context *ctx, int fd)
{
	/** 
	 * TODO Implement functionality to
	 * remove the calling process from the message queue 
	 **/
	if(ctx->files[fd]==NULL || ctx->files[fd]->msg_queue==NULL)
		return -EINVAL;
	
	struct msg_queue_info* msg_queue = ctx->files[fd]->msg_queue;	
	
	int member_pos = get_member_pos(msg_queue, ctx->pid);
	
	if(msg_queue->blocked[member_pos]){
		free_memory_buffer(msg_queue->blocked[member_pos]);
		msg_queue->blocked[member_pos] = NULL;
	}
	
	for(int i=member_pos; i<msg_queue->member_count; ++i){
		msg_queue->member_pid[i] = msg_queue->member_pid[i+1];
		msg_queue->blocked_count[i] = msg_queue->blocked_count[i+1];
		msg_queue->blocked[i] = msg_queue->blocked[i+1];
	}
	msg_queue->member_count--;
	
	struct file* filep = ctx->files[fd];
	ctx->files[fd] = NULL;
	
	filep->ref_count--;
	
	//printk("CLEANUP by pid %d, ref_count= %d, member_count=%d\n", ctx->pid, filep->ref_count, filep->msg_queue->member_count);	
	
	if(filep->ref_count==0 && filep->msg_queue->member_count==0){		//free both file object and message queue if message queue has no members
		free_msg_queue_buffer(filep->msg_queue->buffer);		//and file obj has no references
		free_msg_queue_info(filep->msg_queue);
		free_file_object(filep);
	}else if(filep->ref_count==0){					//otherwise free only the file object
		filep->msg_queue=NULL;
		free_file_object(filep);
	}
	
	
	return 0;
}
