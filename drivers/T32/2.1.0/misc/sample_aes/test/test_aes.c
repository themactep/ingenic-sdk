#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include "test_aes.h"

#undef  GETCBCOUTFILE
#undef USE_MAP_MODE
#define AES_KEY_SIZE	(128)	// AES-128
#define DMABUFSIZE	(16*1024)
// 全局测试数据（确保两个接口使用相同数据）
#define TEST_DATA_LEN (16*1024)
static unsigned char g_key[AES_KEY_SIZE / 8];
static unsigned char g_iv[16];
// static unsigned char g_original_data[TEST_DATA_LEN] __attribute__((aligned(4096)));

#define GETCHIPID (0x1300002c)
typedef enum {
	SOC_START = 0,
	SOC_T32,
	SOC_T33,
	SOC_T33_END,
} CPUID;

int g_fd;

#define HISTORY_SIZE 30

// 统计数据结构体
typedef struct {
	unsigned long call_count;      // 调用次数
	unsigned long total_data;     // 总数据量
	unsigned long history[HISTORY_SIZE]; // 最近数据历史
	unsigned long time_history[HISTORY_SIZE];//最近数据处理时间
	unsigned long memcpy_history[HISTORY_SIZE];//最近数据处理时间
	unsigned long hwaes_history[HISTORY_SIZE];//最近数据处理时间
	int history_index;            // 当前历史记录位置
	time_t start_time;
	pthread_mutex_t lock;         // 互斥锁
	int inited;                   // 是否初始化
} DebugStats;

static DebugStats gstats_enc = {
	.call_count = 0,
	.total_data = 0,
	.history_index = 0,
	.start_time = 0,
	.lock = PTHREAD_MUTEX_INITIALIZER
};

static DebugStats gstats_dec = {
	.call_count = 0,
	.total_data = 0,
	.history_index = 0,
	.start_time = 0,
	.lock = PTHREAD_MUTEX_INITIALIZER
};

// 初始化历史数据为0
void init_history(DebugStats *stats ) {
	int i;
	pthread_mutex_lock(&stats->lock);
	stats->start_time = time(NULL);
	for(i = 0; i < HISTORY_SIZE; i++){
		stats->history[i] = 0;
		stats->memcpy_history[i] = 0;
	}
	pthread_mutex_unlock(&stats->lock);
}

