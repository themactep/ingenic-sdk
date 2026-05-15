/*
 * @Description  : T32 LZMA 测试程序
 * @Date         : 2024-03-01
 * @FilePath     : sample_lzma.c
 * @Author       : oywei <setsuna.oywei@ingenic.com>
 * @Author       : jim.jwang <jim.jwang@ingenic.com>
 * * Copyright (C) 2024 Ingenic Semiconductor Co., Ltd.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>


#define JZLZMA_IOC_MAGIC  'L'
#define IOCTL_LAMA_WORK			_IOW(JZLZMA_IOC_MAGIC, 01, struct lzma_file_info)

#define LZMA_VERSION "20231219"		// 定义LZMA版本号
#define LZMA_OUT_NAME "jz_lzma_out" // 如果没有指定输出文件名，则使用默认的输出文件名
#define LZMA_DEVICE "/dev/jz_lzma"	// 定义LZMA设备文件路径
#define DBUG() printf("----%d\n", __LINE__);
#define MAXNUM 2		//定义最大通道数

enum
{
	OPT_HELP,		 // 定义帮助选项
	OPT_VERSION,	 // 定义版本选项
	OPT_OUTPUT_FILE, // 定义输出文件选项
};

static const struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},			   // 定义长选项--help
	{"version", no_argument, NULL, 'v'},		   // 定义长选项--version
	{"output-file", required_argument, NULL, 'o'}, // 定义长选项--output-file，需要参数
	{NULL, 0, NULL, 0},
};

struct lzma_file_info
{
	char *input_name;  // 输入文件名
	char *output_name; // 输出文件名
	int size;		   // 文件大小
	char channel;		//lzma通道，T32目前只支持两个通道
};

static volatile int g_stop = 0;

// SIGINT信号处理函数
static void sigint_handler(int signum)
{
	printf("\nReceived SIGINT signal.\n");
	g_stop = 1;
}

// 打印帮助信息
static void print_help()
{
	printf("Usage: lzma [OPTIONS] INPUT_FILE\n");
	printf("Compresses INPUT_FILE using the LZMA algorithm.\n");
	printf("\n");
	printf("Options:\n");
	printf("  -h, --help               Display this help and exit.\n");
	printf("  -v, --version            Display version information and exit.\n");
	printf("  -o, --output-file=FILE   Set the output file name. If not specified,\n");
	printf("                           the default output file name(jz_lzma_test.lzma) will be used.\n");
}

// 打印版本信息
static void print_version()
{
	printf("LZMA case test version: %s\n", LZMA_VERSION);
}

// 解压函数
static int lzma_compress(struct lzma_file_info *file_info)
{
	struct stat st;
	int fd;

	// 获取文件信息（大小等）
	if (stat(file_info->input_name, &st) < 0)
	{
		perror("stat");
		return -1;
	}
	file_info->size = st.st_size;

	printf("[app][%s][%d] filename:%s,size:%d\n",__func__,__LINE__,file_info->input_name,file_info->size);
	// 打开LZMA设备文件
	fd = open(LZMA_DEVICE, O_RDWR);
	if (fd < 0)
	{
		perror("open");
		return -1;
	}

	// 调用ioctl进行LZMA解压
	 if (ioctl(fd, IOCTL_LAMA_WORK, file_info) != 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

	close(fd);
	return 0;
}

void * lzma_uncompress(void *arg)
{
	struct lzma_file_info *file_info = NULL;
	file_info = (struct lzma_file_info *)arg;
	printf("this is phread:%d\n",file_info->channel);
	lzma_compress(file_info);
	return "lzmauncompress";
}

// 主函数
int main(int argc, char **argv)
{
	struct lzma_file_info file_info[MAXNUM] = {{0},{0}};
	char name[MAXNUM][32] = {{0},{0}};
	int i = 0;
	int opt, indexptr, option_index;
	pthread_t tid[2];

	signal(SIGINT, sigint_handler); // 注册SIGINT信号处理函数

	// 处理命令行参数
	while ((opt = getopt_long(argc, argv, "hvo:", longopts, &option_index)) != -1)
	{
		switch (opt)
		{
		case 'h': // 如果是-h/--help，则打印帮助信息并退出程序
			print_help();
			return 0;
		case 'v': // 如果是-v/--version，则打印版本信息并退出程序
			print_version();
			return 0;
		case 'o': // 如果是-o/--output-file，则设置输出文件名
			for(i= 0;i < argc-1;i++){
				snprintf(name[i],32,"%s%d",optarg,i);
				file_info[i].output_name = name[i];
			}
			break;
		case '?':
			return -1;
		}
	}

	indexptr = optind;
	if(indexptr > 2 || indexptr < 1){
		printf("[%s][%d]invalid param\n",__func__,__LINE__);
		print_help();
		return -1;
	}
	if (indexptr < argc) // 获取输入文件名
	{
		// file_info.input_name = argv[indexptr];
		// file_info.channel = indexptr - 1;
	}
	else // 如果没有输入文件名，则打印帮助信息并退出程序
	{
		print_help();
		return -1;
	}


	if (!g_stop)
	{
		for(i = 0;i < argc - 1;i++){
			file_info[i].input_name = argv[i+1];
			file_info[i].channel = i;
			snprintf(name[i],32,"jz_lzma_out%d",i);
			file_info[i].output_name = name[i];
			printf("chan:%d,inname:%s,outname:%s\n",file_info[i].channel,file_info[i].input_name,file_info[i].output_name);
			pthread_create(&tid[i],NULL,lzma_uncompress,&file_info[i]);
		}

		for(i = 0; i < indexptr;i++){
			pthread_join(tid[i],NULL);
		}
	}

	return 0;
}
