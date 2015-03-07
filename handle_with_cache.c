#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

#include "simplecache.h"


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


	//we may need to initialize this in web proxy according to instructions

	//1.) a request has come in, as the call back, we have retrieved a shared memory descriptor
	//2.) communicate request via socket to simplecached, the file name and the file descriptor
	//3.) lock, copy saved by simplecached, unlock
	//4.) send the file from the shared memory desciptor,






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

