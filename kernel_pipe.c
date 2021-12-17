
#include "kernel_pipe.h"
#include "tinyos.h"
#include "kernel_proc.h"

static file_ops pipe_reader_fops = {
        .Open = NULL,
        .Read = pipe_read,
        .Write = NULL,
        .Close = pipe_reader_close
};


static file_ops pipe_writer_fops = {
        .Open = NULL,
        .Read = NULL,
        .Write = pipe_write,
        .Close =  pipe_writer_close
};





int sys_Pipe(pipe_t* pipe)
{
    Fid_t fid[2];
    FCB* fcb[2];

    if(!(FCB_reserve(2, fid,fcb)))
        return -1;

    pipe->read=fid[0];
    pipe->write=fid[1];

    PIPE_CB* pipe_cb = (PIPE_CB *)xmalloc(sizeof(PIPE_CB));

    if(pipe_cb==NULL)
        return -1;
    //TODO: CONSTRUCT PIPE
    pipe_cb->has_space=COND_INIT;
    pipe_cb->has_data=COND_INIT;

    pipe_cb->r_position=0;
    pipe_cb->w_position=0;

    pipe_cb->reader=fcb[0];
    pipe_cb->writer=fcb[1];

    pipe_cb->bytes_filled = 0;

    fcb[0]->streamobj=pipe_cb;
    fcb[1]->streamobj=pipe_cb;

    fcb[0]->streamfunc=&pipe_reader_fops;
    fcb[1]->streamfunc=&pipe_writer_fops;
    return 0;
}



/** @brief Write operation.

    Write up to 'size' bytes from 'buf' to the stream 'this'.
    If it is not possible to write any data (e.g., a buffer is full),
    the thread will block. 
    The write function should return the number of bytes copied from buf, 
    or -1 on error. 

    Possible errors are:
    - There was a I/O runtime problem.
  */



int pipe_write(void* pipecb_t, const char *buf, unsigned int size){
    PIPE_CB * pipe_cb = (PIPE_CB*) pipecb_t;

    int * w_position = &pipe_cb->w_position;

    unsigned int bytes_writen = 0;

    if (!pipe_cb->reader || !pipe_cb->writer){
        return -1;
    }

    while(bytes_writen < size) {
        while (pipe_cb->bytes_filled == PIPE_BUFFER_SIZE && pipe_cb->reader) {
            kernel_broadcast(&pipe_cb->has_data);
            kernel_wait(&pipe_cb->has_space, SCHED_PIPE);
        }

        if (!pipe_cb->reader || !pipe_cb->writer){
            return (int) bytes_writen;
        }

        int space_left = PIPE_BUFFER_SIZE - pipe_cb->bytes_filled;
        int copy_size = (size - bytes_writen) < space_left ? (int) (size - bytes_writen) : space_left;
        int final_copy_size = copy_size < PIPE_BUFFER_SIZE - *w_position ? copy_size: PIPE_BUFFER_SIZE - *w_position;

        memcpy(&(pipe_cb->buffer[*w_position]), &(buf[bytes_writen]), final_copy_size);

        bytes_writen += final_copy_size;
        pipe_cb->bytes_filled += final_copy_size;
        assert(*w_position < PIPE_BUFFER_SIZE);
        *w_position = (*w_position + final_copy_size) % PIPE_BUFFER_SIZE;
    }
    kernel_broadcast(&pipe_cb->has_data);
    return (int) bytes_writen;
}


/** @brief Read operation.

    Read up to 'size' bytes from stream 'this' into buffer 'buf'. 
    If no data is available, the thread will block, to wait for data.
    The Read function should return the number of bytes copied into buf, 
    or -1 on error. The call may return fewer bytes than 'size', 
    but at least 1. A value of 0 indicates "end of data".

    Possible errors are:
    - There was a I/O runtime problem.
  */


int pipe_read(void* pipecb_t, char *buf, unsigned int size){
    PIPE_CB * pipe_cb = (PIPE_CB*) pipecb_t;

    int * r_position = &pipe_cb->r_position;

    unsigned int bytes_read = 0;

    if (!pipe_cb->reader){
        return -1;
    }else if(pipe_cb->bytes_filled==0 && pipe_cb->writer == NULL){ //Isos na vgei i malakia?
        return 0;
    }

    while(bytes_read < size) {

        while (pipe_cb->bytes_filled==0 && pipe_cb->writer != NULL) {
            kernel_broadcast(&pipe_cb->has_space);
            kernel_wait(&pipe_cb->has_data, SCHED_PIPE);
        }

        if (pipe_cb->bytes_filled==0 && !pipe_cb->writer){
            return bytes_read;
        }

        int space_left = pipe_cb->bytes_filled;
        int copy_size = (size - bytes_read) < space_left ? (int) (size - bytes_read) : space_left;
        int final_copy_size = copy_size < PIPE_BUFFER_SIZE - *r_position ? copy_size : PIPE_BUFFER_SIZE - *r_position;

        memcpy(&(buf[bytes_read]), &(pipe_cb->buffer[*r_position]), final_copy_size);

        bytes_read += final_copy_size;
        pipe_cb->bytes_filled -= final_copy_size;
        assert(PIPE_BUFFER_SIZE > *r_position );
        *r_position = (*r_position + final_copy_size) % PIPE_BUFFER_SIZE;
        kernel_broadcast(&pipe_cb->has_space);
    }
    kernel_broadcast(&pipe_cb->has_space);
    return bytes_read;
}




/** @brief Close operation.

      Close the stream object, deallocating any resources held by it.
      This function returns 0 is it was successful and -1 if not.
      Although the value in case of failure is passed to the calling process,
      the stream should still be destroyed.

    Possible errors are:
    - There was a I/O runtime problem.
     */



int pipe_writer_close(void* _pipecb){
    if(_pipecb==NULL)
        return -1;

    PIPE_CB * pipe_cb = (PIPE_CB*) _pipecb;
    pipe_cb->writer=NULL;
    kernel_broadcast(&pipe_cb->has_data);

    if(pipe_cb->reader==NULL) {
        free(_pipecb);
    }

    return 0;
}





int pipe_reader_close(void* _pipecb){

    if(_pipecb==NULL)
        return -1;
    PIPE_CB * pipe_cb = (PIPE_CB*) _pipecb;
    pipe_cb->reader=NULL;

    kernel_broadcast(&pipe_cb->has_space);

    if(pipe_cb->writer==NULL){
        free(pipe_cb);
    }
    return 0;


}




PIPE_CB* construct_Pipe(FCB ** fcb){


    PIPE_CB* pipe_cb = (PIPE_CB*) xmalloc(sizeof(PIPE_CB));

    if(pipe_cb==NULL)
        return NULL;

    pipe_cb->has_space=COND_INIT;
    pipe_cb->has_data=COND_INIT;

    pipe_cb->r_position=0;
    pipe_cb->w_position=0;

    pipe_cb->reader=fcb[0];
    pipe_cb->writer=fcb[1];

    pipe_cb->bytes_filled = 0;

    return pipe_cb;
}


