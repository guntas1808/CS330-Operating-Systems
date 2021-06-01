#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
  int pageSize = 1;

  char *addr1 = mmap(NULL, pageSize, PROT_READ|PROT_WRITE, 0);

  pmap(1);

  int munmap1 = munmap(addr1, 1);

  pmap(1);

  return 0;
}
