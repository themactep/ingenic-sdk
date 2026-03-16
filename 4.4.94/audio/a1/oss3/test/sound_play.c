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

//#define	SRC_NAME	"1KHZ-8K.snd"
#define	SRC_NAME	"play.pcm"
//#define	SRC_NAME	"test.pcm"
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

int main(int argc, const char *argv[])
{
	int ret = 0;
	int fmt = 16;
	int fd;
	int ao_fd;
	int samplerate = 8000;
	int channel_count = 1;
	char src_path[128] = {0};
    struct audio_parameter param;
	struct audio_ouput_stream ao_stream;
	struct volume gain;
	char buff[320] = {0};//play
	//char buff[1920] = {0};

	sprintf(src_path, "%s", SRC_NAME);

	fd = open(src_path, O_RDONLY);
	if (fd < 0)
		printf("open 1KHZ-8K.snd error! errno = %d \n", fd);

	ao_fd = open("/dev/dsp", O_WRONLY);
	if (ao_fd < 0) {
		printf("ao dsp error\n");
		return -1;
	}

    param.rate = samplerate;
	param.channel = channel_count;
	param.format = fmt;

    ret = ioctl(ao_fd, AMIC_AO_SET_PARAM, &param);
	if (ret) {
		printf("ao set param error.\n");
        return -1;
	}

	ret = ioctl(ao_fd, AMIC_AO_ENABLE_STREAM, 1);
	if (ret) {
		printf("ao enable error!\n");
		return -1;
	}
#if 1
	gain.channel = 1;
	gain.gain = 31;
	ret = ioctl(ao_fd,AMIC_SPK_SET_GAIN, &gain);
	if(ret) {
		printf("ioctl AMIC_SPK_SET_GAIN failed.\n");
		return -1;
	}
	gain.channel = 1;
	gain.gain = 0xff;
	ret = ioctl(ao_fd,AMIC_SPK_SET_VOLUME, &gain);
	if(ret) {
		printf("ioctl AMIC_SPK_SET_GAIN failed.\n");
		return -1;
	}
#endif
	memset(buff, 0, 320);
	while(1) {
		ret = read(fd, buff, 320);
		if(ret <= 0)
			break;
		ao_stream.data = buff;
		ao_stream.size = 320;
		ret = ioctl(ao_fd, AMIC_AO_SET_STREAM, &ao_stream);
		if (ret != 0) {
			printf("ao play stream error\n");
			return -1;
		}
	}
	ret = ioctl(ao_fd, AMIC_AO_SYNC_STREAM, 1);
	if(ret){
		printf("ao AMIC_AO_SYNC_STREAM error\n");
		return -1;
	}
	ret = ioctl(ao_fd, AMIC_AO_DISABLE_STREAM, 1);
	if (ret != 0) {
		printf("ao AMIC_AO_DISABLE_STREAM error\n");
		return -1;
	}
	close(fd);
	close(ao_fd);

	return 0;
}
