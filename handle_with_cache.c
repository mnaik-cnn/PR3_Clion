#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>




#include "gfserver.h"
#include "simplecache.h"
#include "shm_channel.h"


//Replace with an implementation of handle_with_cache and any other
//functions you may need.


//extern int simplecache_init(char *filename);
//int simplecache_get(char *key);

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg){

	size_t file_len, bytes_transferred;
	ssize_t write_len;

	//************SET QUEUE OF SHARED MEMORY TO ARGUMENT***********
		fprintf(stderr,"***STARTING HANDLER ROUTINE***\n");
		//we may need to initialize this in web proxy according to instructions
		int shm_fd;
		int file_size = 0;

		struct shm_information info = *(struct shm_information *)arg;

		//a request has come in, as the call back, we need to retrieve a shared memory descriptor

		//1.) communicate request via socket to simplecached, the file name and the file descriptor
		//hard code connection info

		char *server_address = info.cache_server_addr;
		int nHostPort = 8887;
		int hSocket;                 /* handle to socket */
		struct hostent *pHostInfo;   /* holds info about a machine */
		struct sockaddr_in Address;  /* Internet socket address stuct */
		long nHostAddress;


		//------------------SOCKET STUFF---------------------------
		fprintf(stderr,"\nMaking a socket");
		/* make a socket */
		hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (hSocket == SOCKET_ERROR) {
			fprintf(stderr,"\nCould not make a socket\n");
			return 0;
		}
		/* get IP address from name */
		pHostInfo = gethostbyname(server_address);
		/* copy address into long */
		memcpy(&nHostAddress, pHostInfo->h_addr, pHostInfo->h_length);
		/* fill address struct */
		Address.sin_addr.s_addr = nHostAddress;
		Address.sin_port = htons(nHostPort);
		Address.sin_port = htons(nHostPort);
		Address.sin_family = AF_INET;
		fprintf(stderr,"\nConnecting to %s on port %d\n", server_address, nHostPort);
		/* connect to host */
		if (connect(hSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR) {
			fprintf(stderr,"\nCould not connect to host\n");
			return 0;
		}
		//......here's the file name..
		char file_name[256];//strrchr(path, '/');
		//if (file_name[0] == '/')
			//file_name++;
		file_name[0] = '\0';
		//strcpy(file_name,".");
		strcat(file_name,path);

		//*********OPEN STRUCTURE**********
		steque_item item_to_pass = steque_front(&SHM_FD_QUEUE);
		info.segment_name = item_to_pass;

		shm_fd = shm_open(info.segment_name, O_CREAT | O_RDWR, 0666);
		if (shm_fd == -1) {
			perror("fd_sh:ERROR");
			return -1;
		}
		//*******MAP STRUCTURE TO SEGMENT*********
	    struct shm_data_struct* chunk_recv = mmap(0,info.segment_size,O_RDWR, MAP_SHARED, shm_fd, 0);
	    //void* test_string = mmap(0, MAX_FILE_SIZE_BYTES,O_RDWR, MAP_SHARED, shm_fd, 0);
		if (chunk_recv == MAP_FAILED) {
			fprintf(stderr,"Map failed\n");
			return -1;
		}

		//initalize mutexes here
		pthread_mutexattr_t mutex_attr;
		pthread_mutexattr_init(&mutex_attr);
		pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&chunk_recv->m, &mutex_attr);

		pthread_condattr_t cond_attr_read;
		pthread_condattr_init(&cond_attr_read);
		pthread_condattr_setpshared(&cond_attr_read, PTHREAD_PROCESS_SHARED);
		pthread_cond_init(&chunk_recv->cond_shm_read, &cond_attr_read);

		pthread_condattr_t cond_attr_write;
		pthread_condattr_init(&cond_attr_write);
		pthread_condattr_setpshared(&cond_attr_write, PTHREAD_PROCESS_SHARED);
		pthread_cond_init(&chunk_recv->cond_shm_write, &cond_attr_write);

		//int shared_memory_size = sizeof(shm_data);
		int transfer_size = info.segment_size - sizeof(chunk_recv);
		//fprintf(stderr,"TRANSFER SIZE: %d\n",transfer_size);

	    fprintf(stderr,"\n***WRITING %s TO SOCKET***\n",file_name);
	    write(hSocket,file_name, 256 * sizeof(char));

		//tell simplecache where to write the file
		fprintf(stderr,"\n***WRITING (%s) NAME OF SHARED MEMORY TO SOCKET***\n",info.segment_name);
		write(hSocket,info.segment_name, 256 * (sizeof(char)));

		//tell simplecache the size of the file the file
		fprintf(stderr,"\n***WRITING (%d) SIZE OF SHARED MEMORY TO SOCKET***\n",info.segment_size);
		write(hSocket,&info.segment_size,sizeof(int));

		//ask for the file size so we can allocate memory efficiently
		read(hSocket,&file_size,sizeof(file_size));
		fprintf(stderr,"\n***RETRIEVED \"%d BYTES\" FROM SOCKET***\n",file_size);
		char *full_buffer;

		if(file_size > 0) {

			file_len = file_size;
			ssize_t FILE_REMAINING = file_len;
			msync(chunk_recv, info.segment_size, MS_SYNC | MS_INVALIDATE);
			full_buffer = malloc(file_len);

			if (chunk_recv->file_size == 0) {
				fprintf(stderr,"\n***SEND F_N_F BACK TO CLIENT FOR %s***\n", file_name);
				return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
			}
			else {
				fprintf(stderr,"\n***SEND GF_OK - FILE LEN %ld BACK TO CLIENT***\n", file_len);
				gfs_sendheader(ctx, GF_OK, file_len);


				while (FILE_REMAINING > 0) {
					while (chunk_recv->read_write_flag == 1) {
						//fprintf(stderr,"FLAG IS SET TO %d\n",chunk_recv->read_write_flag);
						msync(chunk_recv, chunk_recv->segment_size, MS_SYNC | MS_INVALIDATE);
						//pthread_cond_wait(&chunk_recv->cond_shm_read, &chunk_recv->m);

					}
					if (FILE_REMAINING <= transfer_size) {
						transfer_size = FILE_REMAINING;
					}

					pthread_mutex_lock(&chunk_recv->m);
						chunk_recv->read_write_flag = 0;
					pthread_mutex_unlock(&chunk_recv->m);

						fprintf(stderr,"***HANDLER LOCKED***\n");
						char *temp_buffer = malloc(transfer_size);
						temp_buffer[0] = '\0';
						memcpy(temp_buffer, &chunk_recv->data, transfer_size);

						//(char *) &chunk_recv->data;//,chunk_recv->file_size);
						//fprintf(stderr,"chunk data: %s\n\n", (char *) &chunk_recv->data);
						//fprintf(stderr,"handler temp buff: %s\n", temp_buffer);

						write_len = gfs_send(ctx, temp_buffer, transfer_size);
						bytes_transferred += write_len;
						//memcpy(full_buffer + ptr_count, temp_buffer, transfer_size);
						FILE_REMAINING -= transfer_size;
						fprintf(stderr,"***FILE REMAINING: %ld***\n", FILE_REMAINING);
						//ptr_count += transfer_size;

					pthread_mutex_lock(&chunk_recv->m);
						chunk_recv->read_write_flag = 1;
					pthread_mutex_unlock(&chunk_recv->m);
					fprintf(stderr,"***HANDLER UNLOCKED***\n");

					free(temp_buffer);

					pthread_cond_broadcast(&chunk_recv->cond_shm_write);
					fprintf(stderr,"***CACHE SIGNALED***\n");
				}
			}
			free(full_buffer);
		}
		else{
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		}

		steque_push(&SHM_FD_QUEUE,item_to_pass);

		munmap(chunk_recv,chunk_recv->segment_size);
		close(shm_fd);
		close(hSocket);

		return bytes_transferred;
}

