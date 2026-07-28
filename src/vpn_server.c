#include "linux_net.h"
#include "tun.h"
#include "socket.h"

#define BUF_SIZE 212992

int main(int argc, char *argv[]){

    // Allocate a TUN device
    char tun_name[IFNAMSIZ] = "tun1";
    int fd = tun_alloc(tun_name);
    unsigned char buffer[2048]; 

    if (fd < 0) return 1;

    printf("Successfully opened %s (FD: %d). Listening for packets...\n", tun_name, fd);

    // Sender address structure to hold the address of the sender
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    // Create a UDP socket and bind to a part and IP address to listen for incoming packets
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9999); 

    int inet_pton_val = inet_pton(AF_INET, "0.0.0.0", &server_addr.sin_addr);
    if (inet_pton_val <= 0) {
        if (inet_pton_val == 0)
            fprintf(stderr, "Not in presentation format");
        else
            perror("inet_pton");
        return 1;
    }

    int udp_fd = create_udp_socket();
    if (udp_fd < 0) return 1;

    if (bind(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(udp_fd);
        close(fd);
        return 1;
    }


    // Receive packets from the UDP socket and print them in hex format 
    while(1) {

        // Receive packets from UDP socket
        ssize_t nread = recvfrom(udp_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender_addr, &addr_len);

        if (nread < 0) {
            perror("recvfrom error");
            break;
        }

        // Print all received bytes in hex
        printf("Buffer (%zd bytes): ", nread);
        for (int i = 0; i < nread; i++) {
            printf("%02x ", buffer[i]);
        }
        printf("\n");
    }

    close(fd);
    return 0;
}