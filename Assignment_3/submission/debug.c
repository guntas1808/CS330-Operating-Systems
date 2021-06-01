#include <debug.h>
#include <context.h>
#include <entry.h>
#include <lib.h>
#include <memory.h>


/*****************************HELPERS******************************************/

/* 
 * allocate the struct which contains information about debugger
 *
 */
struct debug_info *alloc_debug_info()
{
	struct debug_info *info = (struct debug_info *) os_alloc(sizeof(struct debug_info)); 
	if(info)
		bzero((char *)info, sizeof(struct debug_info));
	return info;
}

/*
 * frees a debug_info struct 
 */
void free_debug_info(struct debug_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct debug_info));
}

/*
 * allocates memory to store registers structure
 */
struct registers *alloc_regs()
{
	struct registers *info = (struct registers*) os_alloc(sizeof(struct registers)); 
	if(info)
		bzero((char *)info, sizeof(struct registers));
	return info;
}

/*
 * frees an allocated registers struct
 */
void free_regs(struct registers *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct registers));
}

/* 
 * allocate a node for breakpoint list 
 * which contains information about breakpoint
 */
struct breakpoint_info *alloc_breakpoint_info()
{
	struct breakpoint_info *info = (struct breakpoint_info *)os_alloc(
		sizeof(struct breakpoint_info));
	if(info)
		bzero((char *)info, sizeof(struct breakpoint_info));
	return info;
}

/*
 * frees a node of breakpoint list
 */
void free_breakpoint_info(struct breakpoint_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct breakpoint_info));
}

/*
 * Fork handler.
 * The child context doesnt need the debug info
 * Set it to NULL
 * The child must go to sleep( ie move to WAIT state)
 * It will be made ready when the debugger calls wait_and_continue
 */
void debugger_on_fork(struct exec_context *child_ctx)
{
	child_ctx->dbg = NULL;	
	child_ctx->state = WAITING;	
}


/******************************************************************************/

/* This is the int 0x3 handler
 * Hit from the childs context
 */
long int3_handler(struct exec_context *ctx)
{
	//printk("Entered int3 handler!\n");
	
	struct exec_context* debugger_ctx= get_ctx_by_pid(ctx->ppid);

	if(debugger_ctx==NULL)
		return -1;

	u64 addr = ctx->regs.entry_rip;
	debugger_ctx->regs.rax = addr-1;
	//printk("Breakpoint Address = %x\n", addr);

	ctx->regs.entry_rsp = (ctx->regs.entry_rsp-8);
	*((u64*)(ctx->regs.entry_rsp)) = ctx->regs.rbp;


	u64 base_addr = ctx->regs.rbp;
	debugger_ctx->dbg->backtrace_count = 2;
	debugger_ctx->dbg->backtrace_buff[0] = addr-1;
	debugger_ctx->dbg->backtrace_buff[1] = *((u64*)(ctx->regs.entry_rsp+8));
	// printk("Base Pointer = %x , Return Address = %x\n", *((u64*)(ctx->regs.entry_rsp)), *((u64*)(ctx->regs.entry_rsp+8)));
	int pos = 2;
	while(*((u64*)(base_addr+8)) != END_ADDR && pos<MAX_BACKTRACE){
		// printk("Base Pointer = %x , Return Address = %x\n", *((u64*)base_addr), *((u64*)(base_addr+8)));
		debugger_ctx->dbg->backtrace_buff[pos] = *((u64*)(base_addr+8));
		debugger_ctx->dbg->backtrace_count++;
		pos++;
		base_addr = *((u64*)base_addr);
	}
	
	//printk("int3 handler done! scheduling debugger!\n");
	ctx->regs.rax = 0;

	ctx->state = WAITING;
	debugger_ctx->state = READY;

	schedule(debugger_ctx);
}

/*
 * Exit handler.
 * Called on exit of Debugger and Debuggee 
 */
