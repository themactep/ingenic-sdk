#include <errno.h>
#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
// #include "linux/ioctl.h"
#include <sys/ioctl.h>

#define DEV_NAME "/dev/tcu"

#define TIMER_STOP 0x1
#define TIMER_START 0x2
#define TIMER_SET_TIME 0x3

int main(int argc, const char *argv[])
{
    int fd;
    unsigned int time;
    char ch;
    char *path = "/dev/timer";
    if ((fd = open(path, O_RDWR)) < 0)
    {
        printf("open %s failed:%s\n", path, strerror(errno));
        return -1;
    }
    while (1)
    {
        printf("\nSTOP:1  START:2  SET_TIME:3  Exit:q\n");
        printf("choose:");
        scanf(" %c", &ch);
        switch (ch)
        {
        case '1':
            ioctl(fd, TIMER_STOP, 0);
            break;
        case '2':
            ioctl(fd, TIMER_START, 0);
            break;
        case '3':
            printf("time:");
            scanf("%u", &time);
            ioctl(fd,TIMER_SET_TIME, (unsigned long)&time);
            break;
        case 'q': //Exit
            close(fd);
            printf("Exit\n");
            return 0;
            break;
        }
    }
    close(fd);
    return 0;
}
