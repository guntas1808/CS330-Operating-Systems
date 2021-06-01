#include<ulib.h>

// checks if hugepage is correctly created
// given address is not 2M aligned
int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	u32 MB = 1 << 20;
	
	u64 addr = 0x180201000; 
	
	char *paddr = mmap((void *)addr, 4 * MB, PROT_READ | PROT_WRITE, 0);
	pmap(1);

	long ret = make_hugepage((void *)paddr, 4 * MB , PROT_READ | PROT_WRITE, 0);
	pmap(1);
	return 0;	
}
