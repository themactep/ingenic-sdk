/*
 * =====================================================================================
 *       Filename:  gpio_test.c
 *
 *    Description:l
 *
 *        Version:  1.0
 *        Created:  2021/07/05 11:32
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yiwen.han@ingenic.com
 *   Organization:
 * =====================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <error.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

struct gpio_des {
	uint32_t pin;
	uint32_t irq_mode;
};

#define GPIO_PA(n)		(0 * 32 + n)
#define GPIO_PB(n)		(1 * 32 + n)
#define GPIO_PC(n)		(2 * 32 + n)
#define GPIO_PD(n)		(3 * 32 + n)
#define GPIO_PE(n)		(4 * 32 + n)

#define REQUEST_GPIO		_IOW('G',1, struct gpio_des)
#define FREE_GPIO		_IOW('G',2, struct gpio_des)
#define GPIO_PULL_UP		_IOW('G',3, struct gpio_des)
#define GPIO_PULL_DOWN		_IOW('G',4, struct gpio_des)
#define GPIO_REQUEST_IRQ	_IOW('G',5, struct gpio_des)
#define GPIO_FREE_IRQ		_IOW('G',6, struct gpio_des)

#define TRIGGER_RISING		1
#define TRIGGER_FALLING		2
#define TRIGGER_RISING_FALLING	3

#define GPIO_TEST_PIN	GPIO_PA(29)
#define IRQ_TEST_PIN	GPIO_PC(2)

int test_gpio_up_down(uint32_t pin);
int test_gpio_irq(uint32_t irq_pin, uint32_t pin, uint32_t irq_mode);

int fb;
int main(int argc, char *argv[])
{
	int i;
	char c;


	printf("#################################################\n");
	printf("0. exit\n");
	printf("1. test gpio pull up and pull down\n");
	printf("2. test gpio rising edge irq\n");
	printf("3. test gpio falling edge irq\n");
	printf("4. test gpio rising edge and falling edge irq\n");
	printf("please enter: ");
	scanf("%c",&c);
	printf("\n\n\n");

	fb = open("/dev/jz_gpio",O_RDWR);
	if(fb < 0) {
		perror("open error");
		return -1;
	}

	switch(c) {
	case '0':
		exit(0);
	case '1':
		test_gpio_up_down(GPIO_TEST_PIN);
		break;
	case '2':
		test_gpio_irq(GPIO_TEST_PIN, IRQ_TEST_PIN, TRIGGER_RISING);
		break;
	case '3':
		test_gpio_irq(GPIO_TEST_PIN, IRQ_TEST_PIN, TRIGGER_FALLING);
		break;
	case '4':
		test_gpio_irq(GPIO_TEST_PIN, IRQ_TEST_PIN, TRIGGER_RISING_FALLING);
		break;
	default :
		printf("invalid argument\n");
		break;
	}
	close(fb);

	return 0;
}

int test_gpio_up_down(uint32_t pin)
{
	int i;
	int ret = 0;
	struct gpio_des gpio;

	gpio.pin = pin;
	ret = ioctl(fb, REQUEST_GPIO, &gpio);
	if(ret < 0) {
		perror("REQUEST_GPIO error");
	}
	for(i = 0; i < 20; i++) {
		ret = ioctl(fb, GPIO_PULL_UP, &gpio);
		if(ret < 0) {
			perror("GPIO_PULL_UP error");
		}
		usleep(100*1000);
		ret = ioctl(fb, GPIO_PULL_DOWN, &gpio);
		if(ret < 0) {
			perror("GPIO_PULL_DOWN error");
		}
		usleep(100*1000);
	}
	ret = ioctl(fb, FREE_GPIO, &gpio);
	if(ret < 0) {
		perror("FREE_GPIO error");
	}

	return ret;

}

int test_gpio_irq(uint32_t irq_pin, uint32_t pin, uint32_t irq_mode)
{
	int ret = 0;
	struct gpio_des gpio;

	gpio.pin = irq_pin;
	gpio.irq_mode = irq_mode;
	ret = ioctl(fb, REQUEST_GPIO, &gpio);
	if(ret < 0) {
		perror("REQUEST_GPIO error");
	}
	ret = ioctl(fb, GPIO_REQUEST_IRQ, &gpio);
	if(ret < 0) {
		perror("GPIO_REQUEST_IRQ error");
	}
	test_gpio_up_down(pin);
	ret = ioctl(fb, GPIO_FREE_IRQ, &gpio);
	if(ret < 0) {
		perror("FREE_GPIO error");
	}

	return ret;
}

