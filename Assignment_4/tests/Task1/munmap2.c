#include<ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

  int pageSize = 4096;

  char * mapping1 = mmap(NULL, pageSize*3, PROT_READ|PROT_WRITE, 0);
 
  // vm_area count should be 1
  pmap(1);

  int val = munmap(mapping1 + pageSize , pageSize);

  // Area unmapped is inside the vm_area. So vm_area will be split into two.
  // vm_area count should be 2
  pmap(1);

  return 0;
}
