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

	fprintf(stderr,"***INIT CONNECTION QUEUE***\n");
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
			fprintf(stderr,"\nERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
		else {
			fprintf(stderr,"\nTHREAD %d CREATED\n", t);
		}
	}


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

	fprintf(stderr,"***Initializing the cache boss***\n");
	fprintf(stderr,"***Cache Dir = %s***\n", cachedir);
	simplecache_init(cachedir);
	fprintf(stderr,"***NUM CACHE THREADS: %d***\n", nthreads);

	//1.)boss thread gets connection, sends details to thread and spins it up
	//-----------------------!!!SOCKET STUFF!!!-----------------------
	int hServerSocket;
	//------------MACHINE INFO-------------------
	//SOCKET ADDRESS
	struct sockaddr_in Address;
	int nAddressSize = sizeof(struct sockaddr_in);
	int nHostPort = 8887;
	fprintf(stderr,"\n***CREATING A SERVER SOCKET***\n");
	//-------MAKE A SERVER SOCKET------------
	int hSocket;
	int returnCode = CreateServerSocket(&hServerSocket);
	if (returnCode == 0) {
		fprintf(stderr,"\nCould not make a socket\n");
		return 0;
	}
	//----------FILL ADDRESS DETAILS------------------
	FillAddress(&Address, nHostPort);
	//----------BIND TO SOCKET TO PORT----------------
	fprintf(stderr,"\n***Binding to port %d***", nHostPort);
	if (bind(hServerSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR) {
		fprintf(stderr,"\nCould not connect to host,error: %s\n", strerror(errno));
		//do{
			return 0;
			//sleep(1);
		//} while (bind(hServerSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR);

	}
	//-----------GET PORT NUMBER-------------------
	getsockname(hServerSocket, (struct sockaddr *) &Address, (socklen_t *) &nAddressSize);
	fprintf(stderr,"\nOpened socket as fd (%d) on port (%d) for stream i/o", hServerSocket, ntohs(Address.sin_port));
	fprintf(stderr,"\nMaking a listen queue of %d elements", QUEUE_SIZE);
	/* establish listen queue */
	//-------------LISTEN FOR CONNECTIONS FOREVER----------------
	while (1) {
		if (listen(hServerSocket, QUEUE_SIZE) == SOCKET_ERROR) {
			fprintf(stderr,"\nCould not listen\n");
			return 0;
		}
		fprintf(stderr,"\nBoss is waiting for a connection\n");
		/* get the connected socket */
		hSocket = accept(hServerSocket, (struct sockaddr *) &Address, (socklen_t *) &nAddressSize);
		fprintf(stderr,"\nGot a connection on socket %d\n", hSocket);

		//PUT IN CONNECTION QUEUE
		steque_item queue_item;
		queue_item = &hSocket;


		while(conn_read_write_flag == 0)
		{
			writer_waiting ++;
			fprintf(stderr,"waiting for write flag.\n");
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

		fprintf(stderr,"***CONNECTION ADDED, QUEUE HAS %d ITEMS***\n",steque_size(&connection_queue));
	}
}
void *doWorkWithSocket() {
	//lock queue
	while (1) {

		fprintf(stderr,"Thread is working.\n");
		steque_item work_item;

		//***********IF QUEUE IS EMPTY THEN WAIT****************

		//while (steque_isempty(&connection_queue)) {
		//fprintf(stderr,"A thread is waiting.\n");
		//pthread_cond_wait(&c_cache_get_connection, &m);
		//}
		fprintf(stderr,"Thread awoke, work to do.\n");

		//ENTER CRITICAL SECTION

		int is_empty = 0;

		while (conn_read_write_flag == 1) {
			//wait
			//fprintf(stderr,"waiting for read flag.\n");
		}
		pthread_mutex_lock(&m);
		conn_read_write_flag = 0;
		fprintf(stderr,"write flag set.\n");
		pthread_mutex_unlock(&m);

		if (conn_read_write_flag == 0) {
			fprintf(stderr,"Thread in critical section.\n");

			if (steque_isempty(&connection_queue)) {
				is_empty = 1;
				fprintf(stderr,"nothing in queue.\n");
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

			fprintf(stderr,"***QUEUE POPPED, HAS %d ITEMS***\n", steque_size(&connection_queue));
			fprintf(stderr,"A thread is has a work item.\n");
			//EXIT CRITICAL SECTION

			char *full_file_path = malloc(256 * sizeof(char));
			char *file_name = malloc(256 * sizeof(char));
			char *shm_name = malloc(256 * (sizeof(char)));
			//int shared_memory_size = 1500;

			//*******GET SOCKET FROM WORK ITEM QUEUE*******
			int hSocket = *(int *) work_item;

			fprintf(stderr,"A thread has socket %d\n", hSocket);


			//**********RECEIVE FILE NAME**************
			read(hSocket, file_name, 256 * (sizeof(char)));
			fprintf(stderr,"\n***FILE TO PLACE IN SHARED MEM IS %s***\n", file_name);

			full_file_path[0] = '\0';
			strcat(full_file_path, file_name);

			fprintf(stderr,"\n***FULL FILE PATH: %s\n", full_file_path);

			//***********RECEIVE SHARED MEM NAME*************
			read(hSocket, shm_name, 256 * sizeof(char));
			fprintf(stderr,"\n***MEMORY SHARE IS NAMED %s***\n", shm_name);

			int segment_size;
			//***********RECEIVE SEGMENT SIZE*************
			read(hSocket, &segment_size, sizeof(int));
			fprintf(stderr,"\n***MEMORY SEGMENT SIZE IS NAMED %d***\n", segment_size);


			//get from cache and read into shared memory
			int shm_fd;
			int cache_fd = simplecache_get(full_file_path);

			int file_len;
			if (cache_fd < 0) {
				//file not found
				file_len = 0;
				fprintf(stderr,"\n***FILE SIZE TO SEND%d***\n", file_len);
				write(hSocket, &file_len, sizeof(file_len));
			}
			else {

				fprintf(stderr,"\n***CACHE FILE DESCRIPTOR IS %d***\n", cache_fd);
				//calculate and return file size
				file_len = lseek(cache_fd, 0, SEEK_END);
				fprintf(stderr,"\n***FILE SIZE TO SEND%d***\n", file_len);
				write(hSocket, &file_len, sizeof(file_len));
				//get file, return file size

				/* open the shared memory segment */
				shm_fd = shm_open(shm_name, O_RDWR, 0666);
				if (shm_fd == -1) {
					fprintf(stderr,"shared memory failed %s\n", strerror(errno));
					exit(-1);
				}

				/* now map the shared memory segment in the address space of the process */
				struct shm_data_struct *chunk;// = malloc(sizeof(struct shm_data_struct));
				chunk = mmap(0, segment_size, O_RDWR, MAP_SHARED, shm_fd, 0);
				//fprintf(stderr,"***sizeof(segment) %d ***\n", segment_size);
				//fprintf(stderr,"***sizeof(struct shm_data_struct) %ld ***\n", sizeof(struct shm_data_struct));
				//chunk->buffer_size = segment_size - sizeof(chunk);
				//fprintf(stderr,"***sizeof(buffer) %ld ***\n", chunk->buffer_size);
				//chunk->data = malloc(chunk->buffer_size);
				//fprintf(stderr,"***sizeof(chunk) %ld ***\n", sizeof(chunk));
				chunk->file_size = file_len;
				//fprintf(stderr,"***sizeof(chunk) %ld ***\n", sizeof(chunk));

				int transf_size = (segment_size - sizeof(chunk));
				fprintf(stderr,"TRANSFER SIZE: %d\n", transf_size);
				chunk->buffer_size = transf_size;

				// this is important, this tells each side we should write first
				chunk->read_write_flag = 1;

				if (chunk == MAP_FAILED) {
					fprintf(stderr,"Map failed %s\n", strerror(errno));

					exit(-1);
				}
				fprintf(stderr,"writing to shared memory\n");

				//*********************WRITE FILE TO SHARED MEMORY *************************

				char *temp_buffer = malloc(file_len);

				int cache_to_buffer_transf_size = file_len;
				fprintf(stderr,"CACHE TO BUFFER TRANSFER SIZE: %d\n", cache_to_buffer_transf_size);

				//TWO USE CASES
				//1.) SEGMENT HOLDS WHOLE FILE AND TRANSFER SIZE IS SMALLER THAN THAT FILE
				//2.) TRANSFER SIZE IS LARGER THAN SEGMENT

				fprintf(stderr,"\n***READING FILE FROM CACHE FILE DESC(%d) TO BUFFER - SAVING TO SHARED MEM FILE DESC (%d)***\n", cache_fd, shm_fd);
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
					printf("***%ld BYTES REMAINING OUT OF %d***\n", FILE_REMAINING, file_len);

					if (SIZE_SENT == -1) {
						fprintf(stderr,"\nsomething went ong with sendfile()!...Errno %d %s\n", errno, strerror(errno));
						fprintf(stderr, "\nerror.. %ld of %d bytes sent\n", SIZE_SENT, file_len);
						exit(1);
					}
				}

				if (TOTAL_SIZE_SENT != file_len) {
					fprintf(stderr, "incomplete transfer from sendfile: %ld of %d bytes\n", SIZE_SENT, file_len);
					exit(1);
				}
				fprintf(stderr,"***SIZE SENT: %ld***\n", SIZE_SENT);

				//we forgot to close socket, take care of this eventually
				//fprintf(stderr,"***TEMP BUFF CONTENTS: %s***\n", temp_buffer);
				//****LOOP TO COPY OVER SHARED MEM*****
				FILE_REMAINING = file_len;
				int ptr_count = 0;

				fprintf(stderr,"TRANSFER SIZE: %d\n", transf_size);

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
						//fprintf(stderr,"WAITING\n");
						//pthread_cond_wait(&chunk->cond_shm_write, &chunk->m);
					}
					pthread_mutex_lock(&chunk->m);
					fprintf(stderr,"***CACHE LOCKED***\n");
					chunk->read_write_flag = 1;
					pthread_mutex_unlock(&chunk->m);

					//memcpy(&chunk->data, temp_buffer + ptr_count, transf_size);
					memcpy(chunk->data, &temp_buffer[bytesRead], transf_size);
					//memcpy(chunk_buffer, temp_buffer + ptr_count, transf_size);
					//memcpy(&chunk->data, chunk_buffer, transf_size);
					//chunk->data = chunk_buffer;
					//fprintf(stderr,"***chunk buffer:%s***\n", chunk_buffer);
					//fprintf(stderr,"***chunk.data:%s***\n", (char *) &chunk->data);

					pthread_mutex_lock(&chunk->m);
					chunk->read_write_flag = 0;
					pthread_mutex_unlock(&chunk->m);
					fprintf(stderr,"***CACHE UNLOCKED***\n");

					pthread_cond_broadcast(&chunk->cond_shm_read);
					fprintf(stderr,"***HANDLER SIGNALED***\n");

					FILE_REMAINING -= transf_size;
					fprintf(stderr,"***FILE REMAINING: %ld***\n", FILE_REMAINING);
					ptr_count += transf_size;
					bytesRead += transf_size;
					fprintf(stderr,"ptr-count: %d\n\n", ptr_count);
				}

				fprintf(stderr,"OUT OF WHILE LOOP TO COPY FILE\n\n");

				//fprintf(stderr,"\n\ntemp buff: %s\n\n",temp_buffer);
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





