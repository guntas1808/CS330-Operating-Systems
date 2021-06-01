#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
  int pageSize = 4096;
  u64 aligned = 0x180400000;
  char *addr1 = mmap(aligned, pageSize, PROT_READ|PROT_WRITE, MAP_FIXED);
  pmap(1);

  char *addr2 = mmap(aligned + 2*pageSize, pageSize, PROT_READ|PROT_WRITE, MAP_FIXED);
  //  Vm_Area count should be 2
  pmap(1);

  char* addr3 = mmap(aligned + pageSize, pageSize, PROT_READ|PROT_WRITE, MAP_FIXED);
  // should be merged together; Vm_Area count should be 1
  pmap(1);

  return 0;
}