#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
 #include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct ssi_slv_param {
	uint8_t *tx;
	uint8_t *rx;

	uint8_t istx_dma;
	uint8_t isrx_dma;
	uint32_t tx_len;
	uint32_t rx_len;

	void *tx_vaddr;
	void *rx_vaddr;
	uint32_t tx_paddr;
	uint32_t rx_paddr;
};

#define SSISLV_IOC_MAGIC  'S'
#define IOCTL_SSISLV_READ	_IOWR(SSISLV_IOC_MAGIC,0,struct ssi_slv_param)
#define IOCTL_SSISLV_WRITE	_IOWR(SSISLV_IOC_MAGIC,1,struct ssi_slv_param)
#define IOCTL_SSISLV_DMA_READ	_IOWR(SSISLV_IOC_MAGIC,2,struct ssi_slv_param)
#define IOCTL_SSISLV_DMA_WRITE	_IOWR(SSISLV_IOC_MAGIC,3,struct ssi_slv_param)

#define SSISLV_DMA 1
int main(int argc, char *argv[])
{
	int fd,ret = 0,i = 0;
	struct ssi_slv_param param = {0};
	param.rx_len = 32*4;
	param.rx = malloc(param.rx_len);
	if(!param.rx){
		printf("rx is null\n");
		return -1;
	}

	fd = open("/dev/jz_spi_slave",O_RDWR | O_NONBLOCK);
	if(fd < 0){
		printf("open err\n");
	}

	// ret = ioctl(fd,IOCTL_SSISLV_READ,&param);
	// if(ret < 0){
	// 	printf("ioctl err:%s,ret:%d\n",strerror(ret),ret);
	// 	return -1;
	// }
#if SSISLV_DMA
	param.rx_len = 32*4;
	param.rx_vaddr = (char *)malloc(param.rx_len);
	if(!param.rx_vaddr){
		printf("tx_vaddr is null\n");
		return -1;
	}
	ret = ioctl(fd,IOCTL_SSISLV_DMA_READ,&param);
	if(ret < 0){
		printf("ioctl err:%s,ret:%d\n",strerror(ret),ret);
		return -1;
	}
#endif
#if 0
	param.tx_len = 32;
	param.tx_vaddr = (char *)malloc(param.tx_len);
	if(!param.tx_vaddr){
		printf("tx_vaddr is null\n");
		return -1;
	}
	for(i = 0;i < param.tx_len ;i++){
		((char*)param.tx_vaddr)[i] = 0x5a;
	}
	ret = ioctl(fd,IOCTL_SSISLV_DMA_WRITE,&param);
	if(ret < 0){
		printf("ioctl err:%s,ret:%d\n",strerror(ret),ret);
		return -1;
	}
#endif

#if 0
	param.rx_len = 32;
	param.rx = malloc(param.rx_len);
	if(!param.rx){
		printf("rx is null\n");
		return -1;
	}

	ret = ioctl(fd,IOCTL_SSISLV_READ,&param);
	if(ret < 0){
		printf("ioctl err:%s,ret:%d\n",strerror(ret),ret);
		return -1;
	}

	for(i = 0;i < param.rx_len ;i++){
		printf("param.rx[%d]=%#x\n",i,param.rx[i]);
	}
#endif

#if 0
	param.tx_len = 32;
	param.tx = malloc(param.tx_len);
	if(!param.rx){
		printf("tx is null\n");
		return -1;
	}
	for(i = 0;i < param.tx_len ;i++){
		param.tx[i] = i;
	}
	ret = ioctl(fd,IOCTL_SSISLV_WRITE,&param);
	if(ret < 0){
		printf("ioctl err:%s,ret:%d\n",strerror(ret),ret);
		return -1;
	}
#endif

	return 0;
}

