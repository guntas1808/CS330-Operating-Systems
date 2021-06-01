#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
  int pageSize = 4096;
  u64 unaligned = 0x180400003;
  char *addr1 = mmap(unaligned, pageSize, PROT_READ|PROT_WRITE, 0);
  pmap(1);

  return 0;
}