#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define AMIC_AI_SET_PARAM			_SIOR ('P', 113, struct audio_parameter)
#define AMIC_AI_GET_PARAM			_SIOR ('P', 112, struct audio_parameter)
#define AMIC_AO_SET_PARAM			_SIOR ('P', 111, struct audio_parameter)
#define AMIC_AO_GET_PARAM			_SIOR ('P', 110, struct audio_parameter)
#define AMIC_AO_SYNC_STREAM        		_SIOR ('P', 101, int)
#define AMIC_AO_CLEAR_STREAM        		_SIOR ('P', 100, int)
#define AMIC_AO_SET_STREAM        		_SIOR ('P', 99, struct audio_ouput_stream)
#define AMIC_AI_GET_STREAM        		_SIOR ('P', 98, struct audio_input_stream)
#define AMIC_AI_DISABLE_STREAM			_SIOR ('P', 97, int)
#define AMIC_AI_ENABLE_STREAM			_SIOR ('P', 96, int)
#define AMIC_AO_DISABLE_STREAM			_SIOR ('P', 95, int)
#define AMIC_AO_ENABLE_STREAM			_SIOR ('P', 94, int)
#define AMIC_ENABLE_AEC        			_SIOR ('P', 93, int)
#define AMIC_DISABLE_AEC			_SIOR ('P', 92, int)
#define AMIC_AI_SET_VOLUME			_SIOR ('P', 91, struct volume)
#define AMIC_AI_SET_GAIN			_SIOR ('P', 90, struct volume)
#define AMIC_SPK_SET_VOLUME			_SIOR ('P', 89, struct volume)
#define AMIC_SPK_SET_GAIN			_SIOR ('P', 88, struct volume)
#define AMIC_AI_GET_VOLUME			_SIOR ('P', 86, struct volume)
#define AMIC_AI_GET_GAIN			_SIOR ('P', 85, struct volume)
#define AMIC_SPK_GET_VOLUME			_SIOR ('P', 84, struct volume)
#define AMIC_SPK_GET_GAIN			_SIOR ('P', 83, struct volume)
#define AMIC_AI_HPF_ENABLE	    		_SIOR ('P', 79, int)
#define AMIC_AI_SET_MUTE	    		_SIOR ('P', 78, int)
#define AMIC_SPK_SET_MUTE	    		_SIOR ('P', 77, int)
#define AMIC_AI_SET_ALC_GAIN	   	 	_SIOR ('P', 76, struct alc_gain)
#define AMIC_AI_GET_ALC_GAIN	    		_SIOR ('P', 75, struct alc_gain)

struct audio_parameter {
	unsigned int rate;
	unsigned short format;
	unsigned short channel;
};

struct volume {
	unsigned int channel;
	unsigned int gain;
};

struct audio_ouput_stream {
	void * data;
	unsigned int size;
};

struct audio_input_stream {
	void *data;
	unsigned int size;
	void *aec;
	unsigned int aec_size;
	int			timeout;
};

int main(int argc, const char *argv[])
{
	int i;
	int ret = 0;
	int fmt = 16;
	int fd_rec = 0;
	int ai_fd = 0;
	int channel_count = 1;
	int samplerate = 8000;
	int depth = 16;
	int per_sample = 2;
	char file_ai[16] = {0};
	char test_path[128] = {0};
    struct audio_parameter param;
	struct audio_input_stream ai_stream;
	struct volume gain;
	char *aibuff = NULL;
	int buf_sz = 0;
	int frm_cnt = 1600;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-s")) {
			samplerate = atoi(argv[++i]);
		}
		if (!strcmp(argv[i], "-d")) {
			depth = atoi(argv[++i]);
			if(depth == 24){
				per_sample = 4;	
			}else if(depth == 16){
				per_sample = 2;	
			}
		}
	}
	printf("depth=%d per_sample=%d\n",depth,per_sample);
	buf_sz = samplerate/100 * 4 * per_sample;//40ms
	aibuff = (char *)malloc(buf_sz);
	if(!aibuff){
		return 0;
	}

	printf("buf_sz=%d \n",buf_sz);
	ai_fd = open("/dev/dsp", O_WRONLY);
	if (ai_fd < 0) {
		printf("ai dsp error\n");
		return -1;
	}
    param.rate = samplerate;
	param.channel = 1;//mono
	param.format = depth;
	ret = ioctl(ai_fd, AMIC_AI_SET_PARAM, &param);
	if (ret) {
		printf("ai set param error.\n");
		return -1;
	}
	ret = ioctl(ai_fd, AMIC_AI_ENABLE_STREAM, 1);
	if (ret) {
		printf("ao enable error!\n");
		return -1;
	}
	gain.channel = 1;
	gain.gain = 31;
	ret = ioctl(ai_fd,AMIC_AI_SET_GAIN, &gain);
	if(ret) {
		printf("ioctl AMIC_AI_SET_GAIN failed.\n");
		return -1;
	}

	gain.channel = 1;
	gain.gain = 0xC3;
	ret = ioctl(ai_fd,AMIC_AI_SET_VOLUME, &gain);
	if(ret) {
		printf("ioctl AMIC_AI_SET_VOLUME failed.\n");
		return -1;
	}

	sprintf(file_ai,"test-%d",0);
	fd_rec = open(file_ai, O_WRONLY|O_CREAT);
	if (fd_rec < 0)
		printf("open codec_test_r.snd error! errno = %d \n", fd_rec);

	while(frm_cnt--) {
		if(frm_cnt%32 == 0){
			close(fd_rec);
			memset(file_ai,0,16);
			sprintf(file_ai,"test-%d",1600/32 - (frm_cnt-1)/32);
			fd_rec = open(file_ai, O_WRONLY|O_CREAT);
			if (fd_rec < 0)
				printf("open codec_test_r.snd error! errno = %d \n", fd_rec);
		}
		ai_stream.data = aibuff;
		ai_stream.size = buf_sz;
		ai_stream.aec_size = 0;
		ai_stream.aec = NULL;
		ai_stream.timeout = -1;

		ret = ioctl(ai_fd,AMIC_AI_GET_STREAM, &ai_stream);
		if (ret != 0) {
			printf("ai AMIC_AI_GET_STREAM error\n");
			return -1;
		}

		write(fd_rec, aibuff, buf_sz);
	}
	ret = ioctl(ai_fd, AMIC_AI_DISABLE_STREAM, 1);
	if (ret != 0) {
		printf("ai AMIC_AI_DISABLE_STREAM error\n");
		return -1;
	}
	close(fd_rec);
	return 0;
}
