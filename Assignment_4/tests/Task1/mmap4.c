#include<ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

  int pages = 4096;

  char * mm1 = mmap(NULL, pages*2, PROT_READ|PROT_WRITE, 0);

  // vm_area count should be 1.
  pmap(1);

  char* mm2 = mmap(mm1, pages*2, PROT_READ|PROT_WRITE, 0);

  // No MAP_FIXED flag, so should merge them together.
  pmap(1);

 return 0;
}
