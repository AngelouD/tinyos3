/**
  @brief The process thread control block

  This object gives the os the ability to attach multiple threads to each process
*/
typedef struct process_thread_control_block {
    TCB *tcb;     /**< @brief The connected TCB */

    Task task;    /**< @brief The function the TCB runs*/
    int argl;     /**< @brief Function argument length */
    void * args;  /**< @brief Function arguments */

    int exitval;  /**< @brief The exit value of the thread*/

    int exited;   /**< @brief Whether the thread has exited */
    int detached; /**< @brief Whether the thread is detached */
    CondVar exit_cv;    /**< @brief Condition variable for signaling the threads */

    int refcount; /**< @brief Amount of threads waiting to join */

    rlnode ptcb_list_node;
} PTCB ;

/**
 * @brieaf Initialize a new ptcb.
 *
 * This call creates a new ptcb, initializing and returning its PTCB.
 * The task will be set to the function wanted to be executed.
 *
 * @param task The function that will be run
 * @param argl Function arguement length
 * @param args Fucntion arguements
 * @return A pointer to the new PTCB
 */
PTCB* spawn_ptcb(Task task, int argl, void * args);