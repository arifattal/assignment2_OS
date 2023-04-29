#include "uthread.h"
#include "kernel/proc.c"
#include "umalloc.c"

struct uthread proc_uthreads[MAX_UTHREADS]; 
struct uthread* curr_uthread;
struct context context;


/*
This function receives as arguments a pointer to the user thread’s start function and a priority. 
The function should initialize the user thread in a free entry in the table, but not run it just yet. 
Once the thread’s fields are all initialized, the user thread’s state is set to runnable. 
The function returns 0 on success or -1 in case of a failure.
Note: do not forget to set the ‘ra’ register to the start_func and ‘sp’ to the top of the relevant ustack.
Consider what happens when the user thread’s start function finishes: does it have an address to return to?
Hint: A uthread_exit(...) call should be carried out explicitly at the end of each given start_func(...).
We can assume that every start_func given will call uthread_exit()
*/
int uthread_create(void (*start_func)(), enum sched_priority priority){
    struct uthread *t;
    int found = 0;
    for(t = proc_uthreads; t< &proc_uthreads[MAX_UTHREADS]; t++){
        if(t->state == FREE){
            found = 1;
            break;
        }
    }
    if (found == 0){return -1;}//a thread with free state wasn't found
    memset(t, 0, sizeof(struct uthread));//set uthread fields to default values. this might not be needed
    t->state = RUNNABLE;
    t->priority = priority;

    context.ra = (uint64)start_func; //when the thread will start executing it will start at this function
    context.sp = (uint64)t->ustack + STACK_SIZE; //set sp(stack pointer) to the top of the uthread stack
    return 0;
}

/*
an auxiliary function used by uthread_yield and uthread_exit to find the thread with the highest priority.
*/
struct uthread* get_hp_thread(){
    int visited = 0;
    int start_index = -1;
    struct uthread *t;
    struct uthread *hp_t = 0; //stores the highest priority(hp) uthread found.

    for(int i = 0; i<MAX_UTHREADS; i++){ //this loop is used to find the index of the calling uthread in the thread array
        if(&proc_uthreads[i] == curr_uthread){
            start_index = (i+1)%MAX_UTHREADS; //in order for a round-robin pattern to be implemented we set the start_index to the following thread. modular arithmatic is needed in case i == 3
            break;
        }
    }
    t = &proc_uthreads[start_index]; //t starts at the next uthread in line

    while(visited < MAX_UTHREADS){ //this will run over 4 uthreads starting from "proc_uthreads[start_index]"
        if(t >= &proc_uthreads[MAX_UTHREADS]){ //end of array
            t = proc_uthreads; //reset t to the beginning of the array
        }
        if(t->state == RUNNABLE){
            if(hp_t == 0 || t->priority > hp_t->priority){
                hp_t = t;
            }
        }
        t++;
        visited++;
    }
    return hp_t;
}

/*
This function picks up the next user thread from the user threads table, 
according to the scheduling policy (explained later) and restores its context. 
The context of a cooperative ULT is identical to the context field in the xv6 PCB, 
which we’ve seen in the practical sessions.
Therefore, the context switch between the user threads can occur 
in the same way as the regular context switch (look at swtch.S and uswtch.S).
After the context switch occurs, the newly selected thread will run the code starting from the 
address stored by the ‘ra’ register in the same way as in switching between two processes in the kernel.
*/
void uthread_yield(){
    curr_uthread->state = RUNNABLE; //the current thread isn't running anymore. In the instructions given(in the forum) the curr_uthread may run again if it has the highest priority
    struct uthread *hp_t = get_hp_thread(); //stores the highest priority(hp) uthread found.
    if (hp_t != 0){
        hp_t->state == RUNNING;
        uswtch(&curr_uthread->context, &hp_t->context);
    }
}

/*
Terminates the calling user thread and transfers control to some other user thread (similarly to yield). 
Don’t forget to change the terminated user thread’s state to free.
When the last user thread in the process calls uthread_exit the process 
should terminate (i.e., exit(...) should be called).
*/
void uthread_exit(){
    struct uthread *hp_t = get_hp_thread(); //stores the highest priority(hp) uthread found.
    if(hp_t == 0){
        exit(0); //if the proc has no runnable threads exit the running proc
    }
    else{
        curr_uthread->state = FREE;
        hp_t->state = RUNNING;
        uswtch(&curr_uthread->context, &hp_t->context);
    }
}

/*
sets the priority of the calling user thread to the specified argument and returns the previous priority.
*/
enum sched_priority uthread_set_priority(enum sched_priority priority){
    enum sched_priority prev_priority = curr_uthread->priority;
    curr_uthread->priority = priority;
    return prev_priority;
}

/*
returns the current priority of the calling user thread. 
*/
enum sched_priority uthread_get_priority(){
    return curr_uthread->priority;
}

/*
This function is called by the main user thread after it
has created one or more user threads. It is similar to yield; 
it picks the first user thread to run according to the scheduling policy and starts it. 
If successful, this function never returns, and any code beyond it will never be executed (much like execvp).
Since the main user thread was not created using uthread_create, 
and hence has no entry in the user thread table, its context will never be restored. 
Any subsequent calls (after the user threads were already started) to uthread_start_all should not succeed. 
In such a case, this function should return -1 to indicate an error. 
Hint: this functionality can be implemented using a global variable or static variable similarly to forkret in proc.c.
*/
int uthread_start_all(){
    static int first = 1;
    struct uthread *t;
    struct context empty_context;
    if(first){
        for(t = proc_uthreads; t< &proc_uthreads[MAX_UTHREADS]; t++){
            if(t->state == RUNNABLE){
                first = 0;
                uswtch(&empty_context, &t->context);
                return 0;
            }
            }
    }
        return -1; 
    }

/*
Returns a pointer to the UTCB associated with the calling thread.
*/
struct uthread* uthread_self(){
    return curr_uthread;
}