// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define INDEX(pa) ((uint64)(pa) - (uint64)kmem.ref_count) / 1024

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  char *ref_count;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  kmem.ref_count = (char*)PGROUNDUP((uint64)end);
  uint64 total_pages = (PHYSTOP - (uint64)kmem.ref_count) / 1024;
  freerange(kmem.ref_count + total_pages, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kmem.ref_count[((uint64)p - (uint64)kmem.ref_count) / 1024] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 ref_index = INDEX(pa);
  acquire(&kmem.lock);
  if (kmem.ref_count[ref_index] == 0) {
    printf("page %p is freed twice!", pa);
    release(&kmem.lock);
    return;
  }

  if (--kmem.ref_count[ref_index] > 0) {
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
    uint64 ref_index = INDEX(r);
    if (kmem.ref_count[ref_index] != 0) {
      panic("kalloc");
    }
    kmem.ref_count[ref_index] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
ktouch(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("ktouch");

  uint64 ref_index = INDEX(pa);
  acquire(&kmem.lock);
  if (kmem.ref_count[ref_index] == 0) {
    panic("ktouch: access free page!");
  }
  kmem.ref_count[ref_index]++;
  release(&kmem.lock);
}

int
kref_count(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kref_count");
  
  uint ref_count = 0;
  uint64 ref_index = INDEX(pa);
  acquire(&kmem.lock);
  if (kmem.ref_count[ref_index] == 0) {
    panic("kref_count: access free page!");
  }
  ref_count = kmem.ref_count[ref_index];
  release(&kmem.lock);
  return ref_count;
}