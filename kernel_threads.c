
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
    //return NOTHREAD;  not sure on how to use it
    TCB* tcb = spawn_thread(CURPROC, start_sub_thread);
    PTCB* ptcb = spawn_ptcb(task, argl, args);

    ptcb->tcb = tcb;
    tcb->ptcb = ptcb;
    rlist_push_back(& CURPROC->ptcb_list_node, & ptcb->ptcb_list_node);
    CURPROC->thread_count++;
    wakeup(tcb);
	return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread();
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	return -1;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

}

PTCB* spawn_ptcb(Task task, int argl, void * args){
    PTCB *ptcb = (PTCB *) malloc(sizeof(PTCB));

    ptcb->task = task;

    ptcb->exitval = -1;
    ptcb->exited = 0;
    ptcb->detached = 0;

    ptcb->refcount = 0;

    ptcb->argl = argl;

    if(args!=NULL){
        ptcb->args = malloc(argl);
        memcpy(ptcb->args, args, argl);
    }else
        ptcb->args=NULL;

    return ptcb;
};