void log_data_enc(unsigned long data_size, long int deltime, long int memcpytime, long int hwaestime, char *name)
{
	int i;
	static struct timeval ttemp, end;
	DebugStats *stats = &gstats_enc;

	if (!stats->inited) {
		init_history(stats);
		stats->inited = 1;
		gettimeofday(&ttemp, NULL);
	}

	pthread_mutex_lock(&stats->lock);

	stats->call_count++;
	stats->total_data += data_size;

	stats->history[stats->history_index] = data_size;
	stats->time_history[stats->history_index] = deltime;
	stats->memcpy_history[stats->history_index] = memcpytime;
	stats->hwaes_history[stats->history_index] = hwaestime;
	stats->history_index = (stats->history_index + 1) % HISTORY_SIZE;

	pthread_mutex_unlock(&stats->lock);

	// 每2秒打印一次统计信息
	gettimeofday(&end, NULL);
	if (end.tv_sec - ttemp.tv_sec > 2) {
		time_t now = time(NULL);
		time_t total_time = now - stats->start_time;
		double data_rate = (total_time > 0) ? (double)stats->total_data / total_time : 0.0;

		uint32_t valid_count = (stats->call_count < HISTORY_SIZE) ? stats->call_count : HISTORY_SIZE;

		uint64_t total_enc = 0, avg_enc = 0;
		uint64_t min_enc = ~(uint64_t)0;
		uint64_t max_enc = 0;
		for (i = 0; i < valid_count; i++) {
			uint64_t v = stats->time_history[i];
			total_enc += v;
			if (v < min_enc) min_enc = v;
			if (v > max_enc) max_enc = v;
		}
		avg_enc = valid_count > 0 ? total_enc / valid_count : 0;

		uint64_t total_memcpy = 0, avg_memcpy = 0;
		uint64_t min_memcpy = ~(uint64_t)0;
		uint64_t max_memcpy = 0;
		for (i = 0; i < valid_count; i++) {
			uint64_t v = stats->memcpy_history[i];
			total_memcpy += v;
			if (v < min_memcpy) min_memcpy = v;
			if (v > max_memcpy) max_memcpy = v;
		}
		avg_memcpy = valid_count > 0 ? total_memcpy / valid_count : 0;

		uint64_t total_hwaes = 0, avg_hwaes = 0;
		uint64_t min_hwaes = ~(uint64_t)0;
		uint64_t max_hwaes = 0;
		for (i = 0; i < valid_count; i++) {
			uint64_t v = stats->hwaes_history[i];
			total_hwaes += v;
			if (v < min_hwaes) min_hwaes = v;
			if (v > max_hwaes) max_hwaes = v;
		}
		avg_hwaes = valid_count > 0 ? total_hwaes / valid_count : 0;

		printf("hw aes %s totaldata:%ld, rate:%f, times:%lu\n",
			name, stats->total_data, data_rate, stats->call_count);
		printf("[enc    :total:%llu (min~max/avg)=( %llu~%llu / %llu )]\n",
			total_enc, min_enc, max_enc, avg_enc);
		printf("[memcpy :total:%llu (min~max/avg)=( %llu~%llu / %llu )]\n",
			total_memcpy, min_memcpy, max_memcpy, avg_memcpy);
		printf("[hwaes  :total:%llu (min~max/avg)=( %llu~%llu / %llu )]\n",
			total_hwaes, min_hwaes, max_hwaes, avg_hwaes);

		// 打印历史数据（保持原有格式）
		// printf("data_history(byte):");
		// for (i = 0; i < HISTORY_SIZE; i++) {
		// 	printf(":%10lu", stats->history[i]);
		// }
		// printf("\n");
		// printf("time_history( us ):");
		// for (i = 0; i < HISTORY_SIZE; i++) {
		// 	printf(":%10lu", stats->time_history[i]);
		// }
		// printf("\n");
		// printf("mmcpy_history( us ):");
		// for (i = 0; i < HISTORY_SIZE; i++) {
		// 	printf(":%10lu", stats->memcpy_history[i]);
		// }
		// printf("\n");
		// printf("hwaes_history( us ):");
		// for (i = 0; i < HISTORY_SIZE; i++) {
		// 	printf(":%10lu", stats->hwaes_history[i]);
		// }
		printf("\n\n");

		ttemp = end; // 更新计时器
	}
}

