#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SPI_CMD_READ 0
#define SPI_CMD_WRITE 1
#define SPI_CMD_ERASE 2
#define SPI_CMD_PARAM 3

#define SPI_CMD_SIZE 4

struct jz_spi_param {
	unsigned int pagesize;
	unsigned int ppb;
	unsigned int totalblock;
};

struct jz_spi_opsparam {
	unsigned int page;
	unsigned int len;
	void *data;
};

int main(int argc, const char* argv[])
{
	struct jz_spi_param param;
	struct jz_spi_opsparam ops;
	unsigned char *wdata = NULL;
	unsigned char *rdata = NULL;
	int fd = open("/dev/spinor", 0);
	long ret = 0;
	int block = 0;
	int i = 0;

	ret = ioctl(fd, SPI_CMD_PARAM, (unsigned long)(&param));
	if(ret < 0){
		printf("Failed to get flash parameter!\n");
		goto out;
	}
	printf("flash: totalblock = %d, page per block = %d, pagesize = %d\n", param.totalblock, param.ppb, param.pagesize);

#if 1
	wdata = (unsigned char *)malloc(param.pagesize * 2);
	if(wdata == NULL){
		printf("Failed to alloc pagesize!\n");
		goto out;
	}
	rdata = wdata + param.pagesize;

	for(i = 0; i < param.pagesize; i++)
		wdata[i] = i;

	block = param.totalblock - 2;
#if 1
	printf("erase block%d\n", block);
	ret = ioctl(fd, SPI_CMD_ERASE, (unsigned long)block);
	if(ret){
		printf("Failed to erase block%d\n", block);
		goto out;
	}

	ops.page = block * param.ppb;
	ops.len = param.pagesize;
	ops.data = wdata;
	printf("write page %d\n",ops.page);
	ret = ioctl(fd, SPI_CMD_WRITE, (unsigned long)&ops);
	if(ret){
		printf("Failed to write block%d\n", block);
		goto out;
	}
#endif
	ops.page = block * param.ppb;
	ops.len = param.pagesize;
	ops.data = rdata;
	printf("read page %d len = %d\n",ops.page, ops.len);
	ret = ioctl(fd, SPI_CMD_READ, (unsigned long)&ops);
	if(ret){
		printf("Failed to write block%d\n", block);
		goto out;
	}

	for(i = 0; i < param.pagesize; i++){
		if(wdata[i] != rdata[i]){
			printf("wdata[%d] = %d, rdata[%d] = %d\n",i, wdata[i], i, rdata[i]);
			goto out;
		}
		if(i%16 == 0)
			printf("\n");
		printf("0x%02x ", rdata[i]);
	}
	memset(rdata, 0, param.pagesize);
	ops.len = param.pagesize - 7;
	ops.data = rdata;
	printf("read page %d len = %d\n",ops.page, ops.len);
	ret = ioctl(fd, SPI_CMD_READ, (unsigned long)&ops);
	if(ret){
		printf("Failed to write block%d\n", block);
		goto out;
	}
	for(i = 0; i < ops.len; i++){
		if(wdata[i] != rdata[i]){
			printf("wdata[%d] = %d, rdata[%d] = %d\n",i, wdata[i], i, rdata[i]);
			goto out;
		}
		if(i%16 == 0)
			printf("\n");
		printf("0x%02x ", rdata[i]);
	}

	memset(rdata, 0, param.pagesize);
	ops.len = param.pagesize - 1;
	ops.data = rdata;
	printf("read page %d len = %d\n",ops.page, ops.len);
	ret = ioctl(fd, SPI_CMD_READ, (unsigned long)&ops);
	if(ret){
		printf("Failed to write block%d\n", block);
		goto out;
	}
	for(i = 0; i < ops.len; i++){
		if(wdata[i] != rdata[i]){
			printf("wdata[%d] = %d, rdata[%d] = %d\n",i, wdata[i], i, rdata[i]);
			goto out;
		}
		if(i%16 == 0)
			printf("\n");
		printf("0x%02x ", rdata[i]);
	}

	memset(rdata, 0, param.pagesize);
	ops.len = param.pagesize - 15;
	ops.data = rdata;
	printf("read page %d len = %d\n",ops.page, ops.len);
	ret = ioctl(fd, SPI_CMD_READ, (unsigned long)&ops);
	if(ret){
		printf("Failed to write block%d\n", block);
		goto out;
	}
	for(i = 0; i < ops.len; i++){
		if(wdata[i] != rdata[i]){
			printf("wdata[%d] = %d, rdata[%d] = %d\n",i, wdata[i], i, rdata[i]);
			goto out;
		}
		if(i%16 == 0)
			printf("\n");
		printf("0x%02x ", rdata[i]);
	}
	printf("********** test success **************\n");
#endif
out:
	if(wdata)
		free(wdata);
	close(fd);
	return 0;
}
