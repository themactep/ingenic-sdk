#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <jz_proc.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <soc/gpio.h>

#ifdef CONFIG_SOC_T40
static unsigned i2c_adapter_nr = 1;
#else
static unsigned i2c_adapter_nr = 0;
#endif
module_param(i2c_adapter_nr, uint, 0644);
MODULE_PARM_DESC(i2c_adapter_nr, "sensor used i2c_adapter nr");

#ifdef CONFIG_SOC_T40
static int reset_gpio = GPIO_PC(27);
#elif defined(CONFIG_SOC_T41)
static int reset_gpio = GPIO_PC(28);
#else
static int reset_gpio = GPIO_PA(18);
#endif
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

#ifdef CONFIG_SOC_T40
static int cim1_gpio = GPIO_PC(30);
module_param(cim1_gpio, int, S_IRUGO);
MODULE_PARM_DESC(cim1_gpio, "Cim1 GPIO NUM");
#endif

#ifdef CONFIG_SOC_T41
static int cim_gpio = GPIO_PA(15);
module_param(cim_gpio, int, S_IRUGO);
MODULE_PARM_DESC(cim_gpio, "Cim GPIO NUM");
#endif

#define SENSOR_INFO_IOC_MAGIC  'S'
#define IOCTL_SINFO_GET			_IO(SENSOR_INFO_IOC_MAGIC, 100)
#define IOCTL_SINFO_FLASH		_IO(SENSOR_INFO_IOC_MAGIC, 101)

#define SENSOR_TYPE_INVALID	-1

#define I2C_WRITE 0
#define I2C_READ  1

#define MAX_DETECTED_SENSORS 4

struct i2c_trans {
	uint32_t addr;
	uint32_t r_w;
	uint32_t data;
	uint32_t datalen;
};

uint8_t *sclk_name;

typedef struct SENSOR_INFO_S
{
	uint8_t *name;         // Sensor name
	uint8_t i2c_addr;      // I2C address
	uint8_t *mclk_name;    // Clock source
	uint32_t clk;          // Clock frequency

	uint32_t id_value[8];  // Sensor identification values
	uint32_t id_value_len; // Length of id_value
	uint32_t id_addr[8];   // Sensor configuration values
	uint32_t id_addr_len;  // Length of id_addr
	uint8_t id_cnt;        // Additional counter or length parameter

	struct i2c_adapter *adap; // I2C adapter pointer: Points to the I2C adapter structure

} SENSOR_INFO_T, *SENSOR_INFO_P;

/*  Example entry
{
    "sensor_name",   // Name of the sensor (e.g., "gc1024")
    i2c_address,     // I2C address of the sensor (e.g., 0x3c)
    "clock_name",    // Name of the clock source (e.g., "cgu_cim")
    clock_frequency, // Clock frequency in Hz (e.g., 24000000 for 24 MHz)
    {id_val1, id_val2}, // Array of sensor identification values (e.g., {0x10, 0x04})
    id_val_len,      // Length of the id_value array (number of elements, e.g., 1)
    {reg_addr1, reg_addr2}, // Array of register addresses for sensor configuration (e.g., {0xf0, 0xf1})
    reg_addr_len,    // Length of the id_addr array (number of elements, e.g., 1)
    additional_count, // Additional counter or length parameter (e.g., 2)
    pointer          // Pointer to additional data or NULL if not used
}
*/

