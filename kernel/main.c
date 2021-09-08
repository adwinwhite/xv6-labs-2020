#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    printf("after kinit\n");
    kvminit();       // create kernel page table
    printf("after kvminit\n");
    kvminithart();   // turn on paging
    printf("after kvminithart\n");
    procinit();      // process table
    printf("after procinit\n");
    trapinit();      // trap vectors
    printf("after trapinit\n");
    trapinithart();  // install kernel trap vector
    printf("after trapinithart\n");
    plicinit();      // set up interrupt controller
    printf("after plicinit\n");
    plicinithart();  // ask PLIC for device interrupts
    printf("after plicinithart\n");
    binit();         // buffer cache
    printf("after binit\n");
    iinit();         // inode cache
    printf("after iinit\n");
    fileinit();      // file table
    printf("after fileinit\n");
    virtio_disk_init(); // emulated hard disk
    printf("after virtio_disk_init\n");
    userinit();      // first user process
    printf("after userinit\n");
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
