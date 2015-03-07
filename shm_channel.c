//In case you want to implement the shared memory IPC as a library...




#include <shm_channel.h>



//---------------------------------------SOCKET HELPERS-----------------------------------------------
//---SOCKET HELPER-----

void FillAddress(struct sockaddr_in *Address,int nHostPort) {
    //FILL ADDRESS STRUCTURE
    printf("\ncreating address...\n");
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


int getServerSocket() {
    struct sockaddr_in Address;
    nAddressSize = sizeof(struct sockaddr_in);
    nHostPort = 8887;

    //-------MAKE A SERVER SOCKET------------
    int hSocket;
    int returnCode = CreateServerSocket(&hServerSocket);
    if (returnCode == 0) {
        printf("\nCould not make a socket\n");
        return 0;
    }

    //----------FILL ADDRESS DETAILS------------------
    FillAddress(&Address, nHostPort);
    //----------BIND TO SOCKET TO PORT
    printf("\nBinding to port %d", nHostPort);
    if (bind(hServerSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR) {
        printf("\nCould not connect to host\n");
        return 0;
    }
    //-----------GET PORT NUMBER-------------------
    getsockname(hServerSocket, (struct sockaddr *) &Address, (socklen_t *) &nAddressSize);
    //printf("\nOpened socket as fd (%d) on port (%d) for stream i/o\n", hServerSocket, ntohs(Address.sin_port));

    //printf("\nMaking a listen queue of %d elements", QUEUE_SIZE);
    ///* establish listen queue */

    return hSocket;
}

