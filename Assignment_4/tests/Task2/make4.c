
#include<ulib.h>

// checks if hugepage is correctly created
int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	int pageSize = 4096;
	u32 MB = 1 << 20;
	
	// 2MB aligned address
	u64 aligned = 0x180400000; 
	
	char *paddr = mmap((void *)aligned, 2 * MB, PROT_READ | PROT_WRITE, 0);
	pmap(1);
	
	char *hpaddr = (char *) make_hugepage(paddr, 2 * MB , PROT_READ | PROT_WRITE, 0);
	pmap(1);

	return 0;	
}
