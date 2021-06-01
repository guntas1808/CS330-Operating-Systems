#include<ulib.h>

// checks for holes 
int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

	int pageSize = 4096;
	u32 MB = 1 << 20;

	u64 aligned = 0x180400000; // 2MB aligned address

	char *addr1 = mmap((void *)aligned, 1 * MB, PROT_READ, 0);

	pmap(1);

	long ret = make_hugepage(addr1, 2 * MB , PROT_READ, 0);  // different permissions without force prot
	if (ret < 0) {
		printf("OK, RETURNED ERROR\n");  
	}
	else{
		printf("DIDNT RETURNED ERROR\n");
	}
	return 0;
}
