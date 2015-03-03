#ifndef __GETFILE_SERVER_H__
#define __GETFILE_SERVER_H__

#include <pthread.h>
#include "steque.h"

#define MAX_REQUEST_LEN 128

typedef int gfstatus_t;

#define  GF_OK 200
#define  GF_FILE_NOT_FOUND 400
#define  GF_ERROR 500

typedef struct _gfserver_t gfserver_t;
typedef struct _gfcontext_t gfcontext_t;

struct _gfserver_t{
	steque_t req_queue;
	unsigned int short port;
	int max_npending;
	int nthreads;
	int socket_fd;

	ssize_t (*worker_func)(gfcontext_t *, char *, void*);

	gfcontext_t *contexts;
	pthread_mutex_t queue_lock;
	pthread_cond_t req_inserted;
};

struct _gfcontext_t{
	pthread_t thread;
	void *arg;
	gfserver_t *gfs;

	int socket;
	size_t file_len;
	size_t bytes_transferred;

	char *protocol;
	char *method;
	char *path;
	char request[MAX_REQUEST_LEN];	
};

typedef enum{
  GFS_PORT,
  GFS_MAXNPENDING,
  GFS_WORKER_FUNC,
  GFS_WORKER_ARG
} gfserver_option_t;

/* Setting up the server.  Call this from the main thread.*/
void gfserver_init(gfserver_t *gfh, int nthreads);

/* Setting options.  Call this from the main thread.*/
void gfserver_setopt(gfserver_t *gfh, gfserver_option_t option, ...);

/* Start serving.  Call this from the main thread.*/
void gfserver_serve(gfserver_t *gfh);

/* Start stop.  Call this from the main thread or a signal handler.*/
void gfserver_stop(gfserver_t *gfh);

/*Call from registered callback */
ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len);

/*Call from registered callback */
ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t);

#endif