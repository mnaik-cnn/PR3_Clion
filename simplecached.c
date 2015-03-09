#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <pthread.h>


#include "shm_channel.h"
#include "simplecache.h"
#include "steque.h"

#define SOCKET_ERROR -1
#define MAX_CACHE_REQUEST_LEN 256


steque_t connection_queue;

void *doWorkWithSocket();


static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		//shmc_unlink(shmc);
		/*!!!!!!!!!!!!!!!!!!!!UNLINK IPC MECHANISMS HERE!!!!!!!!!!!!!!!!!!!*/
		//make sure we uncomment that !!!!
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

	//init connection queue

	printf("***INIT CONNECTION QUEUE***");
	steque_init(&connection_queue);


	int nthreads = 1;

	pthread_t threads[nthreads];
	for (int t = 0; t < nthreads; t++) {
		int rc;

		//--------CREATE POOL OF THREADS-----------------
		//-----(no params for threads because they will get work from the queue)-------
		rc = pthread_create(&threads[t], NULL, doWorkWithSocket, NULL);
		if (rc) {
			printf("\nERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
		else {
			printf("THREAD %d CREATED\n", t);
		}
	}

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

	if (signal(SIGINT, _sig_handler) == SIG_ERR) {
		fprintf(stderr, "Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
		fprintf(stderr, "Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}

	printf("\n***Initializing the cache***\n");
	printf("\n***Cache Dir = %s***\n", cachedir);

	simplecache_init(cachedir);

	printf("\n***NUM CACHE THREADS: %d***\n", nthreads);



	//1.)boss thread gets connection, sends details to thread and spins it up
	//-----------------------!!!SOCKET STUFF!!!-----------------------
	int hServerSocket;
	//------------MACHINE INFO-------------------
	//SOCKET ADDRESS
	struct sockaddr_in Address;
	int nAddressSize = sizeof(struct sockaddr_in);
	int nHostPort = 8887;
	printf("\n***CREATING A SERVER SOCKET***\n");
	//-------MAKE A SERVER SOCKET------------
	int hSocket;
	int returnCode = CreateServerSocket(&hServerSocket);
	if (returnCode == 0) {
		printf("\nCould not make a socket\n");
		return 0;
	}
	//----------FILL ADDRESS DETAILS------------------
	FillAddress(&Address, nHostPort);
	//----------BIND TO SOCKET TO PORT----------------
	printf("\n***Binding to port %d***", nHostPort);
	if (bind(hServerSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR) {
		printf("\nCould not connect to host,error: %s\n", strerror(errno));


		return 0;
	}
	//-----------GET PORT NUMBER-------------------
	getsockname(hServerSocket, (struct sockaddr *) &Address, (socklen_t *) &nAddressSize);
	printf("\nOpened socket as fd (%d) on port (%d) for stream i/o", hServerSocket, ntohs(Address.sin_port));
	printf("\nMaking a listen queue of %d elements", QUEUE_SIZE);
	/* establish listen queue */
	//-------------LISTEN FOR CONNECTIONS FOREVER----------------
	while (1) {
		if (listen(hServerSocket, QUEUE_SIZE) == SOCKET_ERROR) {
			printf("\nCould not listen\n");
			return 0;
		}

		printf("\nWaiting for a connection\n");
		/* get the connected socket */
		hSocket = accept(hServerSocket, (struct sockaddr *) &Address, (socklen_t *) &nAddressSize);
		printf("\nGot a connection on socket %d\n", hSocket);

		//PUT IN CONNECTION QUEUE
		steque_item queue_item;
		queue_item = &hSocket;
		steque_push(&connection_queue,queue_item);
		printf("***QUEUE HAS %d ITEMS***\n",steque_size(&connection_queue));

	}
}
void *doWorkWithSocket()
{
	//lock queue
	while(1)
		if(steque_size(&connection_queue)>0) {

			char* file_name = malloc(256 * sizeof(char));
			steque_item work_item = steque_pop(&connection_queue);
			int hSocket = *(int *) work_item;

			printf("A thread has socket %d\n", hSocket);

			//LOOK HERE!
			//WAIT FOR A FILE NAME AND RETURN A FILE DESCRIPTOR FOR SHARED MEMORY
			//oh thanks for the file name
			read(hSocket,file_name, 256 * (sizeof(char)));
			printf("\n***FILE TO PLACE IN SHARED MEM IS %s***\n",file_name);

			// do simple cache stuff here !!!
			// do all the shared memory stuff here!!!!

			char* fake_shm_segment = "/foobar123";
			// heres the name of the shared memory segment
			write(hSocket,fake_shm_segment, 256 * sizeof(char));

			//we forgot to close socket, take care of this eventually

		}
	return NULL;
}


	//2.) boss grab 1st data structure from socket....maybe includes the requested file name and file descriptor for shared memory segment

	//3. )boss should have already downloaded the files using CURL to static file path
	//^^^^^^^^^^^ DON"T FUCK THIS UP THIS TIME, DEFAULT PATH IS "./"

	//4.) after that, lock and access shared memory segment and copy the retrieved file into whatever structure is available.

	//5.) once done, join?


	//printf("\n***STATUS FOR %s is %d***\n",,status);

	//Your code here...





