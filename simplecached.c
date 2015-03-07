#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <w32api/dde.h>

#include "shm_channel.h"
#include "simplecache.h"

#define MAX_CACHE_REQUEST_LEN 256

static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		//shmc_unlink(shmc);
		/*!!!!!!!!!!!!!!!!!!!!UNLINK IPC MECHANISMS HERE!!!!!!!!!!!!!!!!!!!*/
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"                              

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"nthreads",           required_argument,      NULL,           't'},
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
	int nthreads = 1;

	// what is int i for???
	//int i;
	char *cachedir = "workload_cache.txt";
	char option_char;


	while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			default:
				Usage();
				exit(1);
		}
	}

	if (signal(SIGINT, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}

	printf("\n***Initializing the cache***\n");
	printf("\n***Cache Dir = %s***\n",cachedir);
	simplecache_init(cachedir);

	printf("***NUM CACHE THREADS: %d***\n",nthreads);


	//1.)boss thread gets connection, sends details to thread and spins it up
	//-------------LISTEN FOR CONNECTIONS FOREVER----------------

	int hSocket;
	int hServerSocket = getServerSocket();

	while(1) {
		if (listen(hServerSocket, QUEUE_SIZE) == SOCKET_ERROR) {
			printf("\nCould not listen\n");
			return 0;
		}
		printf("\nWaiting for a connection\n");
		/* get the connected socket */
		hSocket = accept(hServerSocket, (struct sockaddr *) &Address, (socklen_t *) &nAddressSize);
		printf("\nGot a connection\n");
	}



	//2.) boss grab 1st data structure from socket....maybe includes the requested file name and file descriptor for shared memory segment

	//3. )have thread access retrieve file into memory using CURL

	//4.) after that, lock and access shared memory segment and copy the retrieved file into whatever structure is available.

	//5.) once done, join?


	//printf("\n***STATUS FOR %s is %d***\n",,status);

	//Your code here...
}