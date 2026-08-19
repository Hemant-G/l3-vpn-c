#include "linux_net.h"
#include "socket.h"

// Create a UDP socket 
int create_udp_socket(){
    int socket_fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if(socket_fd < 0){
        perror("Socket creation failed");
        return -1;
    }

    return socket_fd;
}
