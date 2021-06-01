#include<ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

  int pages = 4096;

  char * mm1 = mmap(NULL, pages*2, PROT_READ|PROT_WRITE, 0);

  // vm_area count should be 1.
  pmap(1);

  mmap(mm1, pages*2, PROT_READ|PROT_WRITE, MAP_FIXED);

  // MAP_FIXED flag and alread mapped address, so should give error.
  pmap(1);

 return 0;
}