void debugger_on_exit(struct exec_context *ctx)
{
	if(ctx->ppid){
		struct exec_context* debugger_ctx =  get_ctx_by_pid(ctx->ppid);
		debugger_ctx->regs.rax = CHILD_EXIT;

		debugger_ctx->state = READY;
		
		return;
	}else{
		struct breakpoint_info* node1 = ctx->dbg->head;
		struct breakpoint_info* node2 = NULL;
		while(node1){
			node2 = node1->next;
			free_breakpoint_info(node1);
			node1 = node2;
		}
		free_debug_info(ctx->dbg);
		return;
	}
}

/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx)
{
	ctx->dbg = alloc_debug_info();
	if(ctx->dbg==NULL)
		return -1;

	ctx->dbg->head = NULL;
	ctx->dbg->global_count = 0;
	ctx->dbg->breakpoint_count = 0;
	ctx->dbg->backtrace_count = 0;

	//printk("Debugger initialized!\n");

	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr)
{
	if(ctx==NULL || addr==NULL)
		return -1;

	struct breakpoint_info* temp_node1 = ctx->dbg->head;
	struct breakpoint_info* temp_node2 = NULL;
	while(temp_node1){
		if(temp_node1->addr == (u64)addr)
			break;
		temp_node2 = temp_node1;
		temp_node1 = temp_node1->next;
	}

	if(temp_node1){
		temp_node1->status = ENABLED;
		*((u64*)addr) = (*((u64*)addr) & ~0xff) | INT3_OPCODE;
		return 0;
	}

	struct breakpoint_info* breakpoint_info_node = alloc_breakpoint_info();
	
	if(breakpoint_info_node==NULL)
		return -1;

	breakpoint_info_node->num = ctx->dbg->global_count + 1;
	breakpoint_info_node->status = ENABLED;
	breakpoint_info_node->addr = (u64)addr;
	breakpoint_info_node->next = NULL;

	ctx->dbg->global_count++;
	ctx->dbg->breakpoint_count++;
	
	if(temp_node2){
		temp_node2->next = breakpoint_info_node;
	}else{
		ctx->dbg->head = breakpoint_info_node;
	}

	*(u64*)addr = (*((u64*)addr) & ~0xff) | INT3_OPCODE ;

	//printk("Breakpoint Set!\n");

	return 0;
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	if(ctx==NULL || addr==NULL)
		return -1;

	struct breakpoint_info* temp_node = ctx->dbg->head;
	struct breakpoint_info* temp_prev_node = NULL;
	while(temp_node){
		if(temp_node->addr == (u64)addr)
			break;
		temp_prev_node = temp_node;
		temp_node = temp_node->next;
	}
	if(temp_node == NULL)
		return -1;

	if(temp_prev_node){
		temp_prev_node->next = temp_node->next;
		temp_node->next = NULL;
	}else{
		ctx->dbg->head = temp_node->next;
		temp_node->next = NULL;
	}
	free_breakpoint_info(temp_node);
	
	ctx->dbg->breakpoint_count--;

	*((u64*)addr) = (*((u64*)addr) & ~0xff) | PUSHRBP_OPCODE;

	return 0;
}

/*
 * called from debuggers context
 */
int do_enable_breakpoint(struct exec_context *ctx, void *addr)
{
	// printk("Inside Enable Function!\n");
	if(ctx == NULL || addr == NULL)
		return -1;

	struct breakpoint_info* temp_node = ctx->dbg->head;
	while(temp_node != NULL){
		if(temp_node->addr == (u64)addr)
			break;
		temp_node = temp_node->next;
	}
	if(temp_node==NULL)
		return -1;
	
	temp_node->status = ENABLED;
	
	// printk("Enabling Breakpoint! Instruction = %x\n", *((u64*)addr));

	*((u64*)addr) = (*((u64*)addr) & ~0xff) | INT3_OPCODE;
	
	// printk("Breakpoint Enabled! Instruction = %x\n", *((u64*)addr));

	return 0;
}

/*
 * called from debuggers context
 */
