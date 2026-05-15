#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define JZDTRNG_IOC_MAGIC  'D'
#define IOCTL_DTRNG_DMA_GET_RANDOM					_IO(JZDTRNG_IOC_MAGIC, 110)
#define IOCTL_DTRNG_CPU_GET_RANDOM					_IO(JZDTRNG_IOC_MAGIC, 111)

int main(int argc, const char* argv[])
{
	int ret = -1;
	int fd = open("/dev/dtrng", 0);
	if(fd < 0){
		printf("Failed to open /dev/dtrng\n");
		return 0;
	}
	unsigned int random = 0;
#if 0
	ret = ioctl(fd, IOCTL_DTRNG_CPU_GET_RANDOM, &random);
#else
	ret = ioctl(fd, IOCTL_DTRNG_DMA_GET_RANDOM, &random);
#endif
	if (ret < 0){
		printf("Failed to get random number\n");
		return 0;
	}
	printf("random = %d\n", random);
	close(fd);
	return 0;
}
