
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

struct shm_data_struct{
    pthread_mutex_t m_shm;
    char* data;
    ssize_t data_size;
};



//***********SOCKETS****************

struct sockaddr_in Address;
int nAddressSize;
int nHostPort;

void FillAddress(struct sockaddr_in *Address,int nHostPort);
int CreateServerSocket(int *hServerSocket);
