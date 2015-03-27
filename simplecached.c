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

int conn_read_write_flag = 1;
int writer_waiting = 0;

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

	printf("***INIT CONNECTION QUEUE***\n");
	steque_init(&connection_queue);

	int nthreads = 1;
	char *cachedir = "locals.txt";

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

	printf("***Initializing the cache boss***\n");
	printf("***Cache Dir = %s***\n", cachedir);
	simplecache_init(cachedir);
	printf("***NUM CACHE THREADS: %d***\n", nthreads);

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
		//do{
			return 0;
			//sleep(1);
		//} while (bind(hServerSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR);

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


		while(conn_read_write_flag == 0)
		{
			writer_waiting ++;
			printf("waiting for write flag.\n");
		}
		pthread_mutex_lock(&m);
			conn_read_write_flag = 1;
		pthread_mutex_unlock(&m);

		steque_push(&connection_queue,queue_item);

		pthread_mutex_lock(&m);
			conn_read_write_flag = 0;
			writer_waiting --;
		pthread_mutex_unlock(&m);

		//pthread_cond_broadcast(&c_cache_get_connection);

		printf("***CONNECTION ADDED, QUEUE HAS %d ITEMS***\n",steque_size(&connection_queue));
	}
}
void *doWorkWithSocket() {
	//lock queue
	while (1) {

		printf("Thread is working.\n");
		steque_item work_item;

		//***********IF QUEUE IS EMPTY THEN WAIT****************

		//while (steque_isempty(&connection_queue)) {
		//printf("A thread is waiting.\n");
		//pthread_cond_wait(&c_cache_get_connection, &m);
		//}
		printf("Thread awoke, work to do.\n");

		//ENTER CRITICAL SECTION

		int is_empty = 0;

		while (conn_read_write_flag == 1) {
			//wait
			//printf("waiting for read flag.\n");
		}
		pthread_mutex_lock(&m);
		conn_read_write_flag = 0;
		printf("write flag set.\n");
		pthread_mutex_unlock(&m);

		if (conn_read_write_flag == 0) {
			printf("Thread in critical section.\n");

			if (steque_isempty(&connection_queue)) {
				is_empty = 1;
				printf("nothing in queue.\n");
			}
			else {
				is_empty = 0;
				work_item = steque_pop(&connection_queue);
			}
		}

		pthread_mutex_lock(&m);
		if ((writer_waiting > 0) || (is_empty == 1));
		conn_read_write_flag = 1;
		pthread_mutex_unlock(&m);

		if (is_empty == 0) {

			printf("***QUEUE POPPED, HAS %d ITEMS***\n", steque_size(&connection_queue));
			printf("A thread is has a work item.\n");
			//EXIT CRITICAL SECTION

			char *full_file_path = malloc(256 * sizeof(char));
			char *file_name = malloc(256 * sizeof(char));
			char *shm_name = malloc(256 * (sizeof(char)));
			//int shared_memory_size = 1500;

			//*******GET SOCKET FROM WORK ITEM QUEUE*******
			int hSocket = *(int *) work_item;

			printf("A thread has socket %d\n", hSocket);


			//**********RECEIVE FILE NAME**************
			read(hSocket, file_name, 256 * (sizeof(char)));
			printf("\n***FILE TO PLACE IN SHARED MEM IS %s***\n", file_name);

			full_file_path[0] = '\0';
			strcat(full_file_path, file_name);

			printf("\n***FULL FILE PATH: %s\n", full_file_path);

			//***********RECEIVE SHARED MEM NAME*************
			read(hSocket, shm_name, 256 * sizeof(char));
			printf("\n***MEMORY SHARE IS NAMED %s***\n", shm_name);

			int segment_size;
			//***********RECEIVE SEGMENT SIZE*************
			read(hSocket, &segment_size, sizeof(int));
			printf("\n***MEMORY SEGMENT SIZE IS NAMED %d***\n", segment_size);


			//get from cache and read into shared memory
			int shm_fd;
			int cache_fd = simplecache_get(full_file_path);

			int file_len;
			if (cache_fd < 0) {
				//file not found
				file_len = 0;
				printf("\n***FILE SIZE TO SEND%d***\n", file_len);
				write(hSocket, &file_len, sizeof(file_len));
			}
			else {

				printf("\n***CACHE FILE DESCRIPTOR IS %d***\n", cache_fd);
				//calculate and return file size
				file_len = lseek(cache_fd, 0, SEEK_END);
				printf("\n***FILE SIZE TO SEND%d***\n", file_len);
				write(hSocket, &file_len, sizeof(file_len));
				//get file, return file size

				/* open the shared memory segment */
				shm_fd = shm_open(shm_name, O_RDWR, 0666);
				if (shm_fd == -1) {
					printf("shared memory failed %s\n", strerror(errno));
					exit(-1);
				}

				/* now map the shared memory segment in the address space of the process */
				struct shm_data_struct *chunk;// = malloc(sizeof(struct shm_data_struct));
				chunk = mmap(0, segment_size, O_RDWR, MAP_SHARED, shm_fd, 0);
				printf("***sizeof(segment) %d ***\n", segment_size);
				printf("***sizeof(struct shm_data_struct) %ld ***\n", sizeof(struct shm_data_struct));
				//chunk->buffer_size = segment_size - sizeof(chunk);
				//printf("***sizeof(buffer) %ld ***\n", chunk->buffer_size);
				//chunk->data = malloc(chunk->buffer_size);
				printf("***sizeof(chunk) %ld ***\n", sizeof(chunk));
				chunk->file_size = file_len;
				printf("***sizeof(chunk) %ld ***\n", sizeof(chunk));

				int transf_size = (segment_size - sizeof(chunk));
				printf("TRANSFER SIZE: %d\n", transf_size);
				chunk->buffer_size = transf_size;

				// this is important, this tells each side we should write first
				chunk->read_write_flag = 1;

				if (chunk == MAP_FAILED) {
					printf("Map failed %s\n", strerror(errno));

					exit(-1);
				}
				printf("writing to shared memory\n");

				//*********************WRITE FILE TO SHARED MEMORY *************************

				char *temp_buffer = malloc(file_len);

				int cache_to_buffer_transf_size = file_len;
				printf("CACHE TO BUFFER TRANSFER SIZE: %d\n", cache_to_buffer_transf_size);

				//TWO USE CASES
				//1.) SEGMENT HOLDS WHOLE FILE AND TRANSFER SIZE IS SMALLER THAN THAT FILE
				//2.) TRANSFER SIZE IS LARGER THAN SEGMENT

				printf("\n***READING FILE FROM CACHE FILE DESC(%d) TO BUFFER - SAVING TO SHARED MEM FILE DESC (%d)***\n", cache_fd, shm_fd);
				size_t FILE_REMAINING = file_len;

				//-------------------FILE TRANSFER FROM CACHE TO BUFFER---------------------
				ssize_t TOTAL_SIZE_SENT = 0;
				ssize_t SIZE_SENT = 0;
				while (FILE_REMAINING > 0) {
					if (FILE_REMAINING >= cache_to_buffer_transf_size) {
						SIZE_SENT = pread(cache_fd, temp_buffer, cache_to_buffer_transf_size, TOTAL_SIZE_SENT);
						printf("***READ %ld BYTES OUT OF %ld***\n", SIZE_SENT, FILE_REMAINING);
					}
					else {
						SIZE_SENT = pread(cache_fd, temp_buffer, FILE_REMAINING, TOTAL_SIZE_SENT);
						printf("***READ %ld BYTES OUT OF %ld***\n", SIZE_SENT, FILE_REMAINING);
					}
					TOTAL_SIZE_SENT += SIZE_SENT;
					printf("***TOTAL SIZE SENT: %ld***\n", TOTAL_SIZE_SENT);
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
				//printf("***TEMP BUFF CONTENTS: %s***\n", temp_buffer);
				//****LOOP TO COPY OVER SHARED MEM*****
				FILE_REMAINING = file_len;
				int ptr_count = 0;

				printf("TRANSFER SIZE: %d\n", transf_size);

				// this was testing cache transfer
				/*
			FILE* pFile;
			char* yourFilePath  = "TEST_FILE.jpeg";
			pFile = fopen(yourFilePath,"wb");
			if (pFile){
				fwrite(temp_buffer,file_len, 1, pFile);
				puts("Wrote to file!");
			}
			else{
				puts("Something wrong writing to File.");
			}
			fclose(pFile);*/

				int bytesRead = 0;
				//transfer size set to buffer chunk size before
				//----------------------------CHUNKING UP BUFFER TO SHARE MEM------------------------
				while (FILE_REMAINING > 0) {

					if (FILE_REMAINING <= transf_size) {
						transf_size = FILE_REMAINING;
					}

					while (chunk->read_write_flag == 0) {
						msync(chunk, chunk->segment_size, MS_SYNC | MS_INVALIDATE);
						//printf("WAITING\n");
						//pthread_cond_wait(&chunk->cond_shm_write, &chunk->m);
					}
					pthread_mutex_lock(&chunk->m);
					printf("***CACHE LOCKED***\n");
					chunk->read_write_flag = 1;
					pthread_mutex_unlock(&chunk->m);

					//memcpy(&chunk->data, temp_buffer + ptr_count, transf_size);
					memcpy(chunk->data, &temp_buffer[bytesRead], transf_size);
					//memcpy(chunk_buffer, temp_buffer + ptr_count, transf_size);
					//memcpy(&chunk->data, chunk_buffer, transf_size);
					//chunk->data = chunk_buffer;
					//printf("***chunk buffer:%s***\n", chunk_buffer);
					//printf("***chunk.data:%s***\n", (char *) &chunk->data);

					pthread_mutex_lock(&chunk->m);
					chunk->read_write_flag = 0;
					pthread_mutex_unlock(&chunk->m);
					printf("***CACHE UNLOCKED***\n");

					pthread_cond_broadcast(&chunk->cond_shm_read);
					printf("***HANDLER SIGNALED***\n");

					FILE_REMAINING -= transf_size;
					printf("***FILE REMAINING: %ld***\n", FILE_REMAINING);
					ptr_count += transf_size;
					bytesRead += transf_size;
					printf("ptr-count: %d\n\n", ptr_count);
				}

				printf("OUT OF WHILE LOOP TO COPY FILE\n\n");

				//printf("\n\ntemp buff: %s\n\n",temp_buffer);
				munmap(chunk, chunk->segment_size);
				free(file_name);
				free(shm_name);
				free(full_file_path);
			}
			close(shm_fd);
			close(hSocket);
		}
	}

	return NULL;
}





