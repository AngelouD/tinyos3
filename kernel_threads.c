
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
    if (task == NULL){
        return NOTHREAD;
    }

    TCB* tcb = spawn_thread(CURPROC, start_sub_thread);
    PTCB* ptcb = spawn_ptcb(task, argl, args);

    ptcb->tcb = tcb;
    tcb->ptcb = ptcb;
    rlist_push_back(& CURPROC->ptcb_list, & ptcb->ptcb_list_node);
    CURPROC->thread_count++;
    wakeup(tcb);
	return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
    rlnode* node_list = &CURPROC->ptcb_list;
    rlnode* ptcb_node = rlist_find(node_list, (PTCB *) tid, NULL);

    if (ptcb_node == NULL){
        return -1;
    }
    PTCB* ptcb = ptcb_node->ptcb;

    if(ptcb == NULL || ptcb->detached || tid == sys_ThreadSelf()){
        return -1;
    }else{
        ptcb->refcount ++;

        while(!ptcb->exited && !ptcb->detached){
            kernel_wait(& ptcb->exit_cv, SCHED_USER);
        }

        ptcb->refcount --;

        if (ptcb->detached){
            return -1;
        }

        if (exitval){
            *exitval = ptcb->exitval;
        }

        if(ptcb->refcount == 0){
            rlist_remove(& ptcb->ptcb_list_node);
            free(ptcb);
        }
        return 0;
    }
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
    rlnode* node_list = &CURPROC->ptcb_list;
    rlnode* ptcb_node = rlist_find(node_list, (PTCB *) tid, NULL);

    if(ptcb_node==NULL || ptcb_node->ptcb->exited == 1){
        return -1;
    }
    else{
        ptcb_node->ptcb->detached = 1;
        kernel_broadcast(& ptcb_node->ptcb->exit_cv);
        return 0;
    }
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

    if(get_pid(CURPROC)!=1) {

        /* Reparent any children of the exiting process to the
           initial task */
        PCB* initpcb = get_pcb(1);
        while(!is_rlist_empty(& CURPROC->children_list)) {
            rlnode* child = rlist_pop_front(& CURPROC->children_list);
            child->pcb->parent = initpcb;
            rlist_push_front(& initpcb->children_list, child);
        }

        /* Add exited children to the initial task's exited list
           and signal the initial task */
        if(!is_rlist_empty(& CURPROC->exited_list)) {
            rlist_append(& initpcb->exited_list, &CURPROC->exited_list);
            kernel_broadcast(& initpcb->child_exit);
        }

        /* Put me into my parent's exited list */
        rlist_push_front(& CURPROC->parent->exited_list, &CURPROC->exited_node);
        kernel_broadcast(& CURPROC->parent->child_exit);

    }

    assert(is_rlist_empty(& CURPROC->children_list));
    assert(is_rlist_empty(& CURPROC->exited_list));


    /*
      Do all the other cleanup we want here, close files etc.
     */

    /* Release the args data */
    if(CURPROC->args) {
        free(CURPROC->args);
        CURPROC->args = NULL;
    }

    /* Clean up FIDT */
    for(int i=0;i<MAX_FILEID;i++) {
        if(CURPROC->FIDT[i] != NULL) {
            FCB_decref(CURPROC->FIDT[i]);
            CURPROC->FIDT[i] = NULL;
        }
    }

    /* Disconnect my main_thread */
    CURPROC->main_thread = NULL;

    /* Now, mark the process as exited. */
    CURPROC->pstate = ZOMBIE;

    /* Bye-bye cruel world */
    kernel_sleep(EXITED, SCHED_USER);
}

PTCB* spawn_ptcb(Task task, int argl, void * args){
    PTCB *ptcb = (PTCB *) xmalloc(sizeof(PTCB));

    ptcb->task = task;

    ptcb->exitval = -1;
    ptcb->exited = 0;
    ptcb->detached = 0;

    ptcb->refcount = 0;

    ptcb->argl = argl;

    if(args!=NULL){
        ptcb->args = xmalloc(argl);
        memcpy(ptcb->args, args, argl);
    }else
        ptcb->args=NULL;

    rlnode_init(&ptcb->ptcb_list_node, ptcb);
    return ptcb;
};

