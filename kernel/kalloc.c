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
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  uint64 blocksz;
  struct spinlock lock;
  struct run *freelist;
  struct run *freelists[NCPU];
  struct spinlock locks[NCPU];
} kmem;

// alignment required
int pa2cpu(void *pa) {
    return ((uint64)pa - PGROUNDUP((uint64)end)) / kmem.blocksz;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.blocksz = (PGROUNDDOWN(PHYSTOP) - PGROUNDUP((uint64)end)) / NCPU;
  freerange((void *)PGROUNDUP((uint64)end), (void*)PGROUNDDOWN(PHYSTOP));
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  int cpuid = pa2cpu(pa);

  acquire(&kmem.locks[cpuid]);
  r->next = kmem.freelists[cpuid];
  kmem.freelists[cpuid] = r;
  release(&kmem.locks[cpuid]);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int id = cpuid();
  acquire(&kmem.locks[id]);
  r = kmem.freelists[id];
  if(r) {
    kmem.freelists[id] = r->next;
      release(&kmem.locks[id]);
  } else {
      release(&kmem.locks[id]);
      for (int i = 0; i < NCPU; i++) {
          if (i == id) {
              continue;
          }
          acquire(&kmem.locks[i]);
          r = kmem.freelists[i];
          if (r) {
              kmem.freelists[i] = r->next;
              release(&kmem.locks[i]);
              break;
          } else {
              release(&kmem.locks[i]);
          }
      }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
