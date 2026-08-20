#include "linux_net.h"
#include "tun.h"

// Allocate a TUN device (dev) and return the file descriptor
int tun_alloc(char *dev)
{
   struct ifreq ifr;
   int fd, err;

   if ((fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0)
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