void log_data_dec(unsigned long data_size, long int deltime, long int memcpytime, long int hwaestime, char *name)
{
	int i;
	static struct timeval ttemp, end;
	DebugStats *stats = &gstats_dec;

	if (!stats->inited) {
		init_history(stats);
		stats->inited = 1;
		gettimeofday(&ttemp, NULL);
	}

	pthread_mutex_lock(&stats->lock);

	stats->call_count++;
	stats->total_data += data_size;

	stats->history[stats->history_index] = data_size;
	stats->time_history[stats->history_index] = deltime;
	stats->memcpy_history[stats->history_index] = memcpytime;
	stats->hwaes_history[stats->history_index] = hwaestime;
	stats->history_index = (stats->history_index + 1) % HISTORY_SIZE;

	pthread_mutex_unlock(&stats->lock);


	gettimeofday(&end, NULL);
	if (end.tv_sec - ttemp.tv_sec > 2) {
		time_t now = time(NULL);
		time_t total_time = now - stats->start_time;
		double data_rate = (total_time > 0) ? (double)stats->total_data / total_time : 0.0;


		uint32_t valid_count = (stats->call_count < HISTORY_SIZE) ? stats->call_count : HISTORY_SIZE;

		uint64_t total_enc = 0, avg_enc = 0;
		uint64_t min_enc = ~(uint64_t)0;
		uint64_t max_enc = 0;
		for (i = 0; i < valid_count; i++) {
			uint64_t v = stats->time_history[i];
			total_enc += v;
			if (v < min_enc) min_enc = v;
			if (v > max_enc) max_enc = v;
		}
		avg_enc = valid_count > 0 ? total_enc / valid_count : 0;

		uint64_t total_memcpy = 0, avg_memcpy = 0;
		uint64_t min_memcpy = ~(uint64_t)0;
		uint64_t max_memcpy = 0;
		for (i = 0; i < valid_count; i++) {
			uint64_t v = stats->memcpy_history[i];
			total_memcpy += v;
			if (v < min_memcpy) min_memcpy = v;
			if (v > max_memcpy) max_memcpy = v;
		}
		avg_memcpy = valid_count > 0 ? total_memcpy / valid_count : 0;

		uint64_t total_hwaes = 0, avg_hwaes = 0;
		uint64_t min_hwaes = ~(uint64_t)0;
		uint64_t max_hwaes = 0;
		for (i = 0; i < valid_count; i++) {
			uint64_t v = stats->hwaes_history[i];
			total_hwaes += v;
			if (v < min_hwaes) min_hwaes = v;
			if (v > max_hwaes) max_hwaes = v;
		}
		avg_hwaes = valid_count > 0 ? total_hwaes / valid_count : 0;

		printf("hw aes %s totaldata:%ld, rate:%f, times:%lu\n",
			name, stats->total_data, data_rate, stats->call_count);
		printf("[enc    :total:%llu (min~max/avg)=( %llu~%llu / %llu )]\n",
			total_enc, min_enc, max_enc, avg_enc);
		printf("[memcpy :total:%llu (min~max/avg)=( %llu~%llu / %llu )]\n",
			total_memcpy, min_memcpy, max_memcpy, avg_memcpy);
		printf("[hwaes  :total:%llu (min~max/avg)=( %llu~%llu / %llu )]\n",
			total_hwaes, min_hwaes, max_hwaes, avg_hwaes);

		// 打印历史数据（保持原有格式）
		// printf("data_history(byte):");
		// for (i = 0; i < HISTORY_SIZE; i++) {
		// 	printf(":%10lu", stats->history[i]);
		// }
		// printf("\n");
		// printf("time_history( us ):");
		// for (i = 0; i < HISTORY_SIZE; i++) {
		// 	printf(":%10lu", stats->time_history[i]);
		// }
		// printf("\n");
		// printf("mmcpy_history( us ):");
		// for (i = 0; i < HISTORY_SIZE; i++) {
		// 	printf(":%10lu", stats->memcpy_history[i]);
		// }
		// printf("\n");
		// printf("hwaes_history( us ):");
		// for (i = 0; i < HISTORY_SIZE; i++) {
		// 	printf(":%10lu", stats->hwaes_history[i]);
		// }
		printf("\n\n");

		ttemp = end; // 更新计时器
	}
}

// 生成随机数据
void generate_random_data(unsigned char *data, size_t len) {
	size_t i;
	for (i = 0; i < len; i++) {
		data[i] = (unsigned char)(rand() % 256);
	}
}

void print_hex(const char *label, const unsigned char *data, size_t len) {
	printf("%s: ", label);
	for (size_t i = 0; i < len; i++) {
		printf("%02X", data[i]);
	}
	printf("\n");
}

static void write_as_be(uint8_t *output, uint32_t val) {
	output[3] = (val >> 0)  & 0xFF;
	output[2] = (val >> 8)  & 0xFF;
	output[1] = (val >> 16) & 0xFF;
	output[0] = (val >> 24) & 0xFF;
}

static void convert_to_manual(uint8_t *output, size_t len, const uint32_t *src, size_t src_count) {
	if (output == NULL || src == NULL || len % 4 != 0 || src_count * 4 != len) {
		return;
	}

	for (size_t i = 0; i < src_count; i++) {
		write_as_be(output + i * 4, src[i]);
	}
}

static int regrw(uint32_t addr, uint32_t *value, int is_write)
{
	void *map_base, *virt_addr;
	off_t target;
	unsigned page_size, mapped_size, offset_in_page;
	int fd, width = 32, ret = 0;

	target = addr;
	fd = open("/dev/mem", is_write ? (O_RDWR | O_SYNC) : (O_RDONLY | O_SYNC));
	if (fd < 0) {
		printf("open /dev/mem failed\n");
		return -1;
	}

	mapped_size = page_size = getpagesize();
	offset_in_page = (unsigned)target & (page_size - 1);
	if (offset_in_page + width > page_size) {
		/* This access spans pages.
		 * Must map two pages to make it possible: */
		mapped_size *= 2;
	}
	map_base = mmap(NULL,
			mapped_size,
			is_write ? (PROT_READ | PROT_WRITE) : PROT_READ,
			MAP_SHARED,
			fd,
			target & ~(off_t)(page_size - 1));
	if (map_base == MAP_FAILED) {
		printf("mmap failed\n");
		ret = -1;
		goto close_fd;
	}

	virt_addr = (char*)map_base + offset_in_page;

	if (!is_write)
		*value = *(volatile uint32_t*)virt_addr;
	else
		*(volatile uint32_t*)virt_addr = *value;

	if (munmap(map_base, mapped_size) == -1)
		printf("munmap failed\n");

close_fd:
	close(fd);

	return ret;
}

