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
struct spinlock nlink_lock;
                   // defined by kernel.ld.

struct run {
  struct run *next;
};



struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint16 *pagenlink(void *pa) {
  if (((uint64)pa % PGSIZE) != 0) {
      panic("pagenlink: not page aligned");
  } else if ((uint64)pa < new_end) {
      printf("pa: %p, new_end: %p\n", pa, new_end);
      panic("pagenlink: pa < new_end");
  } else if ((uint64)pa >= PHYSTOP) {
      panic("pagenlink: pa >= PHYSTOP");
  }
  uint64 ind = ((uint64)pa - PGROUNDUP(new_end)) / PGSIZE;
  return (uint16*)(ind * 2 + PGROUNDUP((uint64)end));
}

void pagenlink_inc(void *pa) {
    acquire(&nlink_lock);
    (*pagenlink(pa)) += 1;
    release(&nlink_lock);
}

void pagenlink_dec(void *pa) {
    acquire(&nlink_lock);
    (*pagenlink(pa)) -= 1;
    release(&nlink_lock);
}



void
kinit()
{
  initlock(&kmem.lock, "kmem");
  new_end = ((PGROUNDDOWN(PHYSTOP) - PGROUNDUP((uint64)end)) / PGSIZE) * 2 + PGROUNDUP((uint64)end);
  memset((void *)PGROUNDUP((uint64)end), 0, new_end - PGROUNDUP((uint64)end));
  freerange((void *)new_end, (void*)PHYSTOP);
  /* struct run *r = kmem.freelist; */
  /* if (r == 0) { */
      /* printf("kinit: freelist is 0\n"); */
  /* } */
  /* while (r) { */
      /* printf("%p\n", r); */
      /* r = r->next; */
  /* } */
}

void
klink(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < new_end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
  if ((uint64)kmem.freelist < new_end || (uint64)kmem.freelist >= PHYSTOP) {
    printf("pa: %p, freelist: %p\n", pa, kmem.freelist);
    panic("klink: out of range");
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    klink(p);
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

  acquire(&nlink_lock);
  uint16 *nlink = pagenlink(pa);

  if (*nlink == 1) {
      *nlink = 0;
      release(&nlink_lock);
      acquire(&kmem.lock);
      struct run *r;
      memset(pa, 1, PGSIZE);
      r = (struct run*)pa;
      r->next = kmem.freelist;
      kmem.freelist = r;
      if ((uint64)r->next != 0 && ((uint64)r->next < new_end || (uint64)r->next >= PHYSTOP)) {
        printf("pa: %p, r: %p, r->next: %p\n", pa, r, r->next);
        panic("kfree: out of range");
      }
      release(&kmem.lock);
  } else {
      (*nlink) -= 1;
      release(&nlink_lock);
  }
      /* if ((uint64)kmem.freelist < new_end || (uint64)kmem.freelist >= PHYSTOP) { */
        /* printf("pa: %p, freelist: %p\n", pa, kmem.freelist); */
        /* panic("kfree: out of range"); */
      /* } */
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
    /* printf("r: %p\n", r); */
    kmem.freelist = r->next;
    /* if ((uint64)kmem.freelist < new_end || (uint64)kmem.freelist >= PHYSTOP) { */
        /* panic("kalloc: out of range"); */
    /* } */
  }
  release(&kmem.lock);


    /* if ((uint64)r->next != 0 && ((uint64)r->next < new_end || (uint64)r->next >= PHYSTOP)) { */
        /* printf("r: %p, r->next: %p\n", r, r->next); */
        /* panic("kalloc: out of range"); */
    /* } */
  if(r) {
    pagenlink_inc(r);
    /* printf("r: %p, nlink_addr: %p\n", r, pagenlink(r)); */
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}
