#ifndef TINYOS3_KERNEL_SOCKET_H
#define TINYOS3_KERNEL_SOCKET_H
#define TIMEOUT 1000

#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_pipe.h"

typedef struct socket_control_block SCB;

typedef enum socket_type_e {
    SOCKET_LISTENER,   /**< @brief The PID is free and available */
    SOCKET_UNBOUND,  /**< @brief The PID is given to a process */
    SOCKET_PEER  /**< @brief The PID is held by a zombie */
} socket_type;

typedef struct unbound_socket_s {
    rlnode unbound_s;
}unbound_socket;

typedef struct listener_socket_s{
    rlnode queue;
    CondVar req_available;
}listener_socket;

typedef struct peer_socket_s {
    SCB* peer;
    PIPE_CB* write_pipe;
    PIPE_CB* read_pipe;
}peer_socket;

typedef struct socket_control_block{
    uint refcount;
    FCB* fcb;
    socket_type type;
    port_t port;

    union {
        unbound_socket unbound_s;
        listener_socket listener_s;
        peer_socket peer_s;
    };
} SCB;

typedef struct connection_request{
    int admitted;
    SCB* peer;

    CondVar connected_cb;
    rlnode queue_node;
} CR;

SCB * PORT_MAP[MAX_PORT + 1];

Fid_t sys_Socket(port_t port);

int sys_Listen(Fid_t sock);

Fid_t sys_Accept(Fid_t lsock);

int sys_Connect(Fid_t sock, port_t port, timeout_t timeout);

int sys_ShutDown(Fid_t sock, shutdown_mode how);

int sys_socket_read();

int sys_socket_write();

int sys_socket_close();


#endif //TINYOS3_KERNEL_SOCKET_H
