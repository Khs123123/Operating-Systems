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

uint64
sys_memsize(void)
{
    // הפונקציה myproc() מחזירה את מבנה הנתונים (PCB) של התהליך הנוכחי
    struct proc *p = myproc();

    // השדה sz מייצג את גודל הזיכרון של התהליך בבתים
    return p->sz;
}


extern struct proc proc[NPROC];
extern void swtch(struct context*, struct context*); 

uint64
sys_co_yield(void)
{
  int target_pid, value;
  struct proc *p = myproc();
  struct proc *target_p = 0;

  argint(0, &target_pid);
  argint(1, &value);

  // שגיאות קצה
  if(target_pid == p->pid || target_pid <= 0) return -1;

  // חיפוש תהליך המטרה - המנעול שלו (target_p->lock) נשאר נעול!
  for(struct proc *tp = proc; tp < &proc[NPROC]; tp++) {
    acquire(&tp->lock);
    if(tp->pid == target_pid && tp->state != UNUSED && tp->state != ZOMBIE) {
      target_p = tp;
      break; 
    }
    release(&tp->lock);
  }

  if(!target_p || target_p->killed) {
    if(target_p) release(&target_p->lock);
    return -1;
  }

  // העברת הערך
  target_p->trapframe->a0 = value;

  // אם המטרה כבר מחכה לנו ב-co_yield
  if(target_p->state == SLEEPING && target_p->chan == (void*)sys_co_yield) {
    target_p->state = RUNNING; 
    p->state = SLEEPING;
    p->chan = (void*)sys_co_yield;
    mycpu()->proc = target_p;
    
    // --- הקסם הפשוט: קופצים למטרה כש*רק* המנעול שלה מוחזק ---
    swtch(&p->context, &target_p->context);
    
    // כשאנחנו מתעוררים, התהליך השני דאג להחזיק את המנעול שלנו!
    // לכן אנחנו פשוט משחררים אותו בצורה חלקה והפסיקות (printf) חוזרות לעבוד.
    p->chan = 0;
    release(&p->lock);
  } else {
    // המטרה עוד לא מוכנה, הולכים לישון רגיל
    release(&target_p->lock);
    
    acquire(&p->lock);
    p->state = SLEEPING;
    p->chan = (void*)sys_co_yield;
    sched(); 
    
    p->chan = 0;
    release(&p->lock);
  }

  return p->trapframe->a0;
}