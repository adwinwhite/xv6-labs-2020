// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  /* initlock(&bcache.lock, "bcache"); */

  // Create linked list of buffers
  /* bcache.head.prev = &bcache.head; */
  /* bcache.head.next = &bcache.head; */
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    /* b->next = bcache.head.next; */
    /* b->prev = &bcache.head; */
    initsleeplock(&b->lock, "buffer");
    /* bcache.head.next->prev = b; */
    /* bcache.head.next = b; */
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  /* acquire(&bcache.lock); */

  // Is the block already cached?
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
      if (b->dev == dev && b->blockno == blockno) {
          acquiresleep(&b->lock);
          b->refcnt++;
          return b;
      }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  uint64 oldest_timestamp = ticks;
  struct buf *lub = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
      acquiresleep(&b->lock);
      if (b->refcnt == 0 && b->timestamp < oldest_timestamp) {
          oldest_timestamp = b->timestamp;
          if (lub) {
              releasesleep(&lub->lock);
          }
          lub = b;
      } else {
          releasesleep(&b->lock);
      }
  }

  if (lub) {
      lub->dev = dev;
      lub->blockno = blockno;
      lub->valid = 0;
      lub->refcnt = 1;
      return lub;
  }
      


  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  b->timestamp = ticks;
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  b->refcnt--;
  releasesleep(&b->lock);

}

void
bpin(struct buf *b) {
  acquiresleep(&b->lock);
  b->refcnt++;
  releasesleep(&b->lock);
}

void
bunpin(struct buf *b) {
  acquiresleep(&b->lock);
  b->refcnt--;
  releasesleep(&b->lock);
}


