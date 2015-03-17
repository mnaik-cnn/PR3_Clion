#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gfserver.h"
                                                                \
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                     \
"options:\n"                                                                  \
"  -p [listen_port]    Listen port (Default: 8888)\n"                         \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)"\
"  -h                  Show this help message\n"                              \
"special options:\n"                                                          \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"

\
#define NEW_USAGE\
"usage:\n"                                                     \
"        webproxy [options]\n"                                 \
"options:\n"                                                   \
"-n number of segments to use in communication with cache.\n"  \
"-z the size (in bytes) of the segments.\n"                    \
"-p port for incoming requests\n"                              \
"-t number of worker threads\n"                                \
"-s server address(e.g. 'localhost:8080', or 'example.com')\n" \
"-h print a help message\n"


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


static gfserver_t gfs;

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    gfserver_stop(&gfs);
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int i, option_char = 0;
  unsigned short port = 8888;
  unsigned short nworkerthreads = 1;
  unsigned short num_segments;
  long segment_size;

  char *server = "s3.amazonaws.com/content.udacity-data.com";

  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:t:s:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;
      case 'n': // file-path
        num_segments = optarg;
        break;
      case 'z': // file-path
        segment_size = optarg;
        break;
      case 'h': // help
        fprintf(stdout, "%s", NEW_USAGE);
        exit(0);
        break;       
      default:
        fprintf(stderr, "%s", NEW_USAGE);
        exit(1);
    }
  }
  
  /* SHM initialization...*/

  /*Initializing server*/
  gfserver_init(&gfs, nworkerthreads);

  //initialize shared memory here per instructions...


  /*Setting options*/
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);

  for(i = 0; i < nworkerthreads; i++)
    //gfs.contexts->arg = server;
    //send queue of shared memory to handler
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, server);





  /*Loops forever*/
  gfserver_serve(&gfs);
}