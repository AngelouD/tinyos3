
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
    socket->refcount = 0;
    socket->type = SOCKET_UNBOUND;
    socket->port = port;
    socket->fcb = fcb[0];
    fcb[0]->streamobj = socket;
    fcb[0]->streamfunc = &socket_fops;

    return fid[0];
}

int sys_Listen(Fid_t sock)
{
    FCB* fcb = get_fcb(sock);

    if (fcb == NULL || sock < 0 || sock > MAX_FILEID ){
        return -1;
    }
    SCB* scb = (SCB *) fcb->streamobj;

    if (scb == NULL || scb->port == NOPORT || PORT_MAP[scb->port] != NULL || scb->type != SOCKET_UNBOUND){ /* Maybe scb->port NULL WRONG */
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
    if(lsock < 0 || lsock > MAX_FILEID){
        return NOFILE;
    }
    FCB* fcb = get_fcb(lsock);
    if (fcb == NULL || fcb->streamobj == NULL){
        return NOFILE;
    }
    SCB* scb_server = (SCB*) fcb->streamobj;
    if (scb_server->type != SOCKET_LISTENER){
        return NOFILE;
    }

    if (PORT_MAP[scb_server->port] == NULL){
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
    cr->admitted = 1;
    SCB* scb_client = cr->peer;
    scb_client->type = SOCKET_PEER; //Mporei kai na mi xreiazontousan

    Fid_t fid_server_peer = Socket(scb_server->port);
    SCB* scb_server_peer = (((PCB*)CURPROC)->FIDT)[fid_server_peer]->streamobj;

    PIPE_CB* pipe_cb1 = construct_Pipe();
    PIPE_CB* pipe_cb2 = construct_Pipe();

    //TODO: check if pipes are ok

    scb_server_peer->type = SOCKET_PEER;
    scb_server_peer->peer_s.write_pipe = pipe_cb1;
    scb_server_peer->peer_s.read_pipe = pipe_cb2;
    scb_server_peer->peer_s.peer = scb_client;

    scb_client->type = SOCKET_PEER; //TODO: WE DONT KNOW IF IT ALREDY SET BY CONNECT
    scb_client->peer_s.write_pipe = pipe_cb2;
    scb_client->peer_s.read_pipe = pipe_cb1;
    scb_client->peer_s.peer = scb_server_peer;

    kernel_signal(&(cr->connected_cb));

    scb_server->refcount--;
    return fid_server_peer;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
    if(sock < 0 || sock > MAX_FILEID){
        return -1;
    }

    FCB* fcb = get_fcb(sock);
    if (fcb == NULL || fcb->streamobj == NULL){
        return -1;
    }
    SCB* scb_peer = (SCB*) fcb->streamobj;
    if (scb_peer->type != SOCKET_LISTENER){
        return -1;
    }

    if (PORT_MAP[scb_peer->port] == NULL){
        return -1;
    }

    if (port < 1 || port > (MAX_PORT+1)){
        return -1;
    }

    if(PORT_MAP[scb_peer->port]->type != SOCKET_LISTENER){
        return -1;
    }

    SCB* scb_server = PORT_MAP[scb_peer->port];
    scb_server->refcount ++;

    CR* cr = xmalloc(sizeof (CR));
    cr->admitted = 0;
    rlnode_init(&(cr->queue_node), cr);
    cr->connected_cb = COND_INIT;
    cr->peer = scb_peer;

    rlist_push_back(&(scb_server->listener_s.queue), &(cr->queue_node));
    kernel_signal(&(scb_server->listener_s.req_available));


    while(cr->admitted == 0){
        if(timeout > 0) {
            kernel_timedwait(&(cr->connected_cb), SCHED_IO, TIMEOUT);
        }else{
            kernel_wait(&(cr->connected_cb), SCHED_IO);
        }
    }
    scb_server--;

    if (cr->admitted == 0){
        free(cr);
        return -1;
    }

	return 0;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

int sys_socket_read(){
    return 0;
}

int sys_socket_write(){
    return 0;
}

int sys_socket_close(){
    return 0;
}

