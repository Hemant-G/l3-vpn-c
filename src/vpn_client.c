#include "linux_net.h"
#include "tun.h"
#include "socket.h"

#define BUF_SIZE 2048

// Libsodium 
#include <sodium.h>
#define ADDITIONAL_DATA (const unsigned char *) "123456"
#define ADDITIONAL_DATA_LEN 6

// Key exchange 
unsigned char client_pk[crypto_kx_PUBLICKEYBYTES], client_sk[crypto_kx_SECRETKEYBYTES];
unsigned char client_rx[crypto_kx_SESSIONKEYBYTES], client_tx[crypto_kx_SESSIONKEYBYTES];

unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];

//Nonce, key, and ciphertext buffers
unsigned char nonce[crypto_aead_chacha20poly1305_IETF_NPUBBYTES];
unsigned char ciphertext[BUF_SIZE + crypto_aead_chacha20poly1305_IETF_ABYTES];
unsigned long long ciphertext_len;
unsigned char decrypted[BUF_SIZE];
unsigned long long decrypted_len;

int main(int argc, char *argv[]){
    // Initialize libsodium
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return 1;
    }
    
    // Load keys from config files
    if (load_key("config/server_public.key", server_pk, crypto_kx_PUBLICKEYBYTES) != 0) {
        fprintf(stderr, "Failed to load server public key\n");
        return 1;
    }

    if (load_key("config/client_private.key", client_sk, crypto_kx_SECRETKEYBYTES) != 0) {
        fprintf(stderr, "Failed to load client private key\n");
        return 1;
    }

    if (load_key("config/client_public.key", client_pk, crypto_kx_PUBLICKEYBYTES) != 0) {
        fprintf(stderr, "Failed to load client public key\n");
        return 1;
    }

    // Key exchange 
    if (crypto_kx_client_session_keys(client_rx, client_tx, 
                    client_pk, client_sk, server_pk) != 0) {
        fprintf(stderr, "Failed to derive session keys\n");
        return 1;
    }

    // Allocate a TUN device
    char tun_name[IFNAMSIZ] = "tun0";
    int fd = tun_alloc(tun_name);
    unsigned char buffer[BUF_SIZE]; 

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

                // Encrypt the packet using libsodium
                randombytes_buf(nonce, sizeof nonce);

                if (crypto_aead_chacha20poly1305_ietf_encrypt(ciphertext, &ciphertext_len,
                                                        buffer, nread,
                                                        NULL, 0,
                                                        NULL, nonce, client_tx) != 0) {
                    fprintf(stderr, "Encryption failed\n");
                    continue;
                }

                // Add the nonce to the ciphertext for sending
                unsigned char packet[sizeof(nonce) + sizeof(ciphertext)];

                memcpy(packet, nonce, sizeof(nonce));
                memcpy(packet + sizeof(nonce), ciphertext, ciphertext_len);

                size_t packet_len = sizeof(nonce) + ciphertext_len;

                // Send the encrypted message + nounce packets over the UDP socket
                send(udp_fd, packet, packet_len, 0);
                
            }

            if(ready_fd == udp_fd){
                // Receive packets from UDP socket
                ssize_t nread = recv(ready_fd, buffer, sizeof(buffer), 0);
                if (nread < 0) {
                    perror("recvf error");
                    break;
                }

                // Print all received bytes in hex
                // printf("Buffer (%zd bytes): ", nread);
                // for (int i = 0; i < nread; i++) {
                //     printf("%02x ", buffer[i]);
                // }
                // printf("\n");

                // Extract the nonce from the received packet
                memcpy(nonce, buffer, sizeof(nonce));
                unsigned char *received_ciphertext = buffer + sizeof(nonce);
                size_t received_ciphertext_len = nread - sizeof(nonce);

                if (crypto_aead_chacha20poly1305_ietf_decrypt(decrypted, &decrypted_len,
                                              NULL,
                                              received_ciphertext, received_ciphertext_len,
                                              NULL, 0,
                                              nonce, client_rx) != 0) {
                    fprintf(stderr, "Decryption failed\n");
                    continue;
                }

                // // Write the packet to the TUN device
                ssize_t nwritten = write(fd, decrypted, decrypted_len);
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