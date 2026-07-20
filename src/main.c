#include "linux_net.h"
#include "tun.h"
#include "socket.h"

#define BUF_SIZE 500

int main(int argc, char *argv[]){

    // Allocate a TUN device
    char tun_name[IFNAMSIZ] = "tun0";
    int fd = tun_alloc(tun_name);
    unsigned char buffer[2048]; 

    if (fd < 0) return 1;

    printf("Successfully opened %s (FD: %d). Listening for packets...\n", tun_name, fd);

    // Create a UDP socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9999); // Example port
    int inet_pton_val = inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr); // localhost for testing

    if (inet_pton_val <= 0) {
        if (inet_pton_val == 0)
            fprintf(stderr, "Not in presentation format");
        else
            perror("inet_pton");
        return 1;
        }

    // Connect the UDP socket to the server address
    int udp_fd = create_udp_socket();
    if (udp_fd < 0) return 1;
    if (connect(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(udp_fd);
        close(fd);
        return 1;
    }
   

    // Continuously read packets from the TUN device and send over the UDP socket  
    while(1) {
        int nread = read(fd, buffer, sizeof(buffer));
        
        if (nread < 0) {
            perror("Read error");
            break;
        }

        printf("\nReceived Packet (%d bytes)\n", nread);
        send(udp_fd, buffer, sizeof(buffer), 0);
    }

    close(fd);
    return 0;
}