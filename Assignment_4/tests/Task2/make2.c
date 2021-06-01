#include<ulib.h>

// checks if given range has hugepage mappings
int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

	int pageSize = 4096;
	u32 MB = 1 << 20;
	
	// 2MB aligned address
	u64 aligned = 0x180400000; 
	
	char* paddr = mmap((void *) aligned, 2 * MB, PROT_READ|PROT_WRITE, 0);
	char* hpaddr1 = (char *) make_hugepage(paddr, 2 * MB , PROT_READ|PROT_WRITE, 0);
	paddr = mmap(hpaddr1 + 2 * MB, 2 * MB, PROT_READ|PROT_WRITE, 0);
	long ret = make_hugepage(hpaddr1, 4 * MB , PROT_READ|PROT_WRITE, 0);
	if(ret < 0)
		printf("OK, RETURNED ERROR\n");
	else
		printf("DIDNT RETURNED ERROR\n");
	return 0;
}
