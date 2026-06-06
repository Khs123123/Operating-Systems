#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// sys_flip_display: zero-copy page flip.
//
// Syscall argument 0: user virtual address of a page-aligned buffer
// that is exactly GPU_FB_PAGES (300) * PGSIZE bytes (i.e. 640x480x4 =
// 1,228,800 bytes).  The buffer must already be fully mapped in the
// calling process's address space.
uint64
sys_flip_display(void)
{
  uint64 addr;
  struct proc *p = myproc();

  // 1. Read the void *buf argument from userspace
  argaddr(0, &addr);

  // 2. Validate that buf is page-aligned
  if(addr % PGSIZE != 0)
    return -1;

  // 3. Ensure the buffer doesn't overflow maximum virtual memory
  if(addr + 300 * PGSIZE > MAXVA)
    return -1;

  // 4. Call virtio_gpu_flip to re-point the device to the current process' buffer
  extern int virtio_gpu_flip(pagetable_t pagetable, uint64 va);
  if(virtio_gpu_flip(p->pagetable, addr) < 0)
    return -1;

  p->flipped = 1; // ADD THIS LINE: Mark that we've flipped to a user buffer

  // Return 0 on success
  return 0;
}

// sys_map_display: map the GPU's kernel framebuffer pages (fb[]) directly
// into the calling process's address space with PTE_U|PTE_R|PTE_W.
//
// Syscall argument 0: desired user virtual address (must be page-aligned).
//   Pass 0 to let the kernel auto-select the next available VA above p->sz.
//
// Returns the mapped virtual address on success, (uint64)-1 on failure.
uint64
sys_map_display(void)
{
  uint64 addr;
  struct proc *p = myproc();

  // 1. Read the 0th argument from the user into the 'addr' variable
  // (In this xv6 template, argaddr returns void)
  argaddr(0, &addr);

  // 2. Validate or auto-select the virtual address
  if(addr == 0) {
    // Auto-select a page-aligned virtual address above the process size
    addr = PGROUNDUP(p->sz);
  } else {
    // User supplied an address. It must be page-aligned.
    if(addr % PGSIZE != 0)
      return -1;
      
    // It must not collide with existing mappings (must be above p->sz)
    if(addr < p->sz)
      return -1;
      
    // The entire 300-page framebuffer must fit inside valid virtual memory
    if(addr + 300 * PGSIZE > MAXVA)
      return -1;
  }

  // 3. Delegate the actual page-table mapping to a helper function in vm.c
  extern int map_framebuffer(pagetable_t pagetable, uint64 va);
  if(map_framebuffer(p->pagetable, addr) < 0)
    return -1;
  
  p->fb_va = addr;

  // 4. Return the newly mapped virtual address to the user
  return addr;
}