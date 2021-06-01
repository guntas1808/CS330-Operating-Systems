#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
  int pageSize = 4096;
  char *addr1 = mmap(NULL, pageSize, PROT_READ|PROT_WRITE, 0);

  pmap(1);

  // Access flag is different should create a new vm_area
  char *addr2 = mmap(NULL, pageSize, PROT_WRITE, 0);

  // Vm_Area count should be 2
  pmap(1);

  int munmap1 = munmap(addr2, 4096);

   // Vm_Area count should be 1
  pmap(1);

  int munmap2 = munmap(addr1, 22);

  // Vm_Area count should be 0
  pmap(1);
  return 0;
}
