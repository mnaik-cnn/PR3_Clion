#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

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

	if(1==1) {
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
		//SEND CACHE A FILE NAME AND IT WILL RETURN A FILE DESCRIPTOR
		char* shm_fd = malloc(256*sizeof(char));

		// heres the file name...
		printf("\n***WRITING %s TO SOCKET***\n",path);

		write(hSocket,path, 256 * sizeof(char));
		// thanks mr. cache, for telling me where i can find the file!!!!
		read(hSocket,shm_fd, 256 * (sizeof(char)));

		printf("\n***RETRIEVED %s FROM SOCKET***\n",shm_fd);

		//do all the shared memory stuff here!!




	}
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

		gfs_sendheader(ctx, GF_OK, file_len);

		/* Sending the file contents chunk by chunk. */
		bytes_transferred = 0;
		while (bytes_transferred < file_len) {
			read_len = read(fildes, buffer, 4096);
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
	}
	return 0;
}

