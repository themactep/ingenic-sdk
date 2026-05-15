#ifndef __TMI8153_SPI_DEV_H__
#define __TMI8153_SPI_DEV_H__

#define MOTOR_DRV_OC	0x0 //Channel output open circuit
#define MOTOR_DRV_CW	0x1 //Clockwise rotation
#define MOTOR_DRV_CCW	0x2 //counterclockwise rotation
#define MOTOR_DRV_BRAKE 0x3 //Braking; stop it; brake

enum
{
	TMI8153_CDIV_1,   // no frequency division
	TMI8153_CDIV_2,   // sysclk2 frequency division
	TMI8153_CDIV_4,   // sysclk4 frequency division
	TMI8153_CDIV_8,   // sysclk8 frequency division
	TMI8153_CDIV_16,  // sysclk16 frequency division
	TMI8153_CDIV_32,  // sysclk32 frequency division
	TMI8153_CDIV_64,  // sysclk64 frequency division
	TMI8153_CDIV_128, // sysclk128 frequency division
}clockchn_source;

//Note: When the PWM frequency of the security IPC motor is below 16kHz, the electromagnetic noise will gradually increase, so it is recommended to set the frequency division number to be less than or equal to 7
enum
{
	TMI8153_CSEL_1, // chdivclk
	TMI8153_CSEL_2, // chdivclk/2
	TMI8153_CSEL_3, // chdivclk/3
	TMI8153_CSEL_4, // chdivclk/4
	TMI8153_CSEL_5, // chdivclk/5
	TMI8153_CSEL_6, // chdivclk/6
	TMI8153_CSEL_7, // chdivclk/7
	TMI8153_CSEL_8, // chdivclk/8
	TMI8153_CSEL_9, // chdivclk/9
}clk_csel;

enum
{
	TMI8153_STEPMD_64,	 //64 steps 16 subdivisions
	TMI8153_STEPMD_128,  //128 steps, 32 subdivisions
	TMI8153_STEPMD_256,  //256 steps 64 subdivisions
};

typedef enum motornum {
	MOTOR_DFT,
	MOTOR_1,
	MOTOR_2,
	MOTOR_BUTT,
}motor_num;

typedef enum {
	MOTOR_AUTO,
	MOTOR_MANUAL,
}motor_contrl;

struct stMotorConfig
{
	unsigned char ch_stepmd;		//subdivision coefficient
	unsigned char rotation_dir;		// rotation direction; Forward mode
	unsigned char sysclk_div;		//Channel clock source frequency division
	unsigned char clk_csel;			//Channel clock division
	unsigned char control_mode;		//Control mode, currently only supports manual mode
	unsigned char phset;			//Phase setting
	unsigned short circle_set;		//Number of circles to run
};

struct spi_motor_platform_data
{
	char *name;
	int id;
};

//Application layer setting parameters
typedef struct spi_param
{
	motor_num motor_num;//Which motor currently only supports two motors
	struct stMotorConfig motor_config;
}spi_motor_param_t;

#define  SPI_MOTOR_STOP		_IOWR('m', 1, spi_motor_param_t)
#define  SPI_MOTOR_RUN		_IOWR('m', 2, spi_motor_param_t)
#define  SPI_MOTOR_PAUSE	_IOWR('m', 3, spi_motor_param_t)
#define  SPI_MOTOR_RESET	_IOWR('m', 4, spi_motor_param_t)
#define  SPI_MOTOR_IRCUT	_IOWR('m', 5, spi_motor_param_t)
#define  SPI_MOTOR_REGISTER_DEBUG _IOWR('m', 5, int)

#endif /*__ingenic_SPI_DEV_H__*/
