
//In case you want to implement the shared memory IPC as a library...

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#define FILE_STORAGE_BUFFER_SIZE 15000000
#define SOCKET_ERROR -1
#define FILE_STORAGE_BUFFER_SIZE 15000000
#define FILE_TRANSFER_BUFFER_SIZE 2048
#define QUEUE_SIZE 1024
#define HOST_NAME_SIZE      255
#define WORKER_QUEUE_SIZE 50000



struct sockaddr_in Address;
int nAddressSize;
int nHostPort;


struct cache_workload{
    char* shared_memory_name;
    char* requested_file_name;
};

void FillAddress(struct sockaddr_in *Address,int nHostPort);
int CreateServerSocket(int *hServerSocket);
