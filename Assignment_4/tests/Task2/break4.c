#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	int pageSize = 4096;
	u32 MB = 1 << 20;

	u64 aligned = 0x180400000; // 2MB aligned address

	char *paddr = mmap((void *)aligned, 2*MB, PROT_READ|PROT_WRITE, 0);
	if(paddr != (char*)aligned)
	{
		printf("Test failed");
		return 0;
	}
	pmap(1);

	paddr = mmap((void *)aligned, 2*MB, PROT_READ, 0);
	if(paddr != (char*)aligned+ (2*MB))
	{
		printf("Test failed");
		return 0;
	}
	pmap(1);

	
	char *hpaddr = (char *) make_hugepage(paddr, 2*MB , PROT_READ|PROT_WRITE, 1);
	if(hpaddr != (char*)aligned+(2*MB))
	{
		printf("Test failed");
		return 0;
	}
	pmap(1);


	paddr = mmap((void *)aligned, 2*MB, PROT_READ|PROT_WRITE, 0);
	if(paddr != (char*)aligned+ (4*MB))
	{
		printf("Test failed");
		return 0;
	}
	pmap(1);
	

	int ret = break_hugepage((char*)aligned, 6*MB);
	if(ret != 0) {
		printf("Test failed");
		return 0;
	}
	pmap(1);

	return 0;
}
