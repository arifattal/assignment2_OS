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
    kt->kstate = K_UNUSED;
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
Given a proc, it allocates a unique kernel thread ID using the counter 
and lock inside the proc.
*/
int allocKTpid(struct proc *p){
  int kt_pid;
  acquire(&p->t_pid_lock);
  kt_pid = p->tpidCounter;
  p->tpidCounter++;
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
struct kthread* allocKthread(struct proc *p){
  int found = 0;
  struct kthread* kt;
  struct kthread* zombie_kt = 0;
  int num_unused = 0;
  int num_zombie = 0;
  for (struct kthread *t = p->kthread; t < &p->kthread[NKT]; t++){
    acquire(&t->lock);
    if(t->kstate == K_UNUSED){
      num_unused++;
    }
    if(t->kstate == K_ZOMBIE){
      num_zombie++;
      zombie_kt = t;
    }
    release(&t->lock);
  }

  if(num_unused == 3 && num_zombie == 1){
    printf("problem\n");
    freeKT(zombie_kt);
  }

  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->lock);
    if(kt->kstate == K_UNUSED){
      kt->tid = allocKTpid(p); //allocates a new kernel thread ID
      kt->kstate = K_USED;
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

  if(found == 0){ //an unused kthread hasn't been found
    return 0;
  }
  return kt;
}
/*
Given a kthread, it sets its fields to null / zero, and the state to unused.
//p->lock must be held
//kt->lock must be held <- we're not sure where these instructions came from, maybe from the forum?
*/
void freeKT(struct kthread *kt){
  kt->chan = 0;
  kt->killed = 0;
  kt->xstate = 0;
  kt->tid = 0;
  kt->kstate = K_UNUSED;
}

//an auxiliary function called by exit
void exitThread(struct kthread *kt, int status){ //we added this
  kt->xstate = status; 
  kt->kstate = K_ZOMBIE;
}

//an auxiliary function called by kill
//see kill in proc.c to see why we did this
void killThread(struct kthread *kt){
  //kt->killed = 1; //might not need this
  if(kt->kstate == K_SLEEPING){ //wake up the target process from sleep because the process may be blocked on a system call or waiting for an event to occur
    kt->kstate = K_RUNNABLE;
  }
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

int
kt_killed(struct kthread *kt)
{
  int k;
  
  acquire(&kt->lock);
  k = kt->killed;
  release(&kt->lock);
  return k;
}

struct kthread* get_kt_from_id(int ktid){
    struct proc *p_loop;
    struct kthread *kt_loop;
    for(p_loop = proc; p_loop < &proc[NPROC]; p_loop++){
    for (kt_loop = p_loop->kthread; kt_loop < &p_loop->kthread[NKT]; kt_loop++){
      if(kt_loop->tid == ktid){
        return kt_loop;
      }
    }
  }
  return 0;
}

/*
Calling kthread_create(...) will create a new thread within the context of the calling process. 
The newly created thread state will be runnable. 
The caller of kthread_create(...) must allocate a stack for the new thread to use (using malloc). 
The stack size should be 4000 bytes (as in the ULT). 
It would be best practice to define a new macro for the size. 
Define it in a file that is accessible from the user space.
start_func is a pointer to the entry function, which the thread will start executing. 
Upon success, the identifier of the newly created thread is returned. In case of an error -1 is returned.
This function should set the user-space ‘epc’ register to point to start_func, 
and the user-space ‘sp’ register to the top of the stack.
Note: make sure that the function pointed to by start_func 
calls kthread_exit(...) at the end, similarly to the requirement in ULT.
*/
int kthread_create( void *(*start_func)(), void *stack, uint stack_size ){
  struct proc* p = myproc();
  if(p == 0 || stack == 0 || stack_size == 0){
    return -1;
  }
  struct kthread* kt = allocKthread(p);
  if(kt == 0){
    return -1;
  }
  kt->context.ra = (uint64)start_func; //when the thread will start executing it will start at this function
  kt->context.sp = (uint64)stack + stack_size; //set sp(stack pointer) to the top of the stack
  kt->kstate = K_RUNNABLE;
  release(&kt->lock);
  return kt->tid;
}

int kthread_id(){
  struct kthread *kt = mykthread();
  if(kt == 0)
    return -1;
  return kt->tid;
}
/*
This function sets the killed flag of the kthread with the given ktid in the same process. 
If the kernel thread is sleeping, it also sets its state to runnable. 
Upon success this function returns 0, and -1 if it fails 
(e.g., when the ktid does not match any kthread’s ktid under this process). 
Note: in order for this system call to have any effect, 
the killed flag of the kthread needs to be checked in 
certain places and kthread_exit(...) should be called accordingly.
Hint: look where proc’s killed flag is checked in proc.c, trap.c and sysproc.c.
*/
int kthread_kill(int ktid){
  struct proc *p = myproc();
  struct kthread *kt;
  int return_val = -1;
  if(p == 0){
    return -1;
  }
  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->lock);
    if(kt->tid == ktid){
      return_val = 0;
      kt->killed = 1;
      if(kt->kstate == K_SLEEPING){
        kt->kstate = K_RUNNABLE;
      }
      release(&kt->lock);
      break;
    }
    else{
      release(&kt->lock);
    }
  }
  return return_val;
}

