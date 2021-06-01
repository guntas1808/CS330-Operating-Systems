#include<ulib.h>
int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
  int pageSize = 4096;
  u32 MB = 1 << 20;
  u64 aligned = 0x180400000;
  char *map1 = mmap(aligned, 4*MB, PROT_READ|PROT_WRITE, 0);
  pmap(1);
  char* map2 = (char *) make_hugepage(aligned, 2*MB , PROT_READ|PROT_WRITE, 0);
  //  Vm_Area count should be 2, different mapping types
  pmap(1);
  return 0;
}