static int get_cpuinfo(uint32_t addr, uint32_t *value)
{
	return regrw(addr, value, 0);
}

static int aes_crypt_hw( struct aes_para *para,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output )
{
	int i;
	char temp_iv[16];
	const unsigned char *input_for_iv = input;
	unsigned char *output_for_iv = output;

	if( length % 16 ){
		printf("err,hw aes make sure datalen align 16\n");
		return( -1 );
	}

	for (i = 0; i < 4; ++i) {
		para->aesiv[i] = ((uint32_t)iv[i*4 +3] << 24) |
					((uint32_t)iv[i*4 +2] << 16) |
					((uint32_t)iv[i*4 +1] << 8)  |
					(uint32_t)iv[i*4 +0];
	}

	para->status = 0;
	para->datalen = length;

	 if( mode == IN_UNF_CIPHER_WORK_MODE_DEC_CBC ) {
		memcpy(temp_iv, input_for_iv + length - 16, 16);

		para->enworkmode = IN_UNF_CIPHER_WORK_MODE_DEC_CBC;

		ioctl(g_fd, IOCTL_AES_PROCESSING, para);

		memcpy(iv,temp_iv,16);

	} else {
		para->enworkmode = IN_UNF_CIPHER_WORK_MODE_ENC_CBC;
		ioctl(g_fd, IOCTL_AES_PROCESSING, para);
		output_for_iv += length - 16;
		memcpy(iv,output_for_iv,16);
	}

}

