#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;

#ifndef NO_COW

    // TODO SRINAG: panic if a is not U page

    // Increase the reference count for the phy page if a user-page is being
    // mapped
    if ((uint)a < KERNBASE)
    {
      if (kpage_ref_inc(pa) < 0)
      {
        panic("mappages: bad page");
      }
    }

#endif

    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz, int perm)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;

    // Segments are writeable by default on allocation. Remove write permission
    // if a read-only segment is being loaded.
    if(!(perm & PTE_W))
    {
      *pte &= ~PTE_W;
    }

    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");

#ifndef NO_COW

      // Only COW user space
      if(a < KERNBASE)
      {
        if(kpage_ref_dec(pa) < 0)
        {
          panic("deallocuvm: corrupt ref");
        }

        // If the page is no longer referred to in any page-table, deallocate
        // it
        if(kpage_ref_cnt(pa) == 0)
        {
          char *v = P2V(pa);
          kfree(v);
        }
      }
      else

#endif

      {
        char *v = P2V(pa);
        kfree(v);
      }

      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

int handle_pgflt_wmap(uint faulting_addr, int mapping_index)
{

  if(mapping_index == -1){
    panic("Accessed memory not in wmap regions. Segmentation Fault\n");
    // myproc()->killed = 1;
    return -1;
  }

  // pte_t *pte = walkpgdir(myproc()->pgdir, (const void *)faulting_addr, 1);
  char *mem = kalloc();

  uint page_number = (faulting_addr >> 12) & 0xFFFFF; 
  // TODO-Srinag Is the page_number correct. Verify arguments
  if(mappages(myproc()->pgdir, (void *)page_number, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
    return -1;
  }

  struct wmapinfo_internal wmap_info = myproc()->_wmap_deets[mapping_index];
  struct inode *ip = wmap_info.inode_ip;

  uint pgsize_offset = (faulting_addr - wmap_info.addr)/PGSIZE;
  uint offset = faulting_addr + pgsize_offset*PGSIZE;

  ilock(ip);

  // Read data
  //
  // TODO-Srinag Verify arguments
  // int bytes_read = readi(ip, (char *)page_number, 0, PGSIZE);
  int bytes_read = readi(ip, mem, offset, PGSIZE);
  if(bytes_read < 0) {
      // Handle error
      iunlock(ip);
      return -1;
  }

  return 0;
}

#ifndef NO_COW

int
handle_pgflt_cow(pte_t *pte)
{
  uint pa = PTE_ADDR(*pte);
  uint flags = PTE_FLAGS(*pte);
  int ref = kpage_ref_cnt(pa);

  // If there are other processes referring to this phy page, create a copy of
  // the phy page for the current process and point the virtual address to this
  // copy instead
  if (ref > 1)
  {
    // Allocate new page and copy data from original phy page
    char *mem;

    if((mem = kalloc()) == 0)
    {
      return -2;
    }

    memmove(mem, (char*)P2V(pa), PGSIZE);

    uint new_pa = V2P(mem);

    // Restore the write flag for the page copy
    flags &= ~PTE_COWRO;
    flags |= PTE_W;
    *pte = new_pa | flags;

    // Increment the reference counter for the page copy
    if(kpage_ref_inc(new_pa) < 0)
    {
      panic("handle_pgflt_cow: bad new page");
    }

    // Decrement the reference counter for the original page
    if(kpage_ref_dec(pa) < 0)
    {
      panic("handle_pgflt_cow: bad old page");
    }
  }
  // If there are no other processes referring to this phy page, simply restore
  // write access to it's sole virtual address mapping
  else if (ref == 1)
  {
    flags &= ~PTE_COWRO;
    flags |= PTE_W;
    *pte = pa | flags;
  }
  else
  {
    panic("handle_pgflt_cow: bad page");
  }

  return 0;
}

#endif

int
handle_pgflt(pde_t *pgdir, char *uva)
{
  // Reset address to the start of the VA page
  uva = (char*)PGROUNDDOWN((uint)uva);

  pte_t *pte = walkpgdir(pgdir, uva, 0);

  // Page really isn't mapped
  if(pte == 0)
  {
    // TODO SRINAG: after all debugging remove panic
    panic("handle_pgflt: pgflt");
    return -1;
  }

#ifndef NO_COW

  uint flags = PTE_FLAGS(*pte);

  // Trigger COW handler if the page was marked read-only by COW
  if (flags & PTE_COWRO)
  {
    int status = handle_pgflt_cow(pte);

    // The page-table has been changed, flush the TLB.
    lcr3(V2P(pgdir));
    return status;
  }

#endif

  // Trigger lazy allocation handler if... TODO JP
  uint faulting_address = (uint)uva;
  int index;
  
  if ((index = find_wmap_region(faulting_address)) >= 0)
  {
    int status = handle_pgflt_wmap(faulting_address, index);

    // The page-table has been changed, flush the TLB.
    lcr3(V2P(pgdir));
    return status;
  }

  return -3;
}

#ifndef NO_COW

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);

    // If the page is writeable, consider sharing the physical page with COW
    if (flags & PTE_W)
    {
      int wmap_idx = find_wmap_region(i);

      // If the page not mapped or is MAP_PRIVATE, we share the physical page
      // until there is a write to it by either processes
      if (wmap_idx < 0 || !myproc()->_wmap_deets[wmap_idx].is_shared)
      {
        // Install the COW trigger to trap and allocate separate physical pages
        // on page write
        flags |= PTE_COWRO;
        flags &= ~PTE_W;
        *pte = pa | flags;
      }
      // If the page is MAP_SHARED, we share the physical page regardless of
      // COW
      else
      {
        // Do nothing
      }
    }

    // If page is not writeable, the physical pages can be shared regardless of
    // WMAP mappings

    // Map the virtual address to the same physical address as the source table
    if(mappages(d, (void*)i, PGSIZE, pa, flags) < 0) {
      goto bad;
    }
  }

  // The source page table could be modified, flush TLB
  lcr3(V2P(pgdir));
  return d;

bad:
  // The source page table could be modified, flush TLB
  lcr3(V2P(pgdir));
  freevm(d);
  return 0;
}

#else

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

#endif

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

