
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

    /* Initializing a new tcb on the current pcb */
    TCB* tcb = spawn_thread(CURPROC, start_sub_thread);

    /* Initializing a new ptcb for the new thread */
    PTCB* ptcb = spawn_ptcb(task, argl, args);

    /* Linking the ptcb with the tcb */
    ptcb->tcb = tcb;
    tcb->ptcb = ptcb;

    /* Adding the ptcb node into the pcb node list */
    rlist_push_back(& CURPROC->ptcb_list, & ptcb->ptcb_list_node);

    /* Increasing the ammount of threads running in the current pcb */
    CURPROC->thread_count++;

    wakeup(tcb);
	return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current PTCB.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->ptcb;
}

/**
  @brief Join the given thread.

  An attempt will be made to join a thread. It will not be successful if we attempt
  to join a nonexistent thread, a detached thread or the current thread.
  While waiting the refcount of the thread we are waiting for will increase.
  The current thread will stop waiting when the other thread has exited or became detached.

  @param tid the thread to be joined
  @param exitval where it's exit value is stored

  @returns 0 on success
  @returns 1 on failure
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
    /* Obtaining the list with the available ptcbs */
    rlnode* node_list = &CURPROC->ptcb_list;
    /* Searching for the ptcb with the tid we want */
    rlnode* ptcb_node = rlist_find(node_list, (PTCB *) tid, NULL);

    /* If the node is not found, we return error */
    if (ptcb_node == NULL){
        return -1;
    }

    PTCB* ptcb = ptcb_node->ptcb;
    /* It can fail on these terms:
     * 1. We are searching for a nonexistent ptcb
     * 2. We are searching for a detached ptcb that we can't join
     * 3. We are searching for ourselves which we can't join. */
    if(ptcb == NULL || ptcb->detached || tid == sys_ThreadSelf()){
        return -1;
    }else{
        /* When the ptcb is found we increase it's refcount */
        ptcb->refcount ++;

        /* And wait while it's not exited, or detached */
        while(!ptcb->exited && !ptcb->detached){
            kernel_wait(& ptcb->exit_cv, SCHED_USER);
        }

        /* We are not waiting now, so the refcount is decreased */
        ptcb->refcount --;

        /* If the reason we stopped waiting was because the thread became
         * detached, we must return an error as it's illegal
         * to be attached to it */
        if (ptcb->detached){
            return -1;
        }

        /* If an exitval is provided, we set it on the current exitval */
        if (exitval){
            *exitval = ptcb->exitval;
        }

        /* If there is noone waiting for the ptcb, it's cleared */
        if(ptcb->refcount == 0){
            rlist_remove(& ptcb->ptcb_list_node);
            free(ptcb);
        }
        return 0;
    }
}

/**
  @brief Detach the given thread.

  Detaching a thread means no one can join it.
  In case the thread is not found an error is returned.
  If the thread has not exited, it becomes detached and broadcasts the
  change of state to all threads so the waiting ones will stop.

  @param tid the thread to be detached

  @returns 0 on success
  @returns 1 on failure
  */
int sys_ThreadDetach(Tid_t tid)
{
    /* Obtaining the list with the available ptcbs */
    rlnode* node_list = &CURPROC->ptcb_list;
    /* Searching for the ptcb with the tid we want */
    rlnode* ptcb_node = rlist_find(node_list, (PTCB *) tid, NULL);

    /* Detach fails when:
     * 1. The node is not found
     * 2. It's already exited */
    if(ptcb_node==NULL || ptcb_node->ptcb->exited == 1){
        return -1;
    }
    else{
        /* Setting the ptcb we found as detached */
        ptcb_node->ptcb->detached = 1;
        /* Waking up waiting threads */
        kernel_broadcast(& ptcb_node->ptcb->exit_cv);
        return 0;
    }
}

/**
  @brief Terminate the current thread.

  When a thread exits:
  1. The exitval is set
  2. The ptcb is marked as exited
  3. The amount of current threads running is decremented
  4. All threads are informed of the exit


  @param the exitval of the exiting process
  */
void sys_ThreadExit(int exitval)
{
    PTCB* ptcb = (PTCB*) sys_ThreadSelf();
    ptcb->exitval = exitval;
    ptcb->exited = 1;
    CURPROC->thread_count--;
    kernel_broadcast(&ptcb->exit_cv);

    if(CURPROC->thread_count == 0) {
        if (get_pid(CURPROC) != 1) {

            /* Reparent any children of the exiting process to the
               initial task */
            PCB *initpcb = get_pcb(1);
            while (!is_rlist_empty(&CURPROC->children_list)) {
                rlnode *child = rlist_pop_front(&CURPROC->children_list);
                child->pcb->parent = initpcb;
                rlist_push_front(&initpcb->children_list, child);
            }

            /* Add exited children to the initial task's exited list
               and signal the initial task */
            if (!is_rlist_empty(&CURPROC->exited_list)) {
                rlist_append(&initpcb->exited_list, &CURPROC->exited_list);
                kernel_broadcast(&initpcb->child_exit);
            }

            /* Put me into my parent's exited list */
            rlist_push_front(&CURPROC->parent->exited_list, &CURPROC->exited_node);
            kernel_broadcast(&CURPROC->parent->child_exit);

        }

        assert(is_rlist_empty(&CURPROC->children_list));
        assert(is_rlist_empty(&CURPROC->exited_list));


        /*
          Do all the other cleanup we want here, close files etc.
         */

        /* Release the args data */
        if (CURPROC->args) {
            free(CURPROC->args);
            CURPROC->args = NULL;
        }

        /* Clean up FIDT */
        for (int i = 0; i < MAX_FILEID; i++) {
            if (CURPROC->FIDT[i] != NULL) {
                FCB_decref(CURPROC->FIDT[i]);
                CURPROC->FIDT[i] = NULL;
            }
        }

        /* Attempt to clean the ptcb from memory if it's exited and no one is waiting for it */
        while(rlist_find(&CURPROC->ptcb_list, ptcb, FREE) != NULL) {
            if (ptcb->refcount < 1) {
                rlist_remove(&ptcb->ptcb_list_node);
                free(ptcb);
            }
        }
    }

    /* Disconnect my main_thread */
    CURPROC->main_thread = NULL;

    /* Now, mark the process as exited. */
    CURPROC->pstate = ZOMBIE;

    /* Bye-bye cruel world */
    kernel_sleep(EXITED, SCHED_USER);
}

/*
  Initialize and return a new PTCB
*/
PTCB* spawn_ptcb(Task task, int argl, void * args){

    /* Allocating space in memory for the ptcb */
    PTCB *ptcb = (PTCB *) xmalloc(sizeof(PTCB));

    /* Setting the ptcb task */
    ptcb->task = task;

    ptcb->exitval = -1;
    ptcb->exited = 0;
    ptcb->detached = 0;

    /* Setting the initial references to the ptcb to 0 */
    ptcb->refcount = 0;

    /* Copying the arguements */
    ptcb->argl = argl;
    ptcb->args = args;

    /* Initializing the ptcb node */
    rlnode_init(&ptcb->ptcb_list_node, ptcb);
    ptcb->exit_cv = COND_INIT;

    return ptcb;
};

