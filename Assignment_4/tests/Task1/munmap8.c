#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
  int pageSize = 4096;
  char *addr1 = mmap(NULL, pageSize, PROT_READ|PROT_WRITE, 0);

  char *addr2 = mmap(NULL, pageSize, PROT_READ, 0);

  char *addr3 = mmap(NULL, pageSize, PROT_READ|PROT_WRITE, 0);

  pmap(1);


  int munmap1 = munmap(addr2, pageSize);

  pmap(1);

  char *addr4 = mmap(addr2, pageSize, PROT_READ|PROT_WRITE, 0);

  pmap(1);

  return 0;
}