static int aes_crypt_hw_cbc(void *src_dma_map_buf,void *dst_dma_map_buf)
{
	struct aes_para para;
	char *src = NULL;
	char *dst = NULL;
	char *check = NULL;
	unsigned int len = 16*1024;
	unsigned int offset = 0;
	int i = 0;
	int dmalen = 0;
	unsigned char iv[16],iv_dec[16];
	unsigned char key[AES_KEY_SIZE / 8];
	struct timeval ttemp,end,mt1,mt2,mt3,mt4;
	long memcpy_time,hwaes_time,totaltime;
	src = malloc(len * 3);
	if(!src){
		printf("Failed to malloc buffer!\n");
		return -1;
	}

	dst = src + len;
	check = dst + len;

	srand((int)(time(0)));
	memset(src, 0, len*3);

	for(i = 0; i < len; i++){
		src[i] =  rand() % 256;
	}

	memset(&para, 0, sizeof(para));

	generate_random_data(key, sizeof(key));
	generate_random_data(iv, sizeof(iv));
	memcpy(iv_dec, iv,16);

	para.keybits = sizeof(key)*8;
	for (i = 0; i < (para.keybits >> 5); ++i) {
		para.aeskey[i] = ((uint32_t)key[i * 4 + 3] << 24) |
					((uint32_t)key[i * 4 + 2] << 16) |
					((uint32_t)key[i * 4 + 1] << 8)  |
					(uint32_t)key[i * 4 + 0];
	}

	//enc
	gettimeofday(&ttemp,NULL);
	offset = 0;
	memcpy_time = 0;
	hwaes_time = 0;
	totaltime = 0;
	while(offset < len){
		dmalen = len - offset > DMABUFSIZE ? DMABUFSIZE : len - offset;
		gettimeofday(&mt1,NULL);
		memcpy(src_dma_map_buf ,src + offset,dmalen);
		gettimeofday(&mt2,NULL);

		aes_crypt_hw(&para,IN_UNF_CIPHER_WORK_MODE_ENC_CBC,dmalen,iv,src_dma_map_buf,dst_dma_map_buf);

		gettimeofday(&mt3,NULL);
		memcpy(dst + offset,dst_dma_map_buf ,dmalen);
		gettimeofday(&mt4,NULL);
		offset += dmalen;

		hwaes_time += ((mt3.tv_sec - mt2.tv_sec)) * 1000000 + (mt3.tv_usec - mt2.tv_usec);
		memcpy_time += ((mt2.tv_sec - mt1.tv_sec)+(mt4.tv_sec - mt3.tv_sec) )* 1000000 + ((mt2.tv_usec - mt1.tv_usec)) + (mt4.tv_usec - mt3.tv_usec);
		totaltime += ((mt4.tv_sec - mt1.tv_sec)) * 1000000 + (mt4.tv_usec - mt1.tv_usec);
	}
	// log_data_enc(len,totaltime,memcpy_time,hwaes_time,"enc_cbc");

	//dec
	offset = 0;
	memcpy_time = 0;
	hwaes_time = 0;
	totaltime = 0;

	while(offset < len){
		dmalen = len - offset > DMABUFSIZE ? DMABUFSIZE : len - offset;
		gettimeofday(&mt1,NULL);
		memcpy(src_dma_map_buf ,dst + offset,dmalen);
		gettimeofday(&mt2,NULL);

		aes_crypt_hw(&para,IN_UNF_CIPHER_WORK_MODE_DEC_CBC,dmalen,iv_dec,src_dma_map_buf,dst_dma_map_buf);

		gettimeofday(&mt3,NULL);
		memcpy(check + offset,dst_dma_map_buf ,dmalen);
		gettimeofday(&mt4,NULL);
		offset += dmalen;
		hwaes_time += ((mt3.tv_sec - mt2.tv_sec)) * 1000000 + (mt3.tv_usec - mt2.tv_usec);
		memcpy_time += ((mt2.tv_sec - mt1.tv_sec)+(mt4.tv_sec - mt3.tv_sec) )* 1000000 + ((mt2.tv_usec - mt1.tv_usec)) + (mt4.tv_usec - mt3.tv_usec);
		totaltime += ((mt4.tv_sec - mt1.tv_sec)) * 1000000 + (mt4.tv_usec - mt1.tv_usec);
	}

	// log_data_dec(len,totaltime,memcpy_time,hwaes_time,"dec_cbc");

	if (memcmp(src, check, len) != 0) {
		printf("[FAIL] aes cbc enc and dec err\n");
		print_hex("src", (const unsigned char *)src, len);
		print_hex("dst", (const unsigned char *)check, len);
		return -1;
	}
	// printf("aes cbc run succeed\n");
	free(src);
	src = NULL;
	return 0;
}

static int aes_crypt_hw_ecb(void)
{
	struct aes_para para;
	char *src = NULL;
	char *dst = NULL;
	char *check = NULL;
	unsigned int len = 1 * 1024 * 1024;
	unsigned int offset = 0;
	int i = 0;
	int dmalen = 0;
	unsigned char iv[16],iv_dec[16];
	unsigned char key[AES_KEY_SIZE / 8];
	struct timeval start,end;
	long deltime;

	src = malloc(len * 3);
	if(!src){
		printf("Failed to malloc buffer!\n");
		return -1;
	}

	dst = src + len;
	check = dst + len;

	srand((int)(time(0)));
	memset(src, 0, len*3);

	for(i = 0; i < len; i++){
		src[i] =  rand() % 256;
	}

	memset(&para, 0, sizeof(para));

	generate_random_data(key, sizeof(key));
	generate_random_data(iv, sizeof(iv));
	memcpy(iv_dec, iv,16);

	para.keybits = sizeof(key)*8;
	for (i = 0; i < (para.keybits >> 5); ++i) {
		para.aeskey[i] = ((uint32_t)key[i * 4 + 3] << 24) |
					((uint32_t)key[i * 4 + 2] << 16) |
					((uint32_t)key[i * 4 + 1] << 8)  |
					(uint32_t)key[i * 4 + 0];
	}

	for (i = 0; i < 4; ++i) {
		para.aesiv[i] = ((uint32_t)iv[i*4 +3] << 24) |
					((uint32_t)iv[i*4 +2] << 16) |
					((uint32_t)iv[i*4 +1] << 8)  |
					(uint32_t)iv[i*4 +0];
	}

	//enc
	gettimeofday(&start,NULL);
	para.src = src;
	para.dst = dst;
	para.datalen = len;
	para.enworkmode = IN_UNF_CIPHER_WORK_MODE_ENC_ECB;
	ioctl(g_fd, IOCTL_AES_PROCESSING, &para);
	gettimeofday(&end,NULL);
	deltime = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
	// log_data(len,deltime,"enc_ecb_enc");

	//dec
	gettimeofday(&start,NULL);
	para.src = dst;
	para.dst = check;
	para.datalen = len;
	para.enworkmode = IN_UNF_CIPHER_WORK_MODE_DEC_ECB;
	ioctl(g_fd, IOCTL_AES_PROCESSING, &para);

	gettimeofday(&end,NULL);
	deltime = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
	// log_data(len,deltime,"enc_ecb_dec");

	if (memcmp(src, check, len) != 0) {
		printf("[FAIL] aes ecb enc and dec err\n");
		print_hex("src", (const unsigned char *)src, len);
		print_hex("dst", (const unsigned char *)check, len);
		return -1;
	}

	printf("aes ecb run succeed\n");
	free(src);
	src = NULL;
	return 0;
}

