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
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[4096];
	char *data_dir = arg;

	strcpy(buffer,data_dir);
	strcat(buffer,path);

		printf("***STARTING CACHE***\n");
		//we may need to initialize this in web proxy according to instructions

		//a request has come in, as the call back, we need to retrieve a shared memory descriptor

		//1.) communicate request via socket to simplecached, the file name and the file descriptor

		//hard code connection info
		char *server_address = "0.0.0.0";
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


		//LOOK HERE!

		//CREATE SHARED MEM
		// SEND SHARED MEM FD AND FILE NAME

		int file_size = 0;
		char* shm_name = "\\foo1234";
		int shm_fd;
		//heres the file name..
		char *file_name = strrchr(path, '/');
		if (file_name[0] == '/')
			file_name++;


		printf("\n***WRITING %s TO SOCKET***\n",file_name);
		write(hSocket,file_name, 256 * sizeof(char));

		//ask for the file size so we can allocate memory efficiently
		read(hSocket,&file_size,sizeof(file_size));
		printf("\n***RETRIEVED %d BYTES FROM SOCKET***\n",file_size);


		//do all the shared memory stuff here!!

		//create shared mem
		struct shm_data_struct shm_data;

		shm_data.data_size = file_size;
		printf("***shm.data.data_size = %d",file_size);
		//SHOulD WE ADD 1?
		shm_data.data = malloc(file_size);


		//create
		shm_fd = shm_open(shm_name,O_CREAT | O_RDWR,0666);
		if(shm_fd == -1){
			perror("fd_sh:ERROR");
			return -1;
		}
		//set size
		//ftruncate(shm_fd,sizeof(shm_data));
		ftruncate(shm_fd, MAX_FILE_SIZE_BYTES);

		void* shr_ptr;
		//link
		shr_ptr = mmap(0,sizeof(shm_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		if (shr_ptr == MAP_FAILED) {
			printf("Map failed\n");
			return -1;
		}


		//create and init mutex
		//pthread_mutexattr_t(&m_attr);
		//pthread_mutexattr_set_pshared(&m_attr, _POSIX_THREAD_PROCESS_SHARED);
		//pthread_mutex_init(&shm_data.m_shm,&m_attr);

		shr_ptr = &shm_data;

		//int shared_memory_size = sizeof(shm_data);

		//tell simplecache where to write the file
		printf("\n***WRITING (%s) NAME OF SHARED MEMORY TO SOCKET***\n",shm_name);
		write(hSocket,shm_name, 256 * (sizeof(char)));

		//tell simple cache how much memory to allocate for shared mem
		//printf("\n***WRITING %ld SIZE OF SHARED MEMORY TO SOCKET***\n",sizeof(shm_data));
		//write(hSocket,&shared_memory_size,sizeof(int));

		//unlink
		//if (shm_unlink(shm_name) == -1) {
			//printf("Error removing %s\n",shm_name);
			//exit(-1);
		//}

		printf("pointer: %p\n",shr_ptr);

		//free this somewhere

		//free(file_size);



	//-----------------END SOCKET STUFF-------------------------

	//2.) lock, copy saved by simplecached, unlock



	//3.) send the file from the shared memory desciptor,
	// sending file back to client

    if(1==2) {
		if (0 > (fildes = open(buffer, O_RDONLY))) {
			if (errno == ENOENT)
				/* If the file just wasn't found, then send FILE_NOT_FOUND code*/
				return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
			else
				/* Otherwise, it must have been a server error. gfserver library will handle*/
				return EXIT_FAILURE;
		}

		/* Calculating the file size */
		file_len = lseek(fildes, 0, SEEK_END);
		lseek(fildes, 0, SEEK_SET);

	}
		//clean this up
		file_len = file_size;


		gfs_sendheader(ctx, GF_OK, file_len);

		/* Sending the file contents chunk by chunk. */
		bytes_transferred = 0;
		while (bytes_transferred < file_len) {
			read_len = read(shm_fd, buffer, 4096);
			if (read_len <= 0) {
				fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len);
				return EXIT_FAILURE;
			}
			write_len = gfs_send(ctx, buffer, read_len);
			if (write_len != read_len) {
				fprintf(stderr, "handle_with_file write error");
				return EXIT_FAILURE;
			}
			bytes_transferred += write_len;
		}

		return bytes_transferred;
	//}
	//return 0;
}

