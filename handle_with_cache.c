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

void display(char *prog, char *bytes, int n);
void display(char *prog, char *bytes, int n)
{
	printf("display: %s\n", prog);
	for (int i = 0; i < n; i++)
	{ printf("%02x%c", bytes[i], ((i+1)%16) ? ' ' : '\n'); }
	printf("\n");
}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg){
	//int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	//char buffer[4096];

	//************SET QUEUE OF SHARED MEMORY TO ARGUMENT***********
	//char *data_dir = arg;

	//strcpy(buffer,data_dir);
	//strcat(buffer,path);

		printf("***STARTING HANDLER ROUTINE***\n");
		//we may need to initialize this in web proxy according to instructions

		int shm_fd;
		int file_size = 0;

		//*****IMPORTANT******
		//****SHM_NAME WILL NOW BE THE SHARED SEGMENT PASSED TO US BY WEB_PROXY****
		//****WE MAY ONLY USE THIS ONE SEGMENT***
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
		//char pBuffer[FILE_STORAGE_BUFFER_SIZE];
		//unsigned nReadAmount;
		//------------------SOCKET STUFF---------------------------
		printf("\nMaking a socket");
		/* make a socket */
		hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (hSocket == SOCKET_ERROR) {
			printf("\nCould not make a socket\n");
			return 0;
		}
		/* get IP address from name */
		pHostInfo = gethostbyname(server_address);
		pHostInfo = gethostbyname(server_address);
		/* copy address into long */
		memcpy(&nHostAddress, pHostInfo->h_addr, pHostInfo->h_length);
		/* fill address struct */
		Address.sin_addr.s_addr = nHostAddress;
		Address.sin_port = htons(nHostPort);
		Address.sin_port = htons(nHostPort);
		Address.sin_family = AF_INET;
		printf("\nConnecting to %s on port %d\n", server_address, nHostPort);
		/* connect to host */
		if (connect(hSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR) {
			printf("\nCould not connect to host\n");
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
		shm_fd = shm_open(info.segment_name, O_CREAT | O_RDWR, 0666);
		if (shm_fd == -1) {
			perror("fd_sh:ERROR");
			return -1;
		}


		//*******MAP STRUCTURE TO SEGMENT*********
	    struct shm_data_struct* chunk_recv = mmap(0,info.segment_size,O_RDWR, MAP_SHARED, shm_fd, 0);
	    //void* test_string = mmap(0, MAX_FILE_SIZE_BYTES,O_RDWR, MAP_SHARED, shm_fd, 0);
		if (chunk_recv == MAP_FAILED) {
			printf("Map failed\n");
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
		//create and init mutex
		//pthread_mutexattr_t(&m_attr);
		//pthread_mutexattr_set_pshared(&m_attr, _POSIX_THREAD_PROCESS_SHARED);
		//pthread_mutex_init(&shm_data.m_shm,&m_attr);

		//int shared_memory_size = sizeof(shm_data);
		int transfer_size = info.segment_size - sizeof(chunk_recv);
		printf("TRANSFER SIZE: %d\n",transfer_size);

	    printf("\n***WRITING %s TO SOCKET***\n",file_name);
	    write(hSocket,file_name, 256 * sizeof(char));

		//tell simplecache where to write the file
		printf("\n***WRITING (%s) NAME OF SHARED MEMORY TO SOCKET***\n",info.segment_name);
		write(hSocket,info.segment_name, 256 * (sizeof(char)));

		//tell simplecache the size of the file the file
		printf("\n***WRITING (%d) SIZE OF SHARED MEMORY TO SOCKET***\n",info.segment_size);
		write(hSocket,&info.segment_size,sizeof(int));

		//tell simple cache how much memory to allocate for shared mem
		//printf("\n***WRITING %ld SIZE OF SHARED MEMORY TO SOCKET***\n",sizeof(shm_data));
		//write(hSocket,&shared_memory_size,sizeof(int));

		//ask for the file size so we can allocate memory efficiently
		read(hSocket,&file_size,sizeof(file_size));
		printf("\n***RETRIEVED \"%d BYTES\" FROM SOCKET***\n",file_size);

		//printf("pointer: %p\n",shr_ptr);

		//free this somewhere

		//free(file_size);


		file_len = file_size;



		//int transfer_size = chunk_recv->buffer_size;
		//int transfer_size = info.segment_size - sizeof(chunk_recv);

	    //display("cons", test_string, 64);

		//int ptr_count = 0;
		ssize_t FILE_REMAINING = file_len;



		msync(chunk_recv,info.segment_size,MS_SYNC| MS_INVALIDATE);
		char* full_buffer = malloc(file_len);




		if(chunk_recv->file_size == 0)
		{
			printf("\n***SEND F_N_F BACK TO CLIENT FOR %s***\n",file_name);
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		}

		else {
			printf("\n***SEND GF_OK - FILE LEN %ld BACK TO CLIENT***\n", file_len);
			gfs_sendheader(ctx, GF_OK, file_len);


			while (FILE_REMAINING > 0) {
				while (chunk_recv->read_write_flag == 1) {
					//printf("FLAG IS SET TO %d\n",chunk_recv->read_write_flag);
					msync(chunk_recv, chunk_recv->segment_size, MS_SYNC | MS_INVALIDATE);
					//pthread_cond_wait(&chunk_recv->cond_shm_read, &chunk_recv->m);
					//printf("Try to lock\n");
				}
				if (FILE_REMAINING <= transfer_size) {
					transfer_size = FILE_REMAINING;
				}

				pthread_mutex_lock(&chunk_recv->m);
				chunk_recv->read_write_flag = 0;
				pthread_mutex_unlock(&chunk_recv->m);

				printf("***HANDLER LOCKED***\n");
				char *temp_buffer = (char *) &chunk_recv->data;//,chunk_recv->file_size);
				printf("chunk data: %s\n\n", (char *) &chunk_recv->data);
				printf("handler temp buff: %s\n", temp_buffer);

				write_len = gfs_send(ctx,temp_buffer,transfer_size);
				bytes_transferred += write_len;
				//memcpy(full_buffer + ptr_count, temp_buffer, transfer_size);
				FILE_REMAINING -= transfer_size;
				printf("***FILE REMAINING: %ld***\n", FILE_REMAINING);
				//ptr_count += transfer_size;

				pthread_mutex_lock(&chunk_recv->m);
				chunk_recv->read_write_flag = 1;
				pthread_mutex_unlock(&chunk_recv->m);
				printf("***HANDLER UNLOCKED***\n");

				pthread_cond_broadcast(&chunk_recv->cond_shm_write);
				printf("***CACHE SIGNALED***\n");


			}
		}
			//loop




		printf("***\nSHM DATA is %s\n***",full_buffer);



	//-----------------END SOCKET STUFF-------------------------

	//2.) lock, copy saved by simplecached, unlock



	//3.) send the file from the shared memory desciptor,
	// sending file back to client

	printf("\n***SEND FILE BACK TO CLIENT***\n");
    //if(1==1) {
		//if (0 > (fildes = open((char*)&chunk_recv->data, O_RDONLY))) {
			//if (errno == ENOENT) {
				/* If the file just wasn't found, then send FILE_NOT_FOUND code*/
				//printf("\n***SEND F_N_F BACK TO CLIENT FOR %s***\n",(char*)test_string);
				//return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
			//}
			//else {
			//printf("\n***SERVER ERROR***\n");
			///* Otherwise, it must have been a server error. gfserver library will handle*/
			//return EXIT_FAILURE;
			//}
		//}

		/* Calculating the file size */
		//file_len = lseek(fildes, 0, SEEK_END);
		//printf("\n***FILE DES SAYS FILE IS %ld***\n",file_len);
		//lseek(fildes, 0, SEEK_SET);


		//clean this up


		if(2==1)
		{

		printf("\n***SEND GF_OK - FILE LEN %ld BACK TO CLIENT***\n",file_len);
		gfs_sendheader(ctx, GF_OK, file_len);

		int transf_amt = 1024;
		int cur_ptr = 0;

		printf("\n***BEGIN TRANSFER***\n");
		/* Sending the file contents chunk by chunk. */
		bytes_transferred = 0;
		FILE_REMAINING = file_len;
		//while (bytes_transferred < file_len) {
		while (FILE_REMAINING > 0) {
			if(FILE_REMAINING >= transf_amt) {
				printf("***FILE REMAINING > 1024***");
				//read_len = pread(shm_fd, test_string,transf_amt,0);
				write_len = gfs_send(ctx, full_buffer + cur_ptr, transf_amt);
				read_len = write_len;
				cur_ptr += write_len;
			}
			else{
				printf("***FILE REMAINING < 1024...ONLY %ld***\n",FILE_REMAINING);
				//read_len = pread(shm_fd, test_string,FILE_REMAINING,0);
				write_len = gfs_send(ctx, full_buffer + cur_ptr,FILE_REMAINING);
				read_len = write_len;
				cur_ptr += write_len;
			}

			if (read_len <= 0) {
				fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len);
				return EXIT_FAILURE;
			}

			if (write_len != read_len) {
				fprintf(stderr, "handle_with_file write error");
				return EXIT_FAILURE;
			}
			bytes_transferred += write_len;
			FILE_REMAINING -= write_len;
			printf("***%ld BYTES REMAINING - TOTAL SENT = %ld ***\n",FILE_REMAINING,bytes_transferred );
		}

		}

		//unlink
		//if (shm_unlink(shm_name) == -1) {
		//printf("Error removing %s\n",shm_name);
		//exit(-1);
		//}
		munmap(chunk_recv,chunk_recv->segment_size);
		close(shm_fd);
		close(hSocket);
		free(full_buffer);


		return bytes_transferred;
	//}

	//return 0;
}

