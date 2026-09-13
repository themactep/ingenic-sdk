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

#define	TEST_NAME	"play.pcm"
#define AMIC_AI_SET_PARAM			_SIOR ('P', 113, struct audio_parameter)
#define AMIC_AI_GET_PARAM			_SIOR ('P', 112, struct audio_parameter)
#define AMIC_AO_SET_PARAM			_SIOR ('P', 111, struct audio_parameter)
#define AMIC_AO_GET_PARAM			_SIOR ('P', 110, struct audio_parameter)

#define AMIC_AO_SYNC_STREAM        	_SIOR ('P', 101, int)
#define AMIC_AO_CLEAR_STREAM        _SIOR ('P', 100, int)
#define AMIC_AO_SET_STREAM        	_SIOR ('P', 99, struct audio_ouput_stream)
#define AMIC_AI_GET_STREAM        	_SIOR ('P', 98, struct audio_input_stream)
#define AMIC_AI_DISABLE_STREAM		_SIOR ('P', 97, int)
#define AMIC_AI_ENABLE_STREAM		_SIOR ('P', 96, int)
#define AMIC_AO_DISABLE_STREAM		_SIOR ('P', 95, int)
#define AMIC_AO_ENABLE_STREAM		_SIOR ('P', 94, int)
#define AMIC_ENABLE_AEC        		_SIOR ('P', 93, int)
#define AMIC_DISABLE_AEC			_SIOR ('P', 92, int)

#define AMIC_AI_SET_VOLUME			_SIOR ('P', 91, struct volume)
#define AMIC_AI_SET_GAIN			_SIOR ('P', 90, struct volume)
#define AMIC_SPK_SET_VOLUME			_SIOR ('P', 89, struct volume)
#define AMIC_SPK_SET_GAIN			_SIOR ('P', 88, struct volume)
#define AMIC_AI_GET_VOLUME			_SIOR ('P', 86, struct volume)
#define AMIC_AI_GET_GAIN			_SIOR ('P', 85, struct volume)
#define AMIC_SPK_GET_VOLUME			_SIOR ('P', 84, struct volume)
#define AMIC_SPK_GET_GAIN			_SIOR ('P', 83, struct volume)
#define AMIC_AI_HPF_ENABLE	    	_SIOR ('P', 79, int)
#define AMIC_AI_SET_MUTE	    	_SIOR ('P', 78, int)
#define AMIC_SPK_SET_MUTE	    	_SIOR ('P', 77, int)
#define AMIC_AI_SET_ALC_GAIN		_SIOR ('P', 76, struct alc_gain)
#define AMIC_AI_GET_ALC_GAIN	    _SIOR ('P', 75, struct alc_gain)


struct audio_parameter {
	unsigned int rate;
	unsigned short format;
	unsigned short channel;
};

struct volume {
	int channel;
	int gain;
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
};

int main(int argc, const char *argv[])
{
	int ret = 0;
	int fmt = 16;
	int fd_rec = 0;
	int ai_fd = 0;
	int channel_count = 1;
	char test_path[128] = {0};
    struct audio_parameter param;
	struct audio_input_stream ai_stream;
	struct volume gain;
	char aibuff[640] = {0};
	int frm_cnt = 200;

	sprintf(test_path, "%s", TEST_NAME);

	fd_rec = open(test_path, O_WRONLY | O_CREAT);
	if (fd_rec < 0)
		printf("open codec_test_r.snd error! errno = %d \n", fd_rec);
	ai_fd = open("/dev/dsp", O_WRONLY);
	if (ai_fd < 0) {
		printf("ai dsp error\n");
		return -1;
	}
    param.rate = 8000;
	param.channel = 1;//mono
	param.format = 16;
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

	while(frm_cnt--) {
		ai_stream.data = aibuff;
		ai_stream.size = 640;
		ai_stream.aec_size = 0;
		ai_stream.aec = NULL;

		ret = ioctl(ai_fd,AMIC_AI_GET_STREAM, &ai_stream);
		if (ret != 0) {
			printf("ai SNDCTL_EXT_GET_AI_STREAM error\n");
			return -1;
		}
		write(fd_rec, aibuff, 640);
	}
	ret = ioctl(ai_fd, AMIC_AI_DISABLE_STREAM, 1);
	if (ret != 0) {
		printf("ai AMIC_AI_DISABLE_STREAM error\n");
		return -1;
	}
	close(fd_rec);
	return 0;
}