SENSOR_INFO_T g_sinfo[] =
{
	{"ar0130", 0x10,  "cgu_cim", 24000000, {0x2402}, 2, {0x3000}, 2, 1, NULL},
	{"ar0237", 0x10,  "cgu_cim", 27000000, {0x0256}, 2, {0x3000}, 2, 1, NULL},
	{"bf3115", 0x6e,  "cgu_cim", 24000000, {0x31, 0x16}, 1, {0xfc, 0xfd}, 1, 2, NULL},
	{"bg0806", 0x32,  "cgu_cim", 24000000, {0x08, 0x06}, 1, {0x0000, 0x0001}, 2, 2, NULL},
	{"c23a98", 0x36,  "cgu_cim", 24000000, {0x23, 0x98}, 1, {0x0000, 0x0001}, 2, 2, NULL},
	{"cv3001", 0x35,  "cgu_cim", 24000000, {0x01, 0x30}, 1, {0x3002, 0x3003}, 2, 2, NULL},
	{"cv4001", 0x35,  "cgu_cim", 24000000, {0x01, 0x40}, 1, {0x3002, 0x3003}, 2, 2, NULL},
	{"cv5001", 0x35,  "cgu_cim", 24000000, {0x01, 0x50}, 1, {0x3002, 0x3003}, 2, 2, NULL},
	{"gc1024", 0x3c,  "cgu_cim", 24000000, {0x10, 0x04}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc1034", 0x21,  "cgu_cim", 24000000, {0x10, 0x34}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc1064", 0x3c,  "cgu_cim", 24000000, {0x10, 0x24}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc2023", 0x37,  "cgu_cim", 24000000, {0x20, 0x23}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc2033", 0x37,  "cgu_cim", 24000000, {0x20, 0x33}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc2083", 0x37,  "cgu_cim", 24000000, {0x20, 0x83}, 1, {0x3f0, 0x3f1}, 2, 2, NULL},
	{"gc2607", 0x37,  "cgu_cim", 24000000, {0x26, 0x07}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"gc3003", 0x37,  "cgu_cim", 27000000, {0x30, 0x03}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"gc3003a", 0x37,  "cgu_cim", 27000000, {0x30, 0x03}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"gc4023", 0x21,  "cgu_cim", 24000000, {0x40, 0x23}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"gc4023", 0x29,  "cgu_cim", 24000000, {0x40, 0x23}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"gc5603", 0x31,  "cgu_cim", 24000000, {0x56, 0x03}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"gc5613", 0x31,  "cgu_cim", 24000000, {0x56, 0x03}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"gc8613", 0x31,  "cgu_cim", 27000000, {0x86, 0x13}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"hm2131", 0x24,  "cgu_cim", 24000000, {0x14, 0x0a}, 1, {0x0000, 0x0001}, 2, 2, NULL},
	{"hm2140", 0x24,  "cgu_cim", 24000000, {0x21, 0x40}, 1, {0x0000, 0x0001}, 2, 2, NULL},
	// Several Sony sensors share identical ID's, for now, lets leave the imx307 enabled and disable the others.
	//{"imx219", 0x1a,  "div_cim", 24000000, {0xA0, 0xB2}, 1, {0x3008, 0x301e}, 2, 2, NULL},
	{"imx225", 0x1a,  "cgu_cim", 24000000, {0x10, 0x01}, 1, {0x3004, 0x3013}, 2, 2, NULL},
	//{"imx291", 0x1a,  "cgu_cim", 37125000, {0xA0, 0xB2}, 1, {0x3008, 0x301e}, 2, 2, NULL},
	{"imx307", 0x1a,  "cgu_cim", 37125000, {0xA0, 0xB2}, 1, {0x3008, 0x301e}, 2, 2, NULL},
	{"imx322", 0x1a,  "cgu_cim", 37125000, {0x50, 0x0}, 1, {0x301c, 0x301d}, 2, 2, NULL},
	{"imx323", 0x1a,  "cgu_cim", 37125000, {0x50, 0x0}, 1, {0x301c, 0x301d}, 2, 2, NULL},
	{"imx327", 0x1a,  "cgu_cim", 37125000, {0xB2, 0x01}, 1, {0x301e, 0x301f}, 2, 2, NULL},
	{"imx334", 0x1a,  "cgu_cim", 37125000, {0x20, 0x03}, 1, {0x302e, 0x302f}, 2, 2, NULL},
	{"imx335", 0x1a,  "cgu_cim", 37125000, {0x08, 0x0}, 1, {0x302e, 0x302f}, 2, 2, NULL},
	{"imx355", 0x1a,  "cgu_cim", 37125000, {0x38, 0x0A}, 1, {0x302e, 0x302f}, 2, 2, NULL},
	{"imx363", 0x1a,  "cgu_cim", 24000000, {0x02, 0x60}, 1, {0x0016, 0x0017}, 2, 2, NULL},
	{"imx378", 0x1a,  "cgu_cim", 27000000, {0x03, 0x78}, 1, {0x0016, 0x0017}, 2, 2, NULL},
	{"imx386", 0x1a,  "cgu_cim", 24000000, {0x28, 0x23}, 1, {0x0016, 0x0017}, 2, 2, NULL},
	{"imx415", 0x1a,  "cgu_cim", 37125000, {0x28, 0x23}, 1, {0x3b00, 0x3b06}, 2, 2, NULL},
	{"imx482", 0x1a,  "cgu_cim", 37125000, {0x4c, 0x01}, 1, {0x3A42, 0x3A43}, 2, 2, NULL},
	{"imx515", 0x1a,  "cgu_cim", 37125000, {0x28, 0x23}, 1, {0x3B00, 0x3B06}, 2, 2, NULL},
	{"jxf22",  0x40,  "cgu_cim", 24000000, {0x0f, 0x22}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxf23",  0x40,  "cgu_cim", 24000000, {0x0f, 0x23}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxf23s", 0x40,  "cgu_cim", 24000000, {0x0f, 0x23}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxf28",  0x40,  "cgu_cim", 24000000, {0x0f, 0x28}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxf37",  0x40,  "cgu_cim", 24000000, {0x0f, 0x37}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxf37pa", 0x40,  "cgu_cim", 24000000, {0x08, 0x41}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxf38p", 0x40,  "cgu_cim", 24000000, {0x08, 0x44}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxh42",  0x30,  "cgu_cim", 24000000, {0xa0, 0x42, 0x81}, 1, {0xa, 0xb, 0x9}, 1, 3, NULL},
	{"jxf51",  0x40,  "cgu_cim", 24000000, {0x0f, 0x51}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxh61",  0x30,  "cgu_cim", 24000000, {0xa0, 0x42, 0x3}, 1, {0xa, 0xb, 0x9}, 1, 3, NULL},
	{"jxh62",  0x30,  "cgu_cim", 24000000, {0xa0, 0x62}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxh63",  0x40,  "cgu_cim", 24000000, {0x0a, 0x63}, 1, {0x0a, 0x0b}, 1, 2, NULL},
	{"jxh63p",  0x40,  "cgu_cim", 27000000, {0x08, 0x48}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxh65",  0x30,  "cgu_cim", 24000000, {0x0a, 0x65}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxk02",  0x40,  "cgu_cim", 24000000, {0x04, 0x03}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxk03",  0x40,  "cgu_cim", 24000000, {0x05, 0x03}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxk04",  0x40,  "cgu_cim", 24000000, {0x04, 0x04}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxk05",  0x40,  "cgu_cim", 24000000, {0x05, 0x05}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxk06",  0x40,  "cgu_cim", 24000000, {0x08, 0x52}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxk08",  0x40,  "cgu_cim", 24000000, {0x06, 0x05}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxq03",  0x40,  "cgu_cim", 24000000, {0x05, 0x07}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxq03p",  0x40,  "cgu_cim", 24000000, {0x08, 0x43}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxv01",  0x21,  "cgu_cim", 27000000, {0x0e, 0x04}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"mis2003", 0x30,  "cgu_cim", 24000000, {0x20, 0x03}, 1, {0x3000, 0x3001}, 2, 2, NULL},
	{"mis2008", 0x30,  "cgu_cim", 24000000, {0x20, 0x08}, 1, {0x3000, 0x3001}, 2, 2, NULL},
	{"mis2009",  0x30,  "cgu_cim", 24000000, {0x20, 0x08}, 1, {0x3000, 0x3001}, 2, 2, NULL},
	{"mis4001",  0x30,  "cgu_cim", 27000000, {0x13, 0x11}, 1, {0x3000, 0x3001}, 2, 2, NULL},
	{"os02b10", 0x3c,  "cgu_cim", 24000000, {0x23, 0x08}, 1, {0x02, 0x03}, 1, 2, NULL},
	{"os02g10", 0x3c,  "cgu_cim", 24000000, {0x56, 0x02, 0x47}, 1, {0x02, 0x03, 0x04}, 1, 3, NULL},
	{"os03b10", 0x3c,  "cgu_cim", 24000000, {0x53, 0x03, 0x42}, 1, {0x02, 0x03, 0x04}, 1, 3, NULL},
	{"os04a10",  0x36,  "cgu_cim", 24000000, {0x53, 0x04, 0x41}, 1, {0x300a, 0x300b, 0x300c}, 2, 3, NULL},
	{"os04b10", 0x3c,  "cgu_cim", 24000000, {0x43, 0x08, 0x01}, 1, {0x02, 0x03, 0x04}, 1, 3, NULL},
	{"os04c10",  0x36,  "cgu_cim", 24000000, {0x53, 0x04}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"os05a10", 0x36,  "cgu_cim", 24000000, {0x53, 0x05, 0x41}, 1, {0x300a, 0x300b, 0x300c}, 2, 3, NULL},
	{"os08a10",  0x36,  "cgu_cim", 24000000, {0x53, 0x08, 0x41}, 1, {0x300a, 0x300b, 0x300c}, 2, 3, NULL},
	{"ov2710", 0x36,  "cgu_cim", 24000000, {0x27, 0x10}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ov2718", 0x36,  "cgu_cim", 24000000, {0x27, 0x70}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ov2732", 0x36,  "cgu_cim", 24000000, {0x00, 0x27, 0x32}, 1, {0x300a, 0x300b, 0x300c}, 2, 3, NULL},
	{"ov2735", 0x3c,  "cgu_cim", 24000000, {0x27, 0x35, 0x05}, 1, {0x02, 0x03, 0x04}, 1, 3, NULL},
	{"ov2735b", 0x3c,  "cgu_cim", 24000000, {0x27, 0x35, 0x6, 0x7}, 1, {0x02, 0x03, 0x04, 0x04}, 1, 4, NULL},
	{"ov4689", 0x36,  "cgu_cim", 24000000, {0x46, 0x88}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ov5647", 0x36,  "cgu_cim", 24000000, {0x56, 0x47}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ov5648", 0x36,  "cgu_cim", 24000000, {0x56, 0x48}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ov7740", 0x21,  "cgu_cim", 24000000, {0x77, 0x42}, 1, {0x0a, 0x0b}, 1, 2, NULL},
	{"ov9712", 0x30,  "cgu_cim", 24000000, {0x97, 0x11}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"ov9732", 0x36,  "cgu_cim", 24000000, {0x97, 0x32}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ov9750", 0x36,  "cgu_cim", 24000000, {0x97, 0x50}, 1, {0x300b, 0x300c}, 2, 2, NULL},
	{"ps5230", 0x48,  "cgu_cim", 24000000, {0x52, 0x30}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"ps5250", 0x48,  "cgu_cim", 24000000, {0x52, 0x50}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"ps5260", 0x48,  "cgu_cim", 24000000, {0x52, 0x60}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"ps5270", 0x48,  "cgu_cim", 24000000, {0x52, 0x70}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"ps5280", 0x48,  "cgu_cim", 24000000, {0x52, 0x80}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"sc031gs",  0x30,  "cgu_cim", 24000000, {0x00, 0x31}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc1035", 0x30,  "cgu_cim", 24000000, {0xf0, 0x00}, 1, {0x580b, 0x3c05}, 2, 2, NULL},
	{"sc1045", 0x30,  "cgu_cim", 24000000, {0x10, 0x45}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc1135", 0x30,  "cgu_cim", 24000000, {0x00, 0x35}, 1, {0x580b, 0x2148}, 2, 2, NULL},
	{"sc1145", 0x30,  "cgu_cim", 24000000, {0x11, 0x45}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc1235", 0x30,  "cgu_cim", 24000000, {0x12, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc1245", 0x30,  "cgu_cim", 24000000, {0x12, 0x45, 0x03}, 1, {0x3107, 0x3108, 0x3020}, 2, 3, NULL},
	{"sc1245a", 0x30,  "cgu_cim", 24000000, {0x12, 0x45, 0x02}, 1, {0x3107, 0x3108, 0x3020}, 2, 3, NULL},
	{"sc1346", 0x30,  "cgu_cim", 24000000, {0xda, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc1a4t", 0x30,  "cgu_cim", 24000000, {0x9a, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc1a4ts1", 0x32,  "cgu_cim", 24000000, {0x9a, 0x4d}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc200ai", 0x30,  "cgu_cim", 24000000, {0xcb, 0x1c}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2135", 0x30,  "cgu_cim", 24000000, {0x21, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2145", 0x30,  "cgu_cim", 24000000, {0x21, 0x45}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2230", 0x30,  "cgu_cim", 24000000, {0x22, 0x32, 0x20}, 1, {0x3107, 0x3108, 0x3109}, 2, 3, NULL},
	{"sc2232", 0x30,  "cgu_cim", 24000000, {0x22, 0x32, 0x01}, 1, {0x3107, 0x3108, 0x3109}, 2, 3, NULL},
	{"sc2232h", 0x30,  "cgu_cim", 24000000, {0xcb, 0x07, 0x01}, 1, {0x3107, 0x3108, 0x3109}, 2, 3, NULL},
	{"sc2235", 0x30,  "cgu_cim", 24000000, {0x22, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc233a",  0x30,  "cgu_cim", 24000000, {0xcb, 0x3e}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2300", 0x30,  "cgu_cim", 24000000, {0x23, 0x00}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2310", 0x30,  "cgu_cim", 24000000, {0x23, 0x11}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2331s1", 0x32,  "cgu_cim", 24000000, {0xcb, 0x17}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2332", 0x30,  "cgu_cim", 24000000, {0xcb, 0x17}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2335", 0x30,  "cgu_cim", 24000000, {0xcb, 0x14}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2336", 0x30,  "cgu_cim", 24000000, {0xcb, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2336p", 0x30,  "cgu_cim", 24000000, {0x9b, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2336s1", 0x32,  "cgu_cim", 24000000, {0xcb, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc2336ps1", 0x32,  "cgu_cim", 24000000, {0x9b, 0x3a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc3035", 0x30,  "cgu_cim", 24000000, {0x30, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc301iot", 0x30,  "cgu_cim", 24000000, {0xcc, 0x40}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc3235", 0x30,  "cgu_cim", 24000000, {0xcc, 0x05}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc3335", 0x30,  "cgu_cim", 24000000, {0xcc, 0x1a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc3338", 0x30,  "cgu_cim", 24000000, {0xcc, 0x41}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc401ai", 0x30,  "cgu_cim", 24000000, {0xcd, 0x2e}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc4236", 0x30,  "cgu_cim", 24000000, {0x32, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc4335", 0x30,  "cgu_cim", 27000000, {0xcd, 0x01}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc450ai",  0x30,  "cgu_cim", 24000000, {0xbd, 0x2f}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc500ai", 0x30,  "cgu_cim", 24000000, {0xce, 0x1f}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc501ai", 0x30,  "cgu_cim", 24000000, {0xce, 0x1f}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc5235", 0x30,  "cgu_cim", 24000000, {0x52, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc5239", 0x30,  "cgu_cim", 24000000, {0x52, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc5336", 0x30,  "cgu_cim", 27000000, {0xce, 0x50}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc5338", 0x30,  "cgu_cim", 27000000, {0xce, 0x50}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc830ai", 0x30,  "cgu_cim", 27000000, {0xc1, 0x43}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc8238", 0x30,  "cgu_cim", 24000000, {0x82, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sp1409", 0x34,  "cgu_cim", 24000000, {0x14, 0x09}, 1, {0x04, 0x05}, 1, 2, NULL},
	{"sp140a", 0x3c,  "cgu_cim", 24000000, {0x14, 0x0a}, 1, {0x02, 0x03}, 1, 2, NULL},
#ifdef CONFIG_SOC_T41
	{"gc2063", 0x37,  "cgu_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc4663", 0x29,  "cgu_cim", 24000000, {0x46, 0x53}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
#else
	{"gc2053", 0x37,  "cgu_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc4653", 0x29,  "cgu_cim", 24000000, {0x46, 0x53}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
#endif
};

static int8_t g_sensor_id = -1;
static int8_t g_sensor_ids[MAX_DETECTED_SENSORS];
static int8_t g_num_detected_sensors = 0;
static struct mutex g_mutex;

int sensor_read(SENSOR_INFO_P sinfo, struct i2c_adapter *adap, uint32_t addr, uint32_t *value)
{
	int ret;
	uint8_t buf[4] = {0};
	uint8_t data[4] = {0};

	uint8_t rlen = sinfo->id_value_len;
	uint8_t wlen = sinfo->id_addr_len;
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= sinfo->i2c_addr,
			.flags	= 0,
			.len	= wlen,
			.buf	= buf,
		},
		[1] = {
			.addr	= sinfo->i2c_addr,
			.flags	= I2C_M_RD,
			.len	= rlen,
			.buf	= data,
		}
	};

	if (1 == wlen) {
		buf[0] = addr&0xff;
	} else if (2 == wlen){
		buf[0] = (addr>>8)&0xff;
		buf[1] = addr&0xff;
	} else if (3 == wlen){
		buf[0] = (addr>>16)&0xff;
		buf[1] = (addr>>8)&0xff;
		buf[2] = addr&0xff;
	} else if (4 == wlen){
		buf[0] = (addr>>24)&0xff;
		buf[1] = (addr>>16)&0xff;
		buf[2] = (addr>>8)&0xff;
		buf[3] = addr&0xff;
	} else {
		printk("sinfo: [Error] Function: %s, Line: %d, Invalid write length (wlen): %d\n", __func__, __LINE__, wlen);
	}
	ret = i2c_transfer(adap, msg, 2);
	if (ret > 0) ret = 0;
	if (0 != ret)
		printk("sinfo: [Error] Function: %s, Line: %d, I2C transfer failed with return code: %d\n", __func__, __LINE__, ret);
	if (1 == rlen) {
		*value = data[0];
	} else if (2 == rlen){
		*value = (data[0]<<8)|data[1];
	} else if (3 == rlen){
		*value = (data[0]<<16)|(data[1]<<8)|data[2];
	} else if (4 == rlen){
		*value = (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
	} else {
		printk("sinfo: [Error] Function: %s, Line: %d, Invalid read length (rlen): %d\n", __func__, __LINE__, rlen);
	}
	printk("sinfo: Read from address: 0x%x, Value: 0x%x\n", addr, *value);
	return ret;
}

static int32_t process_one_adapter(struct device *dev, void *data)
{
	int32_t ret;
	int32_t i = 0;
	int32_t j = 0;
	struct clk *sclk;
	struct clk *mclk;
	struct i2c_adapter *adap;
	uint8_t scnt = sizeof(g_sinfo)/sizeof(g_sinfo[0]);
	mutex_lock(&g_mutex);
	if (dev->type != &i2c_adapter_type) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	adap = to_i2c_adapter(dev);
	printk("sinfo: Processing I2C bus: %s, Number: %d\n", adap->name, adap->nr);

	if (adap->nr != i2c_adapter_nr) {
		mutex_unlock(&g_mutex);
		return 0;
	}

    // Reset detection counter
    g_num_detected_sensors = 0;

#ifdef CONFIG_SOC_T40
	if(cim1_gpio != -1){
		ret = gpio_request(cim1_gpio,"cim1");
		if(!ret){
			jzgpio_set_func((cim1_gpio / 32), GPIO_FUNC_1, 1 << (cim1_gpio % 32));
		}
	}
#endif
#ifdef CONFIG_SOC_T41
	if(cim_gpio != -1){
		ret = gpio_request(cim_gpio,"cim");
		if(!ret){
			jzgpio_set_func((cim_gpio / 32), GPIO_FUNC_1, 1 << (cim_gpio % 32));
		}
	}

#endif

	for (i = 0; i < scnt; i++) {
		uint8_t idcnt = g_sinfo[i].id_cnt;

#ifdef CONFIG_SOC_T40
		g_sinfo[i].mclk_name = "div_cim1";
#endif

#ifdef CONFIG_SOC_T41
#ifdef CONFIG_KERNEL_4_4_94
		sclk_name = "mux_cim";
		g_sinfo[i].mclk_name = "div_cim";
		sclk = clk_get(NULL, sclk_name);
		if (IS_ERR(sclk)) {
			printk("sinfo: [Error] Failed to get sensor input clock 'mux_cim'\n");
			mutex_unlock(&g_mutex);
			return PTR_ERR(sclk);
		}
		clk_set_rate(sclk, (unsigned long)clk_get(NULL, "vpll"));
#endif
#endif

		mclk = clk_get(NULL, g_sinfo[i].mclk_name);
		if (IS_ERR(mclk)) {
			printk("sinfo: [Error] Failed to get sensor input clock 'div_cim'\n");
			mutex_unlock(&g_mutex);
			return PTR_ERR(mclk);
		}

		clk_set_rate(mclk, g_sinfo[i].clk);

#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
		clk_prepare_enable(mclk);
#else
		clk_enable(mclk);
#endif

		if(reset_gpio != -1){
			ret = gpio_request(reset_gpio,"reset");
			if(!ret){
				gpio_direction_output(reset_gpio, 1);
				msleep(20);
				gpio_direction_output(reset_gpio, 0);
				if(strcmp(g_sinfo[i].name, "sp1409") == 0)
					msleep(600);
				else{
					msleep(20);
					gpio_direction_output(reset_gpio, 1);
					msleep(20);
				}
			}else{
				printk("sinfo: [Error] GPIO request failed for reset GPIO number: %d\n", reset_gpio);
			}
		}
		if(pwdn_gpio != -1){
			ret = gpio_request(pwdn_gpio,"pwdn");
			if(!ret){
				gpio_direction_output(pwdn_gpio, 1);
				msleep(150);
				gpio_direction_output(pwdn_gpio, 0);
				if(strcmp(g_sinfo[i].name, "sp1409") == 0)
					msleep(600);
				else
					msleep(10);
			}else{
				printk("sinfo: [Error] GPIO request failed for power down GPIO number: %d\n", pwdn_gpio);
			}
		}

		for (j = 0; j < idcnt; j++) {
			uint32_t value = 0;
			ret = sensor_read(&g_sinfo[i], adap, g_sinfo[i].id_addr[j], &value);
			if (0 != ret) {
				printk("sinfo: [Error] Failed to read sensor at address 0x%x, value read: 0x%x\n", g_sinfo[i].id_addr[j], value);
				break;
			}
			if(strcmp(g_sinfo[i].name, "ov2735b") == 0 && j == 2){
				if (value == g_sinfo[i].id_value[j])
					j++;
			}
			else
				if (value != g_sinfo[i].id_value[j])
					break;
		}

		if (-1 != reset_gpio)
			gpio_free(reset_gpio);
		if (-1 != pwdn_gpio)
			gpio_free(pwdn_gpio);
		clk_disable(mclk);
		clk_put(mclk);

		if (j == idcnt) {
			// Match!
			g_sinfo[i].adap = adap;

			// Add to detected sensors array if there's room
			if (g_num_detected_sensors < MAX_DETECTED_SENSORS) {
				g_sensor_ids[g_num_detected_sensors] = i;
				g_num_detected_sensors++;

				if (g_sinfo[i].adap) {
					printk("sinfo: Successful sensor detection: %s, I2C Bus: %d, I2C Address: 0x%X\n",
						g_sinfo[i].name, g_sinfo[i].adap->nr, g_sinfo[i].i2c_addr);
				} else {
					printk("sinfo: Successful sensor detection: %s, I2C Bus: unknown, I2C Address: 0x%X\n",
						g_sinfo[i].name, g_sinfo[i].i2c_addr);
				}
			}

			// Continue checking all other sensors (don't exit)
		}
	}

	// Set g_sensor_id to the first detected sensor (for backward compatibility)
	if (g_num_detected_sensors > 0) {
		g_sensor_id = g_sensor_ids[0];
		printk("sinfo: Total sensors detected: %d\n", g_num_detected_sensors);
	} else {
		printk("sinfo: [Info] Failed to find any sensors\n");
		g_sensor_id = -1;
	}

	mutex_unlock(&g_mutex);
	return 0;
}

static int32_t sensor_open(void)
{
	int ret = -1;
	struct clk *sclk;
	struct clk *mclk;
	if (-1 == g_sensor_id)
		return 0;

	mutex_lock(&g_mutex);
#ifdef CONFIG_SOC_T40
	g_sinfo[g_sensor_id].mclk_name = "div_cim1";
#endif

#ifdef CONFIG_SOC_T41
#ifdef CONFIG_KERNEL_4_4_94
		sclk_name = "mux_cim";
		g_sinfo[g_sensor_id].mclk_name = "div_cim";
		sclk = clk_get(NULL, sclk_name);
		if (IS_ERR(sclk)) {
			printk("sinfo: [Error] Failed to get sensor input clock 'mux_cim'\n");
			mutex_unlock(&g_mutex);
			return PTR_ERR(sclk);
		}
		clk_set_rate(sclk, (unsigned long)clk_get(NULL, "vpll"));
#endif
#endif

	mclk = clk_get(NULL, g_sinfo[g_sensor_id].mclk_name);
	if (IS_ERR(mclk)) {
		printk("sinfo: [Error] Failed to get sensor input clock 'cgu_cim'\n");
		mutex_unlock(&g_mutex);
		return PTR_ERR(mclk);
	}
	clk_set_rate(mclk, g_sinfo[g_sensor_id].clk);

#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
	clk_prepare_enable(mclk);
#else
	clk_enable(mclk);
#endif

	if(reset_gpio != -1){
		ret = gpio_request(reset_gpio,"reset");
		if(!ret){
			gpio_direction_output(reset_gpio, 1);
			msleep(20);
			gpio_direction_output(reset_gpio, 0);
			msleep(20);
			gpio_direction_output(reset_gpio, 1);
			msleep(20);
		}else{
			printk("sinfo: [Error] GPIO request failed for reset GPIO number: %d\n", reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = gpio_request(pwdn_gpio,"pwdn");
		if(!ret){
			gpio_direction_output(pwdn_gpio, 1);
			msleep(150);
			gpio_direction_output(pwdn_gpio, 0);
			msleep(10);
		}else{
			printk("sinfo: [Error] GPIO request failed for power down GPIO number: %d\n", pwdn_gpio);
		}
	}
	mutex_unlock(&g_mutex);
	return 0;
}

static int32_t sensor_release(void)
{
	struct clk *mclk;
	if (-1 == g_sensor_id)
		return 0;
	mutex_lock(&g_mutex);
	mclk = clk_get(NULL, g_sinfo[g_sensor_id].mclk_name);
	if (IS_ERR(mclk)) {
		printk("sinfo: [Error] Failed to get sensor input clock 'cgu_cim'\n");
		mutex_unlock(&g_mutex);
		return PTR_ERR(mclk);
	}
	if (-1 != reset_gpio)
		gpio_free(reset_gpio);
	if (-1 != pwdn_gpio)
		gpio_free(pwdn_gpio);
	clk_disable(mclk);
	clk_put(mclk);
	mutex_unlock(&g_mutex);
	return 0;
}

static int32_t i2c_read_write(struct device *dev, void *data)
{
	int32_t ret;
	struct i2c_adapter *adap;
	struct i2c_trans *t = data;

	uint8_t buf[4] = {0};
	uint32_t value = 0;
	uint8_t len = t->datalen;
	struct i2c_msg msg = {
			.addr	= t->addr,
			.flags	= (t->r_w == I2C_WRITE)?0:I2C_M_RD,
			.len	= len,
			.buf	= buf,
	};


	mutex_lock(&g_mutex);
	if (dev->type != &i2c_adapter_type) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	adap = to_i2c_adapter(dev);
	printk("sinfo: Processing I2C adapter: Name: %s, Number: %d\n", adap->name, adap->nr);

	if (adap->nr != i2c_adapter_nr) {
		mutex_unlock(&g_mutex);
		return 0;
	}

	if (I2C_WRITE == t->r_w) {
		if (1 == len) {
			buf[0] = (t->data)&0xff;
		} else if (2 == len){
			buf[0] = ((t->data)>>8)&0xff;
			buf[1] = (t->data)&0xff;
		} else if (3 == len){
			buf[0] = ((t->data)>>16)&0xff;
			buf[1] = ((t->data)>>8)&0xff;
			buf[2] = (t->data)&0xff;
		} else if (4 == len){
			buf[0] = ((t->data)>>24)&0xff;
			buf[1] = ((t->data)>>16)&0xff;
			buf[2] = ((t->data)>>8)&0xff;
			buf[3] = (t->data)&0xff;
		} else {
			printk("sinfo: [Error] Function: %s, Line: %d, I2C transfer failed with return code: %d\n", __func__, __LINE__, ret);
		}
	}
	ret = i2c_transfer(adap, &msg, 1);
	if (ret > 0) ret = 0;
	if (0 != ret)
		printk("sinfo: [Error] Function: %s, Line: %d, I2C transfer failed with return code: %d\n", __func__, __LINE__, ret);

	if (I2C_READ == t->r_w) {
		if (1 == len) {
			value = buf[0];
		} else if (2 == len){
			value = (buf[0]<<8)|buf[1];
		} else if (3 == len){
			value = (buf[0]<<16)|(buf[1]<<8)|buf[2];
		} else if (4 == len){
			value = (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
		} else {
			printk("sinfo: [Error] Function: %s, Line: %d, I2C transfer failed with return code: %d\n", __func__, __LINE__, ret);
		}
		printk("sinfo: I2C read from address: 0x%x, Value: 0x%x\n", t->addr, value);
	}
	mutex_unlock(&g_mutex);
	return 0;
}

static long sinfo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int32_t data;

	mutex_lock(&g_mutex);
	switch (cmd) {
	case IOCTL_SINFO_GET:
		if (-1 == g_sensor_id)
			data = -1;
		else
			data = g_sensor_id;
		if (copy_to_user((void *)arg, &data, sizeof(data))) {
			printk("sinfo: [Error] copy_from_user failed in IOCTL operation\n");
			ret = -EFAULT;
			break;
		}
		break;
	case IOCTL_SINFO_FLASH:
		i2c_for_each_dev(NULL, process_one_adapter);
		break;
	default:
		printk("sinfo: [Error] Invalid IOCTL command received: 0x%08x\n", cmd);
		ret = -EINVAL;
	}
	mutex_unlock(&g_mutex);
	return ret;
}

static int sinfo_open(struct inode *inode, struct file *filp)
{
	i2c_for_each_dev(NULL, process_one_adapter);
	return 0;
}

static int sinfo_release(struct inode *inode, struct file *filp)
{
	printk("sinfo: Sensor release function invoked\n");
	sensor_release();
	return 0;
}

static ssize_t sinfo_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static struct file_operations sinfo_fops =
{
	.owner = THIS_MODULE,
	.read = sinfo_read,
	.unlocked_ioctl = sinfo_ioctl,
	.open = sinfo_open,
	.release = sinfo_release,
};

static struct miscdevice misc_sinfo = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sinfo",
	.fops = &sinfo_fops,
};

static int sinfo_proc_show(struct seq_file *m, void *v)
{
	int i, j;

	if (g_num_detected_sensors == 0) {
		seq_printf(m, "No sensors found\n");
	} else {
		seq_printf(m, "Detected sensors (%d):\n", g_num_detected_sensors);
		for (i = 0; i < g_num_detected_sensors; i++) {
			int8_t id = g_sensor_ids[i];

			// Display basic sensor info
			if (g_sinfo[id].adap) {
				seq_printf(m, "%d: %s (I2C Bus: %d, Address: 0x%X)\n",
						i+1, g_sinfo[id].name, g_sinfo[id].adap->nr, g_sinfo[id].i2c_addr);
			} else {
				seq_printf(m, "%d: %s (I2C Bus: unknown, Address: 0x%X)\n",
						i+1, g_sinfo[id].name, g_sinfo[id].i2c_addr);
			}

			// Display ID values
			seq_printf(m, "   ID Values: ");
			for (j = 0; j < g_sinfo[id].id_cnt; j++) {
				seq_printf(m, "0x%X ", g_sinfo[id].id_value[j]);
			}
			seq_printf(m, "\n");

			// Display ID registers
			seq_printf(m, "   ID Registers: ");
			for (j = 0; j < g_sinfo[id].id_cnt; j++) {
				seq_printf(m, "0x%X ", g_sinfo[id].id_addr[j]);
			}
			seq_printf(m, "\n");
		}

		seq_printf(m, "Primary sensor: %s\n", g_sinfo[g_sensor_id].name);
	}
	return 0;
}

static int sinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sinfo_proc_show, NULL);
}

ssize_t sinfo_proc_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
	int ret = 0;
	char cmd[100] = {0};
	uint32_t addr,data,datalen;

	if (len > 100) {
		printk("sinfo: [Error] Command too long in proc write\n");
		return -EFAULT;
	}
	if(copy_from_user(cmd, buf, len))
	{
		return -EFAULT;
	}
	/* probe sensor */
	if (!strncmp(cmd, "1", strlen("1"))) {
		i2c_for_each_dev(NULL, process_one_adapter);
	/* probe sensor */
	} else if (!strncmp(cmd, "probe", strlen("probe"))) {
		i2c_for_each_dev(NULL, process_one_adapter);
	/* sensor open/release i2c read/write
	 * open: set sensor clk,reset
	 * release: free clk,reset gpio
	 *
	 * example: sc2135
	 *
	 * echo open:sc2135 > /proc/jz/sinfo/info
	 * echo i2c-w:0x30-0x3017-2 > /proc/jz/sinfo/info
	 * echo i2c-r:0x30-1 > /proc/jz/sinfo/info
	 *
	 * */
	} else if (!strncmp(cmd, "i2c-w:", strlen("i2c-w:"))) {
		ret = sscanf(cmd, "i2c-w:%i-%i-%i", &addr, &data, &datalen);
		if (3 != ret) {
			printk("sinfo: [Error] Command parsing error: %s\n", cmd);
			return len;
		} else {
			struct i2c_trans t = {addr, I2C_WRITE, data, datalen};
			printk("sinfo: I2C write command: Address: 0x%x, Data: 0x%x, Data Length: %d\n", addr, data, datalen);
			i2c_for_each_dev(&t, i2c_read_write);
		}
	} else if (!strncmp(cmd, "i2c-r:", strlen("i2c-r:"))) {
		ret = sscanf(cmd, "i2c-r:%i-%i", &addr, &datalen);
		if (2 != ret) {
			printk("sinfo: [Error] Command parsing error: %s\n", cmd);
			return len;
		} else {
			struct i2c_trans t = {addr, I2C_READ, 0, datalen};
			printk("sinfo: I2C read command: Address: 0x%x, Data Length: %d\n", addr, datalen);
			i2c_for_each_dev(&t, i2c_read_write);
		}
	} else if (!strncmp(cmd, "open", strlen("open"))) {
		int i = 0;
		char s[20] = {0};
		ret = sscanf(cmd, "open:%s", s);
		if (1 != ret) {
			printk("sinfo: [Error] Command parsing error: %s\n", cmd);
			return len;
		} else {
			uint8_t scnt = sizeof(g_sinfo)/sizeof(g_sinfo[0]);
			for (i = 0; i < scnt; i++) {
				if (!strcmp(s, g_sinfo[i].name)) {
					g_sensor_id = i;
					break;
				}
			}
			if (i >= scnt) {
				printk("sinfo: [Error] Sensor '%s' not found, command: %s\n", s, cmd);
				return len;
			}
			sensor_open();
		}
	} else if (!strncmp(cmd, "release", strlen("release"))) {
		sensor_release();
	} else {
		printk("sinfo: [Error] Command not supported\n");
	}
	return len;
}

static const struct file_operations sinfo_proc_fops = {
	.owner = THIS_MODULE,
	.open = sinfo_proc_open,
	.read = seq_read,
	.write = sinfo_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

struct proc_dir_entry *g_sinfo_proc;
static __init int init_sinfo(void)
{
	int ret = 0;
	mutex_init(&g_mutex);
#ifdef CONFIG_SOC_T21
	*(volatile unsigned int*)(0xB0010104) = 0x1;
#endif
	g_sinfo_proc = proc_mkdir("jz/sinfo", 0);
	if (!g_sinfo_proc) {
		printk("sinfo: [Error] Failed to create proc directory 'jz/sinfo'\n");
	}
	proc_create_data("info", S_IRUGO, g_sinfo_proc, &sinfo_proc_fops, NULL);
	/* i2c_for_each_dev(NULL, process_one_adapter); */
	ret = misc_register(&misc_sinfo);
	/* printk("##### g_sensor_id = %d\n", g_sensor_id); */
	return ret;

}

static __exit void exit_sinfo(void)
{
	proc_remove(g_sinfo_proc);
	misc_deregister(&misc_sinfo);
}

module_init(init_sinfo);
module_exit(exit_sinfo);

MODULE_DESCRIPTION("Ingenic T-Series Sensor Info Driver");
MODULE_LICENSE("GPL");
