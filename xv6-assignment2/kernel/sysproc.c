#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"


#define MAX_WAITERS 16
#define NLOCKS 15

// The Israeli Lock Data Structure
struct israeli_lock {
  int active;                  // 1 if this lock is currently created/in use, 0 if free
  struct spinlock lk;          // Protects the lock's internal state
  int locked;                  // 1 if a process currently holds the baton, 0 if free
  int favoritism;              // The c% favoritism coefficient
  int owner_gid;               // The team ID of the process currently holding the lock
  
  // The Wait Queue
  struct proc* queue[MAX_WAITERS]; // Array of processes waiting in line
  int waiter_count;                // How many processes are currently waiting
};

// The global array of 15 locks required by the assignment
struct israeli_lock is_locks[NLOCKS];

void israeli_init(void) {
  for(int i = 0; i < NLOCKS; i++) {
    initlock(&is_locks[i].lk, "israeli_lk");
    is_locks[i].active = 0;
  }
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
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
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
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
  return kkill(pid);
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


// 1. Define the global randomness state and the lock
static uint random_state = 1;
struct spinlock rand_lock;

// 2. Implement the kernel functions
void lcg_srand(uint seed) {
  acquire(&rand_lock);
  random_state = seed;
  release(&rand_lock);
}

uint lcg_rand(void) {
  uint a = 1664525;
  uint b = 1013904223;
  uint current_val;

  acquire(&rand_lock);
  // Unsigned 32-bit integer overflow automatically handles the modulo 2^32
  random_state = a * random_state + b;
  current_val = random_state;
  release(&rand_lock);

  return current_val;
}

uint64 sys_lcg_srand(void) {
  int seed;
  // Just fetch the argument directly. No need to check for < 0!
  argint(0, &seed);
  
  lcg_srand((uint)seed);
  return 0; 
}

uint64 sys_lcg_rand(void) {
  return lcg_rand(); 
}

uint64 sys_setgid(void) {
  int gid;
  
  // Get the argument from userspace
  argint(0, &gid);
  
  // Set the gid of the currently running process
  myproc()->gid = gid;
  
  return 0;
}

uint64 sys_getgid(void) {
  // Return the gid of the currently running process
  return myproc()->gid;
}

uint64 sys_israeli_create(void) {
  int favoritism;
  
  // Just fetch the argument directly
  argint(0, &favoritism);
  
  // Search for a free lock slot
  for(int i = 0; i < NLOCKS; i++) {
    acquire(&is_locks[i].lk);
    
    if(is_locks[i].active == 0) {
      // Found a free slot! Initialize it.
      is_locks[i].active = 1;
      is_locks[i].locked = 0;
      is_locks[i].favoritism = favoritism;
      is_locks[i].owner_gid = -1;
      is_locks[i].waiter_count = 0;
      
      for(int j = 0; j < MAX_WAITERS; j++) {
        is_locks[i].queue[j] = 0;
      }
      
      release(&is_locks[i].lk);
      return i; // Return the lock_id
    }
    
    release(&is_locks[i].lk);
  }
  
  return -1; // No free locks available
}

uint64 sys_israeli_destroy(void) {
  int lock_id;
  
  // Just fetch the argument directly
  argint(0, &lock_id);
  
  if(lock_id < 0 || lock_id >= NLOCKS) return -1;

  acquire(&is_locks[lock_id].lk);
  is_locks[lock_id].active = 0;
  release(&is_locks[lock_id].lk);
  
  return 0;
}

uint64 sys_israeli_acquire(void) {
  int lock_id;
  argint(0, &lock_id);
  
  if(lock_id < 0 || lock_id >= NLOCKS) return -1;

  struct proc *p = myproc();
  acquire(&is_locks[lock_id].lk);

  // If the lock is totally free, just take it
  if(is_locks[lock_id].locked == 0 && is_locks[lock_id].waiter_count == 0) {
    is_locks[lock_id].locked = 1;
    is_locks[lock_id].owner_gid = p->gid;
    release(&is_locks[lock_id].lk);
    return 0;
  }

  // Otherwise, we must wait. Add ourselves to the queue.
  if(is_locks[lock_id].waiter_count >= MAX_WAITERS) {
    release(&is_locks[lock_id].lk);
    return -1; // Safety check
  }
  
  is_locks[lock_id].queue[is_locks[lock_id].waiter_count] = p;
  is_locks[lock_id].waiter_count++;

  // Go to sleep until we are removed from the queue by the releaser
  int in_queue = 1;
  while(in_queue) {
    // Sleep on the memory address of our own process structure
    sleep(p, &is_locks[lock_id].lk); 
    
    // When we wake up, check if we are still in the queue
    in_queue = 0;
    for(int i = 0; i < is_locks[lock_id].waiter_count; i++) {
      if(is_locks[lock_id].queue[i] == p) {
        in_queue = 1;
        break;
      }
    }
  }

  // If we broke out of the loop, the releaser passed the baton to us!
  release(&is_locks[lock_id].lk);
  return 0;
}

uint64 sys_israeli_release(void) {
  int lock_id;
  argint(0, &lock_id);
  
  if(lock_id < 0 || lock_id >= NLOCKS) return -1;

  acquire(&is_locks[lock_id].lk);

  // If no one is waiting, just unlock it and walk away
  if(is_locks[lock_id].waiter_count == 0) {
    is_locks[lock_id].locked = 0;
    is_locks[lock_id].owner_gid = -1;
    release(&is_locks[lock_id].lk);
    return 0;
  }

  int owner_gid = is_locks[lock_id].owner_gid;
  int c = is_locks[lock_id].favoritism;
  int chosen_index = 0; // Default to 0 (Strict FIFO)

  // Roll the dice for Favoritism!
  if((lcg_rand() % 100) < c) {
    // We got the probability! Look for someone in our group.
    for(int i = 0; i < is_locks[lock_id].waiter_count; i++) {
      if(is_locks[lock_id].queue[i]->gid == owner_gid) {
        chosen_index = i; // Found a teammate, they get to cut the line!
        break;
      }
    }
  }

  // Get the winning process
  struct proc *next_owner = is_locks[lock_id].queue[chosen_index];

  // Remove the winner from the queue by shifting everyone else forward
  for(int i = chosen_index; i < is_locks[lock_id].waiter_count - 1; i++) {
    is_locks[lock_id].queue[i] = is_locks[lock_id].queue[i + 1];
  }
  is_locks[lock_id].waiter_count--;

  // Pass the baton to the winner
  is_locks[lock_id].locked = 1;
  is_locks[lock_id].owner_gid = next_owner->gid;

  // Wake up ONLY the winning process
  wakeup(next_owner);

  release(&is_locks[lock_id].lk);
  return 0;
}

// --- Relay Race Scoreboard ---
#define MAX_TEAMS 10
int team_scores[MAX_TEAMS];
struct spinlock score_lock;
int score_lock_init = 0; // Tracks if we initialized the lock

uint64 sys_init_scores(void) {
  // Lazy initialization of the lock so we don't have to edit main.c!
  if(score_lock_init == 0) {
    initlock(&score_lock, "score_lock");
    score_lock_init = 1;
  }
  
  acquire(&score_lock);
  for(int i = 0; i < MAX_TEAMS; i++) {
    team_scores[i] = 0;
  }
  release(&score_lock);
  
  return 0;
}

uint64 sys_inc_score(void) {
  int team_id;
  
  // Fetch the argument
  argint(0, &team_id);
  
  if(team_id < 0 || team_id >= MAX_TEAMS) return -1;
  
  acquire(&score_lock);
  team_scores[team_id]++;
  int current_score = team_scores[team_id];
  release(&score_lock);
  
  return current_score;
}