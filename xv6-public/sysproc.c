#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "wmap.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// wmap() is a system call that can be used by a user process to ask the
// operating system kernel to map either files or devices into the memory (i.e.,
// address space) of that process. The mmap() system call can also be used to
// allocate memory (an anonymous mapping). A key point here is that the mapped
// pages are not actually brought into physical memory until they are
// referenced; thus wmap() can be used to implement "lazy" loading of pages into
// memory (demand paging).
//
int sys_wmap(void)
{
  // uint wmap(uint addr, int length, int flags, int fd);
  // [DONE]In this project, you only implement the case with MAP_FIXED. Return error
  // [DONE]MAP_SHARED should always be set. If it's not, return error.

  // [DONE]MAP_FIXED. Return error if this flag is not set. 
  
  // Also, a valid addr will be a multiple of page size and within 0x60000000
  // and 0x80000000


  // PTE_P indicates whether the PTE is present Map file pages to process
  // virtual memory. 

  // The file memlayout.h declares the constants for xv6’s memory layout, and
  // macros to convert virtual to physical addresses. 
  
  // Which virtual address? 
  // Ans: We will be provided the virtual address addr.
  
  // Which offset in the file should be mapped? 
  // Ans: The entire file should be mapped.

  // How do we know if a user has accessed a page? We need to create a new data
  // structure to track that

  // kalloc returns physical pages but kernel va of the pa

  // We have virtual and physical addresses, we can map them using mappages.
  // mappages maps only one page. We need to map all the pages one by one.
  

  // mappages internally calls walkpagedir to get the PTE of the virtual
  // address. It then modifies the PTE to hold the physical address and modify
  // the flags accordingly

  int length, flags, fd;
  int _iaddr = 0;
  // uint *_uaddr;
  // uint addr;
  // int parent_bufsize, child_bufsize;

  if(argint(0, &_iaddr) < 0 ||
      // if(argint(1, &length) < 0 ||
      argint(1, &length) < 0 ||
      argint(2, &flags) < 0 ||
      argint(3, &fd) < 0) {
      return -1;  // Return error if unable to read arguments
  }
  // addr = (uint)_iaddr;
  // va = addr;

  return add_wmap_region(_iaddr, length, flags, fd);

  //   char *mem;

  //   mem = kalloc();
  //   memset(mem, 0, PGSIZE);
  //   mappages(myproc()->pgdir, va, PGSIZE, V2P(mem), PTE_WMAP_FLAGS);
  // }

  // return 0;
}


int sys_wunmap(void)
{
  int _iaddr;
  uint addr;

  if (argint(0, &_iaddr) < 0){
      return -1;  // Return error if unable to read arguments
  }
  addr = (uint) _iaddr;

  return free_wunmap(addr);
}

int sys_va2pa(void)
{
  int _iaddr;
  uint addr;

  if (argint(0, &_iaddr) < 0){
      return -1;  // Return error if unable to read arguments
  }
  addr = (uint) _iaddr;

  return va_to_pa(addr);
}

int sys_getwmapinfo(void)
{

  struct wmapinfo *map_info;

  if(argptr(0, (char**)&map_info, sizeof(struct wmapinfo)) < 0)
  {
    return FAILED;
  }

  int total_mappings = 0;
  int i, j;

  for(i = 0, j = 0; i < MAX_WMMAP_INFO; i++){
    if(myproc()->_wmap_deets[i].is_valid){

      map_info->addr[j] = myproc()->_wmap_deets[i].addr;
      map_info->length[j] = myproc()->_wmap_deets[i].length;
      map_info->n_loaded_pages[j] = myproc()->_wmap_deets[i].n_loaded_pages;
      total_mappings++;
      j++;
    }
  }
  map_info->total_mmaps = total_mappings;


  while(j < MAX_WMMAP_INFO){
    map_info->addr[j] = 0;
    map_info->length[j] = 0;
    map_info->n_loaded_pages[j] = 0;
    j++;
  }

  return SUCCESS;
}