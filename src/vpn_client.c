#include <stdio.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int tun_alloc(char *dev)
{
   struct ifreq ifr;
   int fd, err;

   if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
   {
      perror("Opening /dev/net/tun");
      printf("Errorno: %d", errno);
      return -2;
   }

   memset(&ifr, 0, sizeof(ifr));

   /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
    *        IFF_TAP   - TAP device
    *
    *        IFF_NO_PI - Do not provide packet information
    */
   ifr.ifr_flags = IFF_TUN;
   if (*dev)
      strncpy(ifr.ifr_name, dev, IFNAMSIZ);

   if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
   {
      close(fd);
      perror("ioctl()");
      printf("Errorno: %d", errno);
      return err;
   }
   strcpy(dev, ifr.ifr_name);
   return fd;
}

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