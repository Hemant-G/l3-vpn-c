#include "linux_net.h"
#include "tun.h"
#include "socket.h"

#define BUF_SIZE 212992

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
    //int inet_pton_val = inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr); // localhost for testing
    int inet_pton_val = inet_pton(AF_INET, "10.1.1.20", &server_addr.sin_addr); // localhost for testing

    if (inet_pton_val <= 0) {
        if (inet_pton_val == 0)
            fprintf(stderr, "Not in presentation format");
        else
            perror("inet_pton");
        return 1;
    }
    int udp_fd = create_udp_socket();
    if (udp_fd < 0) return 1;
    
    // Connect the UDP socket to the server address
    if (connect(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(udp_fd);
        close(fd);
        return 1;
    }

     // Set up the epoll instance
    struct epoll_event events[10];

    struct epoll_event ev;

    int epoll_fd =  epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
    } 

    ev.events = EPOLLIN;
    ev.data.fd = udp_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_fd, &ev);

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD,fd, &ev);
   
    
    while(1) {
        int n_ready_fds = epoll_wait(epoll_fd, events, 10, -1);

        for(int i=0; i<n_ready_fds; i++){
            int ready_fd = events[i].data.fd;

            if(ready_fd == fd && (events[i].events & EPOLLIN)){
                // Read from the TUN device
                ssize_t nread = read(ready_fd, buffer, sizeof(buffer));
        
                if (nread < 0) {
                    perror("Read error");
                    break;
                }

                printf("\nReceived Packet (%zd bytes)\n", nread);

                // Send the packets over the UDP socket
                send(udp_fd, buffer, nread, 0);
                
            }

            if(ready_fd == udp_fd){
                // Receive packets from UDP socket
                ssize_t nread = recvfrom(ready_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &(socklen_t){sizeof(server_addr)});
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

                // Write the received packet to the TUN device
                ssize_t nwritten = write(fd, buffer, nread);
                if (nwritten < 0) {
                    perror("write to tun0 failed");
                } else {
                    printf("Wrote %zd bytes to tun0\n", nwritten);
                }
            }
        }
    }

    close(fd);
    return 0;
}