/*
This function terminates the execution of the calling thread. 
If called by a thread (even the main thread) while other threads 
exist within the same process, it shouldn’t terminate the whole process. 
The number given in status should be saved in the thread’s structure as its exit status.
*/
void kthread_exit(int status){
  //struct proc *p = myproc();
  struct kthread *kt = mykthread();
  //int x = 0;
  // for (struct kthread *t = p->kthread; t < &p->kthread[NKT]; t++){
  //   acquire(&t->lock);
  //   if(kt->kstate == K_UNUSED){
  //     x++;
  //   }
  //   release(&t->lock);
  // }
  wakeup(kt);
  acquire(&kt->lock);
  kt->kstate = K_ZOMBIE;
  kt->xstate = status;
  // if(x == NKT - 1){
  //   release(&kt->lock);
  //   exit(status);
  // }
  sched();
  panic("zombie exit");

}

/*
This function suspends the execution of the calling thread 
until the target thread (indicated by the argument ktid) terminates. 
When successful, the pointer given in status is filled with 
the exit status (if it’s not null), and the function returns zero. 
Otherwise, -1 should be returned to indicate an error.
Note: calling this function with a ktid of an already 
terminated (but not yet joined) kthread should succeed 
and allow fetching the exit status of the kthread.
*/
int kthread_join(int ktid, int *status){
  struct proc *p = myproc();
  struct kthread *kt = mykthread();
  struct kthread *target_kt = get_kt_from_id(ktid);
  //status = 0;
  if(p == 0 || kt == 0 || target_kt == 0){
    return -1;
  }
  //possibly the target kt is already in zombie state, check this
  //acquire(&target_kt->lock); //this is needed if we send target_kt->lock as a parameter to sleep, since the lock is released there
  if(target_kt->kstate != K_ZOMBIE){//if target_kt is already in zombie state sending kt to sleep isn't needed
    acquire(&p->lock);
    sleep(target_kt, &p->lock); //send kt to sleep on target_kt, 
  }
  //we must release target_kt's resources
  if(target_kt->kstate == K_ZOMBIE){
      //target_kt's lock was reacquired through sleep
      acquire(&p->lock);

      if(status != 0 && copyout(p->pagetable, (uint64)status, (char *)&target_kt->xstate,
                            sizeof(target_kt->xstate)) < 0) {
        release(&target_kt->lock);
        return -1;
    }
    freeKT(target_kt);
    release(&target_kt->lock);
    return 0;
  }
  return -1;
}