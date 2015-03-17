
//In case you want to implement the shared memory IPC as a library...

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define FILE_STORAGE_BUFFER_SIZE 15000000
#define SOCKET_ERROR -1
#define FILE_STORAGE_BUFFER_SIZE 15000000
#define FILE_TRANSFER_BUFFER_SIZE 2048
#define QUEUE_SIZE 1024
#define HOST_NAME_SIZE      255
#define WORKER_QUEUE_SIZE 50000


#define MAX_FILE_SIZE_BYTES 10485760



//***********SHARED MEM************

struct shm_information {
    char* cache_server_addr;
    int segment_size;
    char* segment_name;
};


struct shm_data_struct{
    pthread_mutex_t m;
    pthread_cond_t  cond_shm_read;
    pthread_cond_t  cond_shm_write;

    char* data;
    //buffer should size should equal segment size - structure size
    ssize_t segment_size;
    ssize_t buffer_size;
    ssize_t file_size;
};



//***********SOCKETS****************

struct sockaddr_in Address;
int nAddressSize;
int nHostPort;

void FillAddress(struct sockaddr_in *Address,int nHostPort);
int CreateServerSocket(int *hServerSocket);