int do_disable_breakpoint(struct exec_context *ctx, void *addr)
{
	if(ctx==NULL || addr ==NULL)
		return -1;
	// printk("Inside Disable Function! Brk 1\n");
	
	struct breakpoint_info* temp_node = ctx->dbg->head;
	while(temp_node!=NULL){
		// printk("Traversing LL! Addr = %x\n", temp_node->addr);
		if(temp_node->addr == (u64)addr)
			break;
		temp_node = temp_node->next;
	}
	// printk("Inside Disable Function! Brk 2\n");
	
	if(temp_node==NULL)
		return -1;
	
	// printk("Inside Disable Function! Brk 3\n");
	
	temp_node->status = DISABLED;
	// printk("Disabling Breakpoint! Instruction = %x\n", *((u64*)addr));
	

	*((u64*)addr) = (*((u64*)addr) & ~0xff) | PUSHRBP_OPCODE;

	// printk("Breakpoint Disabled! Instruction = %x\n", *((u64*)addr));
	
	return 0;
}

/*
 * called from debuggers context
 */ 
int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	if(ctx==NULL || ubp==NULL)
		return -1;

	struct breakpoint_info* node = ctx->dbg->head;
	int count = 0;
	while(node){
		if(node->status == ENABLED){
			ubp[count].num = node->num;
			ubp[count].status = node->status;
			ubp[count].addr = node->addr;
			count++;
		}
		node = node->next;
	}

	return count;
}

/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	if(ctx==NULL || regs==NULL)
		return -1;

	struct exec_context* debuggee_ctx;
	
	for(int i=1; i<7; ++i){
		if(i!=(ctx->pid)){
			debuggee_ctx = get_ctx_by_pid(i);
			
			if(debuggee_ctx && debuggee_ctx->ppid == ctx->pid)
				break;
		}
	}

	regs->r15 = debuggee_ctx->regs.r15;
	regs->r14 = debuggee_ctx->regs.r14;
	regs->r13 = debuggee_ctx->regs.r13;
	regs->r12 = debuggee_ctx->regs.r12;
	regs->r11 = debuggee_ctx->regs.r11;
	regs->r10 = debuggee_ctx->regs.r10;
	regs->r9 = debuggee_ctx->regs.r9;
	regs->r8 = debuggee_ctx->regs.r8;
	regs->rbp = debuggee_ctx->regs.rbp;
	regs->rdi = debuggee_ctx->regs.rdi;
	regs->rsi = debuggee_ctx->regs.rsi;
	regs->rdx = debuggee_ctx->regs.rdx;
	regs->rcx = debuggee_ctx->regs.rcx;
	regs->rbx = debuggee_ctx->regs.rbx;
	regs->rax = debuggee_ctx->regs.rax;
	regs->entry_rip = debuggee_ctx->regs.entry_rip-1;
	regs->entry_cs = debuggee_ctx->regs.entry_cs;
	regs->entry_rflags = debuggee_ctx->regs.entry_rflags;
	regs->entry_rsp = debuggee_ctx->regs.entry_rsp+8;
	regs->entry_ss = debuggee_ctx->regs.entry_ss;
	
	return 0;
}

/* 
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	u64* bt_buffer = (u64*)bt_buf;
	
	if(bt_buffer == NULL)
		return -1;

	for(int i=0; i<ctx->dbg->backtrace_count; ++i){
		bt_buffer[i] = ctx->dbg->backtrace_buff[i];
	}

	return ctx->dbg->backtrace_count;
}


/*
 * When the debugger calls wait
 * it must move to WAITING state 
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context *ctx)
{	
	//printk("Entered wait and continue!\n");
	struct exec_context* debuggee_ctx = NULL;
	
	for(int i=1; i<7; ++i){
		if(i!= ctx->pid){
			debuggee_ctx = get_ctx_by_pid(i);
			
			if(debuggee_ctx && debuggee_ctx->ppid == ctx->pid)
				break;
		}
	}
	
	if(debuggee_ctx==NULL || debuggee_ctx->ppid != ctx->pid)
		return -1;

	debuggee_ctx->state = READY;
	ctx->state = WAITING;

	//printk("Wait and continue: scheduling debugee process!\n");

	schedule(debuggee_ctx);
}

