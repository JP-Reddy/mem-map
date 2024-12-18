// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;

  char page_ref[(KERNBASE / PGSIZE) * 2];
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}

void
kpage_ref_reset(uint pa)
{
  if(kmem.use_lock)
    acquire(&kmem.lock);
  
  kmem.page_ref[pa / PGSIZE] = 0;

  if(kmem.use_lock)
    release(&kmem.lock);
}

int
kpage_ref_inc(uint pa)
{
  if(kmem.use_lock)
    acquire(&kmem.lock);

  uint frame = pa / PGSIZE;
  int status;

  if (frame < sizeof(kmem.page_ref))
  {
    kmem.page_ref[frame]++;
    status = 0;
  }
  else
  {
    status = -1;
  }

  if(kmem.use_lock)
    release(&kmem.lock);

  return status;
}

int
kpage_ref_dec(uint pa)
{
  if(kmem.use_lock)
    acquire(&kmem.lock);

  uint frame = pa / PGSIZE;
  int status;

  if (frame < sizeof(kmem.page_ref))
  {
    char *ref = &kmem.page_ref[frame];

    if (*ref > 0)
    {
      --*ref;
      status = 0;
    }
    else
    {
      status = -2;
    }
  }
  else
  {
    status = -1;
  }

  if(kmem.use_lock)
    release(&kmem.lock);

  return status;
}

int
kpage_ref_cnt(uint pa)
{
  if(kmem.use_lock)
    acquire(&kmem.lock);

  uint frame = pa / PGSIZE;
  int status;

  if (frame < sizeof(kmem.page_ref))
  {
    status = kmem.page_ref[frame];
  }
  else
  {
    status = -1;
  }

  if(kmem.use_lock)
    release(&kmem.lock);

  return status;
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);

  char *va = (char*)r;

#ifndef NO_COW

  // Init the ref count to zero on page allocation
  kpage_ref_reset(V2P(va));

#endif

  return va;
}