static void init_test_resources(unsigned char *original_data) {
	static int initialized = 0;
	if (initialized) return;

	srand((unsigned int)time(0));
	generate_random_data(g_key, sizeof(g_key));
	generate_random_data(g_iv, sizeof(g_iv));
	generate_random_data(original_data, TEST_DATA_LEN);

	initialized = 1;
	printf("Test resources initialized (key/iv/data same for tests)\n");
}

// 优化的CBC测试（直接使用用户缓冲区）
static int aes_crypt_hw_optimized() {
	struct aes_para para;
	unsigned char *original_data = NULL;
	unsigned char *dst = NULL;
	unsigned char *check = NULL;
	unsigned char iv[16];  // 局部IV，避免修改全局IV
	int i;
#if 0
	if (posix_memalign((void**)&dst, 4096, TEST_DATA_LEN) != 0 ||
		posix_memalign((void**)&check, 4096, TEST_DATA_LEN) != 0 ||
		posix_memalign((void**)&original_data, 4096, TEST_DATA_LEN) != 0) {
		perror("posix_memalign failed");
		free(dst);
		free(check);
		return -1;
	}
#else
	original_data = malloc(TEST_DATA_LEN);
	dst = malloc(TEST_DATA_LEN);
	check = malloc(TEST_DATA_LEN);
	if(!original_data || !dst || !check){
		printf("Failed to malloc buffer!\n");
		if(original_data) free(original_data);
		if(dst) free(dst);
		if(check) free(check);
		return -1;
	}
#endif

	init_test_resources(original_data);
	memcpy(iv, g_iv, sizeof(iv));

#ifdef GETUSERPAGE
	memset(original_data, 0x55, TEST_DATA_LEN);
#endif
	memset(dst, 0, TEST_DATA_LEN);
	memset(check, 0, TEST_DATA_LEN);

	memset(&para, 0, sizeof(para));
	para.keybits = AES_KEY_SIZE;
	for (i = 0; i < (para.keybits >> 5); i++) {
		para.aeskey[i] = ((uint32_t)g_key[i*4 + 3] << 24) |
						((uint32_t)g_key[i*4 + 2] << 16) |
						((uint32_t)g_key[i*4 + 1] << 8)  |
						(uint32_t)g_key[i*4 + 0];
	}

	for (i = 0; i < 4; i++) {
		para.aesiv[i] = ((uint32_t)iv[i*4 + 3] << 24) |
						((uint32_t)iv[i*4 + 2] << 16) |
						((uint32_t)iv[i*4 + 1] << 8)  |
						(uint32_t)iv[i*4 + 0];
	}

	para.src = original_data;
	para.dst = dst;
	para.datalen = TEST_DATA_LEN;
	para.enworkmode = IN_UNF_CIPHER_WORK_MODE_ENC_CBC;//IN_UNF_CIPHER_WORK_MODE_ENC_ECB;//
	if (ioctl(g_fd, IOCTL_AES_SG_PROCESSING, &para) < 0) {
		printf("Encrypt ioctl failed: %s\n", strerror(errno));
		free(dst);
		free(check);
		return -1;
	}

#ifdef GETUSERPAGE
	for(i = 0; i <TEST_DATA_LEN; i++){
		if(g_original_data[i] != 0xaa){
			printf("original data[%d]=0x%02x\n",i,g_original_data[i]);
		} else {
			printf("original data[%d]=0x%02x\n",i,g_original_data[i]);
		}
	}
#endif
	convert_to_manual(dst, TEST_DATA_LEN,(const uint32_t *)dst, TEST_DATA_LEN / 4);
	para.src = dst;
	para.dst = check;
	para.enworkmode = IN_UNF_CIPHER_WORK_MODE_DEC_CBC;//IN_UNF_CIPHER_WORK_MODE_DEC_ECB;//
	if (ioctl(g_fd, IOCTL_AES_SG_PROCESSING, &para) < 0) {
		printf("Decrypt ioctl failed: %s\n", strerror(errno));
		free(dst);
		free(check);
		return -1;
	}

#ifdef CHECK_RESULT
	// 校验结果
	printf("original data:\n");
	for(i = 0; i < para.datalen; i++){
		printf("%02x ",original_data[i]);
	}
	printf("\n");
	printf("decrypted data:\n");
	for(i = 0;i < para.datalen; i++){
		printf("%02x ",check[i]);
	}
#endif
	if (memcmp(original_data, check, TEST_DATA_LEN) != 0) {
		printf("[FAIL] aes_crypt_hw_cbc_optimized: Encrypt/decrypt mismatch\n");
		for(i = 0; i < TEST_DATA_LEN; i++){
			if(original_data[i] != check[i]){
				printf("mismatch at byte %d: original=0x%02x, decrypted=0x%02x,original:%p,check:%p\n",
					i, original_data[i], check[i],&original_data[i], &check[i]);
			}
		}
		print_hex("Original", original_data, 32);
		print_hex("Decrypted", check, 32);
		free(dst);
		free(check);
		return -1;
	}
	// printf("[SUCCESS]: Test passed\n");
	free(original_data);
	free(dst);
	free(check);
	return 0;
}
int main(int argc, const char* argv[])
{

	struct timeval mt1,mt2;
	unsigned long totaltime;
	CPUID cpu_id = SOC_START;
	uint32_t soc_id = 0;
	int i;
	g_fd = open("/dev/aes", O_RDWR);
	if(g_fd < 0){
		printf("Failed to open /dev/aes\n");
		return -1;
	}

	cpu_id = get_cpuinfo(GETCHIPID, &soc_id);
	if(((soc_id & (0xffff<<12))>>12) == 0x33){
		cpu_id = SOC_T33;
	} else if(((soc_id & (0xffff<<12))>>12) == 0x32){
		cpu_id = SOC_T32;
	} else {
		printf("Not support soc id:0x%08x\n",soc_id);
		close(g_fd);
		return -1;
	}

	if(cpu_id == SOC_T32){
		void *src_dma_map_buf = NULL;
		void *dst_dma_map_buf = NULL;
		src_dma_map_buf = mmap(NULL, DMABUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
		if (src_dma_map_buf == MAP_FAILED) {
			perror("mmap src failed");
			exit(EXIT_FAILURE);
		}

		gettimeofday(&mt1,NULL);
		if(aes_crypt_hw_cbc(src_dma_map_buf,src_dma_map_buf)){
			return -1;
		}
		gettimeofday(&mt2,NULL);
		totaltime = (mt2.tv_sec - mt1.tv_sec) * 1000000 + (mt2.tv_usec - mt1.tv_usec);
		log_data_enc(1,totaltime,0,0,"map_cbc");

		munmap(src_dma_map_buf, DMABUFSIZE);
	}
	if(cpu_id == SOC_T33){
		gettimeofday(&mt1,NULL);
		if (aes_crypt_hw_optimized() != 0) {
			printf("Optimized hw test failed\n");
			return -1;
		}
		gettimeofday(&mt2,NULL);

		totaltime = (mt2.tv_sec - mt1.tv_sec) * 1000000 + (mt2.tv_usec - mt1.tv_usec);
		log_data_enc(1,totaltime,0,0,"optimized_cbc");
	}

	close(g_fd);
	return 0;
}
