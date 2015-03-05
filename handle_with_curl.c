#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "gfserver.h"

//Replace with an implementation of handle_with_curl and any other
//functions you may need.

void get_file(const char* url, const char* file_name);

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[4096];
	//char *data_dir = arg;

	//we need to pass this in...
	char* data_dir = "s3.amazonaws.com/content.udacity-data.com";


	strcpy(buffer,data_dir);
	strcat(buffer,path);

	printf("\n***DataDir = %s***\n",data_dir);
	printf("***Path = %s***\n",path);

	char* file_name = strrchr(path, '/');
	if (file_name[0] == '/')
		memmove(file_name, file_name+1, strlen(file_name));


	printf("\nFilename: %s",file_name);

	//CURL and save file here
	//buffer - full path
	//path - file name
	get_file(buffer,file_name);

	if( 0 > (fildes = open(file_name, O_RDONLY))){
		if (errno == ENOENT)
			/* If the file just wasn't found, then send FILE_NOT_FOUND code*/ 
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		else
			/* Otherwise, it must have been a server error. gfserver library will handle*/ 
			return EXIT_FAILURE;
	}

	/* Calculating the file size */
	file_len = lseek(fildes, 0, SEEK_END);
	printf("\n***Filesize: %ld***\n",file_len);
	lseek(fildes, 0, SEEK_SET);

	printf("\n***Sending header***\n");
	gfs_sendheader(ctx, GF_OK, file_len);

	/* Sending the file contents chunk by chunk. */
	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = read(fildes, file_name, 4096);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			return EXIT_FAILURE;
		}
		write_len = gfs_send(ctx, file_name, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			return EXIT_FAILURE;
		}
		bytes_transferred += write_len;
	}

	return bytes_transferred;
}



void get_file(const char* url, const char* file_name)
{
	CURL* easyhandle = curl_easy_init();

	curl_easy_setopt( easyhandle, CURLOPT_URL, url ) ;

	FILE* file = fopen(file_name, "w");

	curl_easy_setopt( easyhandle, CURLOPT_WRITEDATA, file) ;

	curl_easy_perform( easyhandle );

	curl_easy_cleanup( easyhandle );

	fclose(file);
}