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

    if(target_pid == p->pid || target_pid <= 0) return -1;

    // מחפשים ונועלים את תהליך המטרה
    for(struct proc *tmp = proc; tmp < &proc[NPROC]; tmp++) {
        acquire(&tmp->lock);
        if(tmp->pid == target_pid) {
            if(tmp->state == UNUSED || tmp->state == ZOMBIE) {
                release(&tmp->lock);
                return -1;
            }
            target_p = tmp;
            break; 
        }
        release(&tmp->lock);
    }

    if(!target_p) return -1;

    target_p->trapframe->a0 = value;

    if(target_p->state == SLEEPING) {
        // --- מעבר ישיר (Direct Switch) ---
        target_p->state = RUNNING;
        
        acquire(&p->lock);
        p->state = SLEEPING;
        
        // מעדכנים את המעבד לעקוף את המתזמן
        mycpu()->proc = target_p;
        
        // אנחנו משחררים את שלנו, ושומרים את המנעול של המטרה!
        release(&p->lock);
        
        swtch(&p->context, &target_p->context);
        
        // המטרה התעוררה (מישהו העביר לה את המנעול שלה), אז נשחרר
        release(&p->lock);
    } 
    else {
        // --- המתנה (המטרה עוד לא מוכנה) ---
        release(&target_p->lock);
        
        acquire(&p->lock);
        p->state = SLEEPING;
        sched();
        release(&p->lock);
    }

    return p->trapframe->a0;
}