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
  struct spinlock lock;
  struct run *freelist;
} kmem;

int reference_counter[PHYSTOP/PGSIZE]; //You can find the index with (uint64) pa/PGSIZE)

void reference_counter_add(uint64 pa){
  acquire(&kmem.lock);  //Avoid some race conditions

  if(pa>PHYSTOP){
    panic("Non valid pa!");
  }else if(reference_counter[pa/PGSIZE]<1){
    panic("No reference in the page!");
  }
  reference_counter[pa/PGSIZE]++;
  
  release(&kmem.lock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    reference_counter[(uint64)p/PGSIZE] = 1;  //Initialize a page = 1 because the proccess is the only one it is referencing
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r = (struct run *) pa;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  if (reference_counter[(uint64) r/PGSIZE] < 1)
    panic("No reference in this page!");
  reference_counter[(uint64) r/PGSIZE]--;
  release(&kmem.lock);

  if(reference_counter[(uint64) r/PGSIZE] == 0){ //Finally the page can be deleted as noone references it
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
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
  
  if(r!=0){
    if(reference_counter[(uint64) r/PGSIZE]!=0)
      panic("Can't kalloc because someone is referencing the page");
    reference_counter[(uint64) r/PGSIZE] = 1; //Initialization
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r!=0)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  return (void*)r;
}
