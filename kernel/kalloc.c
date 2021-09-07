// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.

uint64 new_end;
                   // defined by kernel.ld.

struct run {
  struct run *next;
};



struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint16 *pagenlink(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < new_end || (uint64)pa >= PHYSTOP)
      panic("pagenlink");
  uint64 ind = ((uint64)pa - PGROUNDUP(new_end)) / PGSIZE;
  return (uint16*)(ind * 2 + PGROUNDUP((uint64)end));
}


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  new_end = ((PGROUNDDOWN(PHYSTOP) - PGROUNDUP((uint64)end)) / PGSIZE) * 2 + (uint64)end;
  memset((void *)PGROUNDUP((uint64)end), 0, new_end - PGROUNDUP((uint64)end));
  freerange((void *)new_end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{

  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < new_end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  /* memset(pa, 1, PGSIZE); */

  /* r = (struct run*)pa; */

  acquire(&kmem.lock);
  uint16 *nlink = pagenlink(pa);

  if (*nlink == 1) {
      *nlink = 0;
      struct run *r;
      r = (struct run*)pa;
      r->next = kmem.freelist;
      kmem.freelist = r;
      release(&kmem.lock);
      memset(pa, 1, PGSIZE);
  } else {
      *nlink -= 1;
      release(&kmem.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    *pagenlink(r) += 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
