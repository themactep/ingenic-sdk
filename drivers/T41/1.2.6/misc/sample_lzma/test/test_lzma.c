#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>


struct lzma_conf {
	char src0_name[64];
	char dst0_name[64];
	char src1_name[64];
	char dst1_name[64];
	unsigned int src0_size;
	unsigned int dst0_size;
	unsigned int src1_size;
	unsigned int dst1_size;
};

#define JZLZMA_IOC_MAGIC  'L'
#define IOCTL_LZMA0_START		_IOW(JZLZMA_IOC_MAGIC, 110, struct lzma_conf*)
#define IOCTL_LZMA1_START		_IOW(JZLZMA_IOC_MAGIC, 111, struct lzma_conf*)
#define IOCTL_LZMAA_START		_IOW(JZLZMA_IOC_MAGIC, 112, struct lzma_conf*)

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("----- Usage: ./test_lzma lzma-PATH\n");
		return -1;
	}
	int fd = open("/dev/jz_lzma", O_RDWR);
	if (fd < 0)
	{
		printf("---- file open failed\n");
		return -1;
	}

	//ioctl
	struct stat sta;
	struct lzma_conf lzma_conf;
	stat(argv[1], &sta);

	strncpy(lzma_conf.src0_name, "kernel.7z.lzma", 20);
	strncpy(lzma_conf.dst0_name, "decompress_uImage", 20);
	lzma_conf.dst0_size = 32*1024*1024;
	lzma_conf.src0_size = sta.st_size;

	ioctl(fd, IOCTL_LZMA0_START, &lzma_conf);

	close(fd);
	return 0;
}
