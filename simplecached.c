#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <pthread.h>
#include <sys/sendfile.h>


#include "shm_channel.h"
#include "simplecache.h"
#include "steque.h"

#define SOCKET_ERROR -1
#define MAX_CACHE_REQUEST_LEN 256


steque_t connection_queue;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c_cache_get_connection = PTHREAD_COND_INITIALIZER;
pthread_cond_t c_cache_add_connection = PTHREAD_COND_INITIALIZER;

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
			printf("\nTHREAD %d CREATED\n", t);
		}
	}

	// what is int i for???
	//int i;
	char *cachedir = "locals.txt";
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

		printf("\nBoss is waiting for a connection\n");
		/* get the connected socket */
		hSocket = accept(hServerSocket, (struct sockaddr *) &Address, (socklen_t *) &nAddressSize);
		printf("\nGot a connection on socket %d\n", hSocket);

		//PUT IN CONNECTION QUEUE
		steque_item queue_item;
		queue_item = &hSocket;

		pthread_mutex_lock(&m);
			steque_push(&connection_queue,queue_item);
		pthread_mutex_unlock(&m);
		pthread_cond_broadcast(&c_cache_get_connection);

		printf("***QUEUE HAS %d ITEMS***\n",steque_size(&connection_queue));

	}
}
void *doWorkWithSocket()
{
	//lock queue
	while(1) {

		printf("Thread is working.\n");
		steque_item work_item;

		//***********IF QUEUE IS EMPTY THEN WAIT****************
		//ENTER CRITICAL SECTION
		pthread_mutex_lock(&m);
		while (steque_isempty(&connection_queue)) {
			printf("A thread is waiting.\n");
			pthread_cond_wait(&c_cache_get_connection, &m);
		}

		//POP ITEM
		work_item = steque_pop(&connection_queue);
		printf("***QUEUE POPPED, HAS %d ITEMS***\n", steque_size(&connection_queue));
		printf("A thread is has a work item.\n");
		pthread_mutex_unlock(&m);
		//EXIT CRITICAL SECTION
		//if(steque_size(&connection_queue) > 0) {

		char *full_file_path = malloc(256 * sizeof(char));
		char *file_name = malloc(256 * sizeof(char));
		char *shm_name = malloc(256 * (sizeof(char)));
		//int shared_memory_size = 1500;

		//*******GET SOCKET FROM WORK ITEM QUEUE*******
		int hSocket = *(int *) work_item;

		printf("A thread has socket %d\n", hSocket);

		//LOOK HERE!
		//RECEIVE FILE NAME
		read(hSocket, file_name, 256 * (sizeof(char)));
		printf("\n***FILE TO PLACE IN SHARED MEM IS %s***\n", file_name);

		full_file_path[0] = '\0';
		//full_file_path = strrchr(file_name, './');
		//strcpy(full_file_path,".");
		strcat(full_file_path, file_name);

		//if (full_file_path[0] == '/') {
		//full_file_path++;

		//}

		printf("\n***FULL FILE PATH: %s\n", full_file_path);

		//RECEIVE FILE DESCRIPTOR
		read(hSocket, shm_name, 256 * sizeof(char));
		printf("\n***MEMORY SHARE IS NAMED %s***\n", shm_name);


		//get how much memory to allocate for shared mem
		//write(hSocket,&shared_memory_size, sizeof(shared_memory_size));
		//printf("\n***SHARED MEM SIZE TO ALLOCATE IS %d***\n",shared_memory_size);

		//get from cache and read into shared memory
		int shm_fd;

		int cache_fd = simplecache_get(full_file_path);

		printf("\n***CACHE FILE DESCRIPTOR IS %d***\n", cache_fd);
		//calculate and return file size
		int file_len = lseek(cache_fd, 0, SEEK_END);
		printf("\n***FILE SIZE TO SEND S %d***\n", file_len);
		write(hSocket, &file_len, sizeof(file_len));
		//get file, return file size


		char *shm_base_addr;    // base address, from mmap()
		//char *ptr;		// shm_base is fixed, ptr is movable

		/* open the shared memory segment */
		shm_fd = shm_open(shm_name, O_RDWR, 0666);
		if (shm_fd == -1) {
			printf("shared memory failed %s\n", strerror(errno));
			exit(-1);
		}

		/* now map the shared memory segment in the address space of the process */
		shm_base_addr = mmap(0, MAX_FILE_SIZE_BYTES, O_RDWR, MAP_SHARED, shm_fd, 0);
		// ptr = mmap(0, MAX_FILE_SIZE_BYTES, O_RDWR, MAP_SHARED, shm_fd, 0);
		if (shm_base_addr == MAP_FAILED) {
			printf("Map failed %s\n", strerror(errno));

			exit(-1);
		}

		printf("writing to shared memory\n");

		char *ptr = shm_base_addr;
		ptr += sprintf(ptr, "%s", "who else seen da leprechaun");
		printf("ptr: %s\n", shm_base_addr);




		//ptr = &shm_data;

		//write(hSocket,"OK",256 * sizeof(char));

		//printf("***SHARED MEM CASTED TO POINTER, STRUCT SIZE IS %ld***\n",sizeof(shm_data));

		//*********************WRITE FILE TO SHARED MEMORY *************************

		char *temp_buffer = malloc(file_len);


		printf("\n***READING FILE FROM CACHE FILE DESC(%d) TO BUFFER - SAVING TO SHARED MEM FILE DESC (%d)***\n", cache_fd, shm_fd);
		size_t FILE_REMAINING = file_len;

		ssize_t TOTAL_SIZE_SENT = 0;
		ssize_t SIZE_SENT = 0;
		while (FILE_REMAINING > 0) {
			if (FILE_REMAINING >= 4096) {
				SIZE_SENT = pread(cache_fd, temp_buffer, 4096, 0);
				printf("***WROTE %ld BYTES OUT OF %ld***\n",SIZE_SENT,FILE_REMAINING);
			}
			else {
				SIZE_SENT = pread(cache_fd, temp_buffer, FILE_REMAINING, 0);
				printf("***WROTE %ld BYTES OUT OF %ld***\n",SIZE_SENT,FILE_REMAINING);
			}
			printf("***TOTAL SIZE SENT: %ld***\n", TOTAL_SIZE_SENT);
			TOTAL_SIZE_SENT += SIZE_SENT;
			FILE_REMAINING = FILE_REMAINING - SIZE_SENT;
			printf("***%ld BYTES REMINAING OUT OF %d***\n", FILE_REMAINING, file_len);

			if (SIZE_SENT == -1) {
				printf("\nsomething went ong with sendfile()!...Errno %d %s\n", errno, strerror(errno));
				fprintf(stderr, "\nerror.. %ld of %d bytes sent\n", SIZE_SENT, file_len);
				exit(1);
			}
		}

		if (TOTAL_SIZE_SENT != file_len) {
			fprintf(stderr, "incomplete transfer from sendfile: %ld of %d bytes\n", SIZE_SENT, file_len);
			exit(1);
		}

		printf("***SIZE SENT: %ld***\n", SIZE_SENT);
		//we forgot to close socket, take care of this eventually

		memcpy(shm_base_addr, temp_buffer, file_len);
		munmap(shm_base_addr, MAX_FILE_SIZE_BYTES);


		free(file_name);
		free(shm_name);
		free(full_file_path);

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





