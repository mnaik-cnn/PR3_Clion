#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

#include "gfserver.h"
#include "shm_channel.h"
                                                                \
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                     \
"options:\n"                                                                  \
"  -p [listen_port]    Listen port (Default: 8888)\n"                         \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)"\
"  -h                  Show this help message\n"                              \
"  -z the size (in bytes) of the segments.\n"                                \
"  -n number of segments to use in communication with cache. \n" \
"special options:\n"                                                          \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"




/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},
  {"num-segments",  required_argument,            NULL,     'n'},
  {"segment size",  required_argument,            NULL,     'z'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

//extern ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg);
//extern ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

unsigned short num_segments;
void CleanUpShm();
void CleanUpShm(){
    for(int i = 0; i<= num_segments; i++) {
    char shm_name[256];
      sprintf(shm_name, "/segment_%d", i);
      unlink(shm_name);
    }
}

static gfserver_t gfs;

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){

      CleanUpShm();
      fprintf(stderr,"\n\nMemory Segments cleaned up...stopping server...\n\n");
      gfserver_stop(&gfs);
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int i, option_char = 0;
  unsigned short port = 8888;
  unsigned short nworkerthreads = 1;
  num_segments = 1;
  long segment_size = 512;
  char* cache_server_addr = "0.0.0.0";


  fprintf(stderr,"***STARTING PROXY***\n\n");
  //char *server = "s3.amazonaws.com/content.udacity-data.com";

  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:s:t:n:z:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 's': // file-path
        //cache_server_addr = optarg;
        fprintf(stderr,"NOT IMPLEMENTED\n");
        break;
      case 'n': // file-path
        num_segments = atoi(optarg);
        break;
      case 'z': // file-path
        segment_size = atoi(optarg);
        break;
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
    }
  }

  /* SHM initialization...*/
    //*******CREATE STEQUE*******
    char shm_name[256];
    int shm_fd;

    fprintf(stderr,"***INIT SHARED MEMORY QUEUE***\n\n");

    //queue is global in shm_channel.h
    steque_init(&SHM_FD_QUEUE);

    //*******CREATE SHARED MEMORY SEGMENT********
    for(int i = 0; i < num_segments; i++) {

      sprintf(shm_name, "/segment_%d", i);
      fprintf(stderr,"***Creating and Opening %s\n***",shm_name);
      shm_fd = shm_open(shm_name, O_CREAT, 0666);
      if (shm_fd == -1) {
        perror("fd_sh:ERROR");
        return -1;
      }
      //*******TRUNCATE TO SEGMENT SIZE VALUE*********
      ftruncate(shm_fd,segment_size);
      steque_item steqi = shm_name;
      steque_push(&SHM_FD_QUEUE,steqi);
      fprintf(stderr,"***%s added to queue\n***",shm_name);
    }


  /*Initializing server*/
  gfserver_init(&gfs, nworkerthreads);
  fprintf(stderr,"***GF server initialized***\n\n");
  /*Setting options*/
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);

  fprintf(stderr,"***spin up handler threads***\n\n");
  for(i = 0; i < nworkerthreads; i++) {
    //gfs.contexts->arg = server;
    //send queue of shared memory to handler
    //steque_item item_to_pass = steque_front(&SHM_FD_QUEUE);

    struct shm_information info;
    info.cache_server_addr = cache_server_addr;
    info.segment_name = "test";
    info.segment_size = segment_size;

    gfserver_setopt(&gfs, GFS_WORKER_ARG, i,&info);
    //steque_cycle(&SHM_FD_QUEUE);

  }


  /*Loops forever*/
  gfserver_serve(&gfs);
}