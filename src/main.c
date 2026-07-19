#include "linux_net.h"
#include "tun.h"

int main(int argc, char *argv[])
{
   char tun_name[IFNAMSIZ] = "tun0";
   int fd = tun_alloc(tun_name);
   
   if (fd < 0) return 1;

   printf("Successfully opened %s (FD: %d). Listening for packets...\n", tun_name, fd);

   unsigned char buffer[2048]; 
   
   while(1) {
      int nread = read(fd, buffer, sizeof(buffer));
      
      if (nread < 0) {
         perror("Read error");
         break;
      }

      printf("\nReceived Packet (%d bytes)\n", nread);

      // Print the bytes in Hex format
      for (int i = 0; i < nread; i++) {
         printf("%02x ", buffer[i]);
         
         // Add a newline every 16 bytes for readability
         if ((i + 1) % 16 == 0) printf("\n");
      }
      printf("\n");
   }
   
   close(fd);
   return 0;
}