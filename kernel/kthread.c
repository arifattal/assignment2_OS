#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];


/*
Given a proc, it initializes the lock in charge of thread ID allocation, then, 
it initializes for every thread in the process table its lock, state (to unused), 
process (to be the given proc it belongs to) and sets the kthread stack pointer in the same way as in procinit(...) (the last part has already been done for you). 
This function will be called once for each process at the initialization time of xv6. 
Add your code to the already existing kthreadinit(...) function in kthread.c.
*/
void kthreadinit(struct proc *p)
{
  initlock(&p->t_pid_lock, "pid_lock"); //initializes the lock in charge of thread ID allocation
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    initlock(&kt->lock, "t_lock");
    kt->kstate = UNUSED;
    kt->parnetProc = p;
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  }
}

/*
Fetches and returns the current running thread from the current cpu’s cpu structs. 
*/
struct kthread *mykthread()
{
  //our implementation is similar to myproc()
  push_off();
  struct cpu *c = mycpu();
  struct kthread *kt = c->kthread;
  pop_off();
  return kt;
  //return &myproc()->kthread[0]; //initial implementation given to us which should be changed
}

/*
Given a proc, it allocates a unique kernel thread ID using the counter and lock inside the proc.
*/
int allocKTpid(struct proc *p){
  int kt_pid;
  acquire(&p->t_pid_lock);
  kt_pid = p->tpidCounter + 1;
  release(&p->t_pid_lock);
  return kt_pid;
}

/*
Given a proc, it finds an unused entry in its kernel thread table. 
It then allocates a new kernel thread ID for it, sets its state to used, 
assigns it its trapframe, initializes the context to zeroes, 
changes the ‘ra’ register in context to forkret address, 
and the ‘sp’ register in context to the top of the stack. 
Finally, it returns a pointer to the newly allocated kthread with its lock acquired.
Note: the pointer to the trapframe should be 
fetched by using the get_kthread_trapframe(...) function from kthread.c.
*/
int allocKthread(struct proc *p){
  acquire(&p->lock);
  int found = 0;
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->lock);
    if(kt->kstate == UNUSED){
      kt->tid = allocKTpid(p); //allocates a new kernel thread ID
      kt->kstate = USED;
      kt->trapframe = get_kthread_trapframe(p, kt); //assigns it its trapframe
      memset(&kt->context, 0, sizeof(kt->context)); //initializes the context to zeroes
      kt->context.ra = (uint64)forkret; //changes the ‘ra’ register in context to forkret address
      kt->context.sp = kt->kstack + PGSIZE; //set ‘sp’ register in context to the top of the stack
      found = 1;
      break;
    }
    else{
      release(&kt->lock);
    }
  }
  release(&p->lock);
  if(found == 0){ //an unused kthread hasn't been found
    return -1;
  }
  return 0;
}
/*
Given a kthread, it sets its fields to null / zero, and the state to unused.
//p->lock must be held
//kt->lock must be held
*/
int freeKT(struct kthread *kt){
  kt->kstate = UNUSED;
  kt->chan = 0;
  kt->killed = 0;
  kt->xstate = 0;
  kt->tid = 0;
  kt->parnetProc = 0;
  //these lines were copied from freeproc, we aren't sure if this is correct
  //also we didn't free the kstack
  if(p->base_trapframes)
    kfree((void*)kt->trapframe);
  kt->trapframe = 0;
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

// TODO: delte this after you are done with task 2.2
void allocproc_help_function(struct proc *p) {
  p->kthread->trapframe = get_kthread_trapframe(p, p->kthread);

  p->context.sp = p->kthread->kstack + PGSIZE;
}