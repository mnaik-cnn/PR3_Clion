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

int get_file(CURL* easyhandle,const char* url, const char* file_name);

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg) {




	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;


	char *full_file_path = malloc(512);
	//char *data_dir = arg;
	//char* data_dir = "poop";
	//we need to pass this in...
	char buffer[4096];
	char* data_dir = "s3.amazonaws.com/content.udacity-data.com";

	//full_file_path[0] = '\0';
	printf("\n***DataDirr = %s***\n",data_dir);
	printf("***Path = %s***\n",path);



	if ((data_dir[0] != '\0') & (path[0] != '\0')) {


		strcpy(full_file_path, data_dir);
		strcat(full_file_path, path);



		}
		else {
			printf("Error, directory or path is null. Exiting");
			exit(1);
		}

		printf("***Full File = %s***\n", full_file_path);


		char *file_name = strrchr(path, '/');


		if (file_name[0] == '/')
			file_name++;
		//memmove(file_name, file_name+1, strlen(file_name));


		printf("\nFilename: %s", file_name);

		//CURL and save file here
		//full_file_path - full path
		//path - file name
		CURL *easyhandle = curl_easy_init();

		if (easyhandle) {
			int res = get_file(easyhandle, full_file_path, file_name);
			printf("\nCURL RES: %d\n", res);
		}
		else {
			printf("\nCURL ERROR\n");
		}
		curl_easy_cleanup(easyhandle);


		if (0 > (fildes = open(file_name, O_RDONLY))) {
			if (errno == ENOENT)
				/* If the file just wasn't found, then send FILE_NOT_FOUND code*/
				return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
			else
				/* Otherwise, it must have been a server error. gfserver library will handle*/
				return EXIT_FAILURE;
		}

		/* Calculating the file size */
		file_len = lseek(fildes, 0, SEEK_END);
		printf("\n***Filesize: %ld***\n", file_len);
		lseek(fildes, 0, SEEK_SET);

		printf("\n***Sending header***\n");
		gfs_sendheader(ctx, GF_OK, file_len);

		/* Sending the file contents chunk by chunk. */
		bytes_transferred = 0;
		while (bytes_transferred < file_len) {

			if (file_len < 4096)
				read_len = read(fildes, buffer, file_len);
			else
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

		free(full_file_path);

		return bytes_transferred;

}



int get_file(CURL* easyhandle, const char* url, const char* file_name)
{
	//CURL* easyhandle = curl_easy_init();

	CURLcode res;

	curl_easy_setopt( easyhandle, CURLOPT_URL, url ) ;

	FILE* file = fopen(file_name, "w");

	curl_easy_setopt( easyhandle, CURLOPT_WRITEDATA, file) ;

	res = curl_easy_perform( easyhandle );

	fclose(file);

	return res;

	//curl_easy_cleanup( easyhandle );
}