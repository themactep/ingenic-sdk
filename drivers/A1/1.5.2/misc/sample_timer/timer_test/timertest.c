#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TTIMER_STOP			0x1
#define TTIMER_START		0x2
#define TTIMER_SPEED		0x3

int main(int argc, const char* argv[])
{
	int a,b,c,d,e,f;
	int fd = open("/dev/ttimer", 0);
	while(1)
	{
		printf("action：\n");
		printf("1:TIMER_STOP 2:TIMER_START 3:TIMER_SPEED\n");
		printf("choose:");
		scanf("%d",&d);
		switch(d)
		{
			case TTIMER_STOP:
			case TTIMER_START:
				ioctl(fd, d, 0);
				break;
			case TTIMER_SPEED:
				printf("speed：");
				scanf("%d",&e);
				ioctl(fd, d, (unsigned long)&e);
				break;
		}

	}
	close(fd);
	return 0;
}
