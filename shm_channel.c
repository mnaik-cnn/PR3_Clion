//In case you want to implement the shared memory IPC as a library...

#include "shm_channel.h"

//---------------------------------------SOCKET HELPERS-----------------------------------------------
//---SOCKET HELPER-----

void FillAddress(struct sockaddr_in *Address,int nHostPort) {
    //FILL ADDRESS STRUCTURE
    Address->sin_addr.s_addr=INADDR_ANY;

    Address->sin_port=htons(nHostPort);
    //hook onto port passed by parameter
    Address->sin_family=AF_INET;
    //ipv4
}

int CreateServerSocket(int *hServerSocket){

    *hServerSocket = socket(AF_INET,SOCK_STREAM,0);

    if(*hServerSocket == SOCKET_ERROR)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

