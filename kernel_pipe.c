
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

	PIPE_CB* pipe_cb = (PIPE_CB*)xmalloc(sizeof(PIPE_CB));
	
	if(pipe_cb==NULL)
		return -1;
	
	pipe_cb->has_space=COND_INIT;
	pipe_cb->has_data=COND_INIT;

	pipe_cb->read_pos=0;
	pipe_cb->write_pos=0;

	pipe_cb->read=fcb[0];
	pipe_cb->write=fcb[1];

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



int pipe_write(void* pipecb_t, const char *buf, unsigned int n){
	return -1;

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


int pipe_read(void* pipecb_t, char *buf, unsigned int n){
	return -1;
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

_pipecb->write=NULL;


if(_pipecb->read==NULL){
	free(_pipecb);
}
else
	kernel_broadcast(&_pipecb->has_data);

return 0;
}





int pipe_reader_close(void* _pipecb){


if(_pipecb==NULL)
	return -1;

_pipecb->read=NULL;


if(_pipecb->write==NULL){
	free(_pipecb);
}
else
	kernel_broadcast(&_pipecb->has_space);

return 0;


}




PIPE_CB* construct_Pipe(){

    Fid_t fid[2];  
    FCB* fcb[2];  


    if(!(FCB_reserve(2, fid,fcb)))	
		return NULL;  

	PIPE_CB* pipe_cb = (PIPE_CB*)xmalloc(sizeof(PIPE_CB));
	
	if(pipe_cb==NULL)
		return NULL;
	
	pipe_cb->has_space=COND_INIT;
	pipe_cb->has_data=COND_INIT;

	pipe_cb->read_pos=0;
	pipe_cb->write_pos=0;

	pipe_cb->read=fcb[0];
	pipe_cb->write=fcb[1];

	fcb[0]->streamobj=pipe_cb;
	fcb[1]->streamobj=pipe_cb;

	fcb[0]->streamfunc=&pipe_reader_fops;
    fcb[1]->streamfunc=&pipe_writer_fops;

   return pipe_cb;
 }

