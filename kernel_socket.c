
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_pipe.h"
#include "kernel_socket.h"
#include "kernel_proc.h"


static file_ops socket_fops = {
        .Open = NULL,
        .Read =  sys_socket_read,
        .Write = sys_socket_write,
        .Close = sys_socket_close
};

Fid_t sys_Socket(port_t port)
{

    if (port < 0 || port > MAX_PORT){
        return NOFILE;
    }

    Fid_t fid[1];
    FCB* fcb[1];

    if(!(FCB_reserve(1, fid, fcb)))
        return NOFILE;

    SCB* socket = xmalloc(sizeof(SCB));
    socket->refcount = 0; //Prpeei na einai 0 i 1
    socket->type = SOCKET_UNBOUND;
    socket->port = port;
    socket->fcb = fcb[0];
    fcb[0]->streamobj = socket;
    fcb[0]->streamfunc = &socket_fops;

    return fid[0];
}

int sys_Listen(Fid_t sock)
{
    SCB* scb = get_scb(sock);

    if (scb == NULL || scb->port == NOPORT || PORT_MAP[scb->port] != NULL || scb->type != SOCKET_UNBOUND){ /* Maybe scb->port 1 < < MAXPORT WRONG */
        return -1;
    }

    PORT_MAP[scb->port] = scb;
    scb->type = SOCKET_LISTENER;
    scb->listener_s.req_available = COND_INIT;
    rlnode_init(&(scb->listener_s.queue), scb);


    return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
    SCB* scb_server = get_scb(lsock);

    if (scb_server == NULL || scb_server->type != SOCKET_LISTENER || PORT_MAP[scb_server->port] == NULL){
        return NOFILE;
    }

    scb_server->refcount++;

    while(is_rlist_empty(&(scb_server->listener_s.queue))){
        kernel_wait(&(scb_server->listener_s.req_available), SCHED_IO);
    }

    if (PORT_MAP[scb_server->port] == NULL){
        scb_server->refcount--; //MALLON
        return NOFILE;
    }

    rlnode *client_node  = rlist_pop_front(&(scb_server->listener_s.queue));
    // Theoro oti den einai null
    CR* cr = client_node->cr;

    SCB* scb_client_peer = cr->peer;

    Fid_t fid_server_peer = Socket(scb_server->port);

    if (fid_server_peer == NOFILE){
        scb_server->refcount--;
        kernel_signal(&(cr->connected_cv));
        return NOFILE;
    }

    SCB* scb_server_peer = get_scb(fid_server_peer);

    PIPE_CB* pipe_cb1 = construct_Pipe();
    PIPE_CB* pipe_cb2 = construct_Pipe();

    pipe_cb1->writer = scb_server_peer->fcb;
    pipe_cb1->reader = scb_client_peer->fcb;
    pipe_cb2->writer = scb_client_peer->fcb;
    pipe_cb2->reader = scb_server_peer->fcb;


    scb_server_peer->type = SOCKET_PEER;
    scb_server_peer->peer_s.write_pipe = pipe_cb1;
    scb_server_peer->peer_s.read_pipe = pipe_cb2;
    scb_server_peer->peer_s.peer = scb_client_peer;

    scb_client_peer->type = SOCKET_PEER;
    scb_client_peer->peer_s.write_pipe = pipe_cb2;
    scb_client_peer->peer_s.read_pipe = pipe_cb1;
    scb_client_peer->peer_s.peer = scb_server_peer;

    cr->admitted = 1;

    scb_server->refcount--;
    kernel_signal(&(cr->connected_cv));

    return fid_server_peer;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
    SCB* scb_peer = get_scb(sock);
    if (scb_peer == NULL || scb_peer->type != SOCKET_UNBOUND || port < 1 || port > (MAX_PORT+1)){
        return -1;
    }

    SCB* scb_server = PORT_MAP[port];
    if (scb_server == NULL || scb_server->type != SOCKET_LISTENER){
        return -1;
    }

    scb_peer->refcount++;

    CR* cr = xmalloc(sizeof (CR));
    cr->admitted = 0;
    rlnode_init(&(cr->queue_node), cr);
    cr->connected_cv = COND_INIT;
    cr->peer = scb_peer;

    rlist_push_back(&(scb_server->listener_s.queue), &(cr->queue_node));
    kernel_signal(&(scb_server->listener_s.req_available));


    if(timeout > 0) {
        kernel_timedwait(&(cr->connected_cv), SCHED_IO, TIMEOUT);
    }else{
        kernel_wait(&(cr->connected_cv), SCHED_IO);
    }

    int retval = cr->admitted ? 0 : -1;
    rlist_remove(&(cr->queue_node));
    free(cr);

    scb_peer->refcount--;
    return retval;
}

SCB * get_scb(Fid_t sock){
    FCB* fcb = get_fcb(sock);
    if (fcb == NULL || sock < 0 || sock > MAX_FILEID ){
        return NULL;
    }

    return fcb->streamobj;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

int sys_socket_read(SCB* scb, char *buf, unsigned int size){
    PIPE_CB* pipe_cb = scb->peer_s.read_pipe;
    return pipe_read(pipe_cb, buf, size);
    //may need check for mistakes
}

int sys_socket_write(SCB* scb, char *buf, unsigned int size){
    PIPE_CB* pipe_cb = scb->peer_s.read_pipe;
    return pipe_read(pipe_cb, buf, size);
}

int sys_socket_close(Fid_t* sock){
//    if(sock==NULL)
//        return -1;
//
//    SCB* scb = get_scb(sock);
//
//    if(scb == NULL){
//        return -1;
//    }
//
//
//    PIPE_CB * pipe_cb = (PIPE_CB*) _pipecb;
//    pipe_cb->writer=NULL;
//    kernel_broadcast(&pipe_cb->has_data);
//
//    if(pipe_cb->reader==NULL) {
//        free(_pipecb);
//    }
//
//    return 0;
    return 0;
}

