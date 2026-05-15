#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define PWM_PATH "/sys/class/pwm/pwmchip0/"
#define PERIOD (2 * 1000 * 1000)
#define DUTY (PERIOD / 2)

struct pwm_data
{
    int channel;
    unsigned int duty; // 0 <= duty <= period
    unsigned int period;
};
struct pwm_data pwmdata = {
    0,
    DUTY,
    PERIOD,
};

int main(int argc, char **argv)
{
    int ret, pwmfd, enablefd, periodfd, dutyfd;
    char ch, chan;
    char pwmbuf[50];

    if (argc != 2)
    {
        printf("Please input: pwm_test channel\n");
        printf("For example: pwm_test 0\n");
        return 0;
    }
    pwmdata.channel = atoi(argv[1]);
    if (pwmdata.channel > 7 || pwmdata.channel < 0)
    {
        printf("channel error. T40,T41:0~7, T2*,T3*::0~3\n");
        return 0;
    }

    // export PWM channel
    pwmfd = open(PWM_PATH "export", O_WRONLY);
    if (pwmfd < 0)
    {
        perror("error");
        printf("Open " PWM_PATH "export error\n");
        return -1;
    }
    chan = '0' + pwmdata.channel;
    write(pwmfd, &chan, 1);
    close(pwmfd);

    // sprintf(char *str, const char *format, ...)
    sprintf(pwmbuf, PWM_PATH "pwm%u/enable", pwmdata.channel);
    enablefd = open(pwmbuf, O_WRONLY);
    sprintf(pwmbuf, PWM_PATH "pwm%u/period", pwmdata.channel);
    periodfd = open(pwmbuf, O_RDWR);
    sprintf(pwmbuf, PWM_PATH "pwm%u/duty_cycle", pwmdata.channel);
    dutyfd = open(pwmbuf, O_RDWR);

    while (1)
    {
        printf("\n0:Disable PWM, 1:Enable PWM, 2:period,duty, q:Exit\n");
        printf("choose:");
        scanf(" %c", &ch);
        switch (ch)
        {
        case '0':
            write(enablefd, "0", 1);
            break;
        case '1':
            write(enablefd, "1", 1);
            break;
        case '2':
            printf("Please input(period,duty):"); // unit: ns
            scanf("%u,%u", &pwmdata.period, &pwmdata.duty);
            sprintf(pwmbuf, "%u", pwmdata.period);
            write(periodfd, pwmbuf, strlen(pwmbuf));
            sprintf(pwmbuf, "%u", pwmdata.duty);
            write(dutyfd, pwmbuf, strlen(pwmbuf));
            break;
        case 'q':
        case 'Q':
            write(enablefd, "0", 1);
            goto end;
        default:
            printf("Parameter error\n");
            break;
        }
    }

end:
    close(dutyfd);
    close(periodfd);
    close(enablefd);

    // unexport PWM channel
    pwmfd = open(PWM_PATH "unexport", O_WRONLY);
    if (pwmfd < 0)
    {
        printf("Open " PWM_PATH " unexport error\n");
        return -1;
    }
    chan = '0' + pwmdata.channel;
    write(pwmfd, &chan, 1);
    close(pwmfd);

    return 0;
}
