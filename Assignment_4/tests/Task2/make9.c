#include<ulib.h>

// checks merging of hugepages
int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

	int pageSize = 4096;
	u32 MB = 1 << 20;
	
	// 2MB aligned address
	u64 aligned = 0x180400000; 
	
	char* paddr = mmap((void *) aligned, 2 * MB, PROT_READ|PROT_WRITE, 0);
	char* hpaddr1 = (char *) make_hugepage(paddr, 2 * MB , PROT_READ|PROT_WRITE, 0);

	paddr = mmap(hpaddr1 + 4 * MB, 2 * MB, PROT_READ|PROT_WRITE, 0);
	char* hpaddr2 = (char *) make_hugepage(paddr, 2*MB , PROT_READ|PROT_WRITE, 0);
	
	paddr = mmap(hpaddr1 + 2*MB, 2*MB, PROT_READ|PROT_WRITE, 0);
	char* hpaddr3 = (char *) make_hugepage(paddr, 2*MB , PROT_READ|PROT_WRITE, 0);

	pmap(1); 
	// contiguous huge pages of same permission should get merged. VM count should be 1.
	return 0;
}
