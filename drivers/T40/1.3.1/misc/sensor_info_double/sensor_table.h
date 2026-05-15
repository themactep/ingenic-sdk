#ifndef __SENSOR_TABLE__
#define __SENSOR_TABLE__

#define SENSOR_INFO_IOC_MAGIC  'S'
#define IOCTL_SINFO_GET			_IO(SENSOR_INFO_IOC_MAGIC, 100)
#define IOCTL_SINFO_FLASH		_IO(SENSOR_INFO_IOC_MAGIC, 101)

#define SENSOR_TYPE_INVALID	-1

#define I2C_WRITE 0
#define I2C_READ  1

struct i2c_trans {
	uint32_t addr;
	uint32_t r_w;
	uint32_t data;
	uint32_t datalen;
};

typedef struct SENSOR_INFO_S
{
	uint8_t *name;
	uint8_t i2c_addr;
	uint8_t *clk_name;
	uint32_t clk;

	uint32_t id_value[8];
	uint32_t id_value_len;
	uint32_t id_addr[8];
	uint32_t id_addr_len;
	uint8_t id_cnt;

	struct i2c_adapter *adap;
} SENSOR_INFO_T, *SENSOR_INFO_P;

enum SENSOR_TYPE
{
	SENSOR_TYPE_OV9712=0,
	SENSOR_TYPE_OV9732,
	SENSOR_TYPE_OV9750,
	SENSOR_TYPE_JXH42,
	SENSOR_TYPE_SC1035,
	SENSOR_TYPE_SC1135,
	SENSOR_TYPE_SC1045,
	SENSOR_TYPE_SC1145,
	SENSOR_TYPE_AR0130,
	SENSOR_TYPE_JXH61,
	SENSOR_TYPE_GC1024,
	SENSOR_TYPE_GC1064,
	SENSOR_TYPE_GC2023,
	SENSOR_TYPE_BF3115,
	SENSOR_TYPE_IMX225,
	SENSOR_TYPE_OV2710,
	SENSOR_TYPE_IMX323,
	SENSOR_TYPE_SC2135,
	SENSOR_TYPE_SP1409,
	SENSOR_TYPE_JXH62,
	SENSOR_TYPE_BG0806,
	SENSOR_TYPE_OV4689,
	SENSOR_TYPE_JXF22,
	SENSOR_TYPE_IMX322,
	SENSOR_TYPE_IMX291,
	SENSOR_TYPE_OV2735,
	SENSOR_TYPE_SC3035,
	SENSOR_TYPE_AR0237,
	SENSOR_TYPE_SC2145,
	SENSOR_TYPE_JXH65,
	SENSOR_TYPE_SC2300,
	SENSOR_TYPE_OV2735B,
	SENSOR_TYPE_JXV01,
	SENSOR_TYPE_PS5230,
	SENSOR_TYPE_PS5250,
	SENSOR_TYPE_OV2718,
	SENSOR_TYPE_OV2732,
	SENSOR_TYPE_SC2235,
	SENSOR_TYPE_JXK02,
	SENSOR_TYPE_OV7740,
	SENSOR_TYPE_HM2140,
	SENSOR_TYPE_GC2033,
	SENSOR_TYPE_JXF28,
	SENSOR_TYPE_OS02B10,
	SENSOR_TYPE_OS05A10,
	SENSOR_TYPE_SC2232,
	SENSOR_TYPE_SC2232H,
	SENSOR_TYPE_SC2230,
	SENSOR_TYPE_SC4236,
	SENSOR_TYPE_SC1245,
	SENSOR_TYPE_SC1245A,
	SENSOR_TYPE_GC1034,
	SENSOR_TYPE_SC1235,
	SENSOR_TYPE_JXF23,
	SENSOR_TYPE_PS5270,
	SENSOR_TYPE_SP140A,
	SENSOR_TYPE_SC2310,
	SENSOR_TYPE_HM2131,
	SENSOR_TYPE_MIS2003,
	SENSOR_TYPE_JXK03,
	SENSOR_TYPE_SC5235,
	SENSOR_TYPE_OV5648,
	SENSOR_TYPE_PS5280,
	SENSOR_TYPE_JXF23S,
	SENSOR_TYPE_GC2053,
	SENSOR_TYPE_SC4335,
	SENSOR_TYPE_PS5260,
	SENSOR_TYPE_OS04B10,
	SENSOR_TYPE_JXK05,
	SENSOR_TYPE_JXH63,
	SENSOR_TYPE_SC2335,
	SENSOR_TYPE_JXF37,
	SENSOR_TYPE_GC4653,
	SENSOR_TYPE_C23A98,
	SENSOR_TYPE_SC3335,
	SENSOR_TYPE_SC3235,
};

SENSOR_INFO_T g_sinfo[] =
{
	{"ov9712", 0x30,  "cgu_cim", 24000000, {0x97, 0x11}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"ov9732", 0x36,  "cgu_cim", 24000000, {0x97, 0x32}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ov9750", 0x36,  "cgu_cim", 24000000, {0x97, 0x50}, 1, {0x300b, 0x300c}, 2, 2, NULL},
	{"jxh42",  0x30,  "cgu_cim", 24000000, {0xa0, 0x42, 0x81}, 1, {0xa, 0xb, 0x9}, 1, 3, NULL},
	{"sc1035", 0x30,  "cgu_cim", 24000000, {0xf0, 0x00}, 1, {0x580b, 0x3c05}, 2, 2, NULL},
	{"sc1135", 0x30,  "cgu_cim", 24000000, {0x00, 0x35}, 1, {0x580b, 0x2148}, 2, 2, NULL},
	{"sc1045", 0x30,  "cgu_cim", 24000000, {0x10, 0x45}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc1145", 0x30,  "cgu_cim", 24000000, {0x11, 0x45}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"ar0130", 0x10,  "cgu_cim", 24000000, {0x2402}, 2, {0x3000}, 2, 1, NULL},
	{"jxh61",  0x30,  "cgu_cim", 24000000, {0xa0, 0x42, 0x3}, 1, {0xa, 0xb, 0x9}, 1, 3, NULL},
	{"gc1024", 0x3c,  "cgu_cim", 24000000, {0x10, 0x04}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc1064", 0x3c,  "cgu_cim", 24000000, {0x10, 0x24}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"gc2023", 0x37,  "cgu_cim", 24000000, {0x20, 0x23}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"bf3115", 0x6e,  "cgu_cim", 24000000, {0x31, 0x16}, 1, {0xfc, 0xfd}, 1, 2, NULL},
	{"imx225", 0x1a,  "cgu_cim", 24000000, {0x10, 0x01}, 1, {0x3004, 0x3013}, 2, 2, NULL},
	{"ov2710", 0x36,  "cgu_cim", 24000000, {0x27, 0x10}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"imx323", 0x1a,  "cgu_cim", 37125000, {0x50, 0x0}, 1, {0x301c, 0x301d}, 2, 2, NULL},
	{"sc2135", 0x30,  "cgu_cim", 24000000, {0x21, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sp1409", 0x34,  "cgu_cim", 24000000, {0x14, 0x09}, 1, {0x04, 0x05}, 1, 2, NULL},
	{"jxh62",  0x30,  "cgu_cim", 24000000, {0xa0, 0x62}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"bg0806", 0x32,  "cgu_cim", 24000000, {0x08, 0x06}, 1, {0x0000, 0x0001}, 2, 2, NULL},
	{"ov4689", 0x36,  "cgu_cim", 24000000, {0x46, 0x88}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"jxf22",  0x40,  "cgu_cim", 24000000, {0x0f, 0x22}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"imx322", 0x1a,  "cgu_cim", 37125000, {0x50, 0x0}, 1, {0x301c, 0x301d}, 2, 2, NULL},
	{"imx291", 0x1a,  "cgu_cim", 37125000, {0xA0, 0xB2}, 1, {0x3008, 0x301e}, 2, 2, NULL},
	{"ov2735", 0x3c,  "cgu_cim", 24000000, {0x27, 0x35, 0x05}, 1, {0x02, 0x03, 0x04}, 1, 3, NULL},
	{"sc3035", 0x30,  "cgu_cim", 24000000, {0x30, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"ar0237", 0x10,  "cgu_cim", 27000000, {0x0256}, 2, {0x3000}, 2, 1, NULL},
	{"sc2145", 0x30,  "cgu_cim", 24000000, {0x21, 0x45}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"jxh65",  0x30,  "cgu_cim", 24000000, {0x0a, 0x65}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"sc2300", 0x30,  "cgu_cim", 24000000, {0x23, 0x00}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"ov2735b", 0x3c,  "cgu_cim", 24000000, {0x27, 0x35, 0x6, 0x7}, 1, {0x02, 0x03, 0x04, 0x04}, 1, 4, NULL},
	{"jxv01",  0x21,  "cgu_cim", 27000000, {0x0e, 0x04}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"ps5230", 0x48,  "cgu_cim", 24000000, {0x52, 0x30}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"ps5250", 0x48,  "cgu_cim", 24000000, {0x52, 0x50}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"ov2718", 0x36,  "cgu_cim", 24000000, {0x27, 0x70}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ov2732", 0x36,  "cgu_cim", 24000000, {0x00, 0x27, 0x32}, 1, {0x300a, 0x300b, 0x300c}, 2, 3, NULL},
	{"sc2235", 0x30,  "cgu_cim", 24000000, {0x22, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"jxk02",  0x40,  "cgu_cim", 24000000, {0x04, 0x03}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"ov7740", 0x21,  "cgu_cim", 24000000, {0x77, 0x42}, 1, {0x0a, 0x0b}, 1, 2, NULL},
	{"hm2140", 0x24,  "cgu_cim", 24000000, {0x21, 0x40}, 1, {0x0000, 0x0001}, 2, 2, NULL},
	{"gc2033", 0x37,  "cgu_cim", 24000000, {0x20, 0x33}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"jxf28",  0x40,  "cgu_cim", 24000000, {0x0f, 0x28}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"os02b10", 0x3c,  "cgu_cim", 24000000, {0x23, 0x08}, 1, {0x02, 0x03}, 1, 2, NULL},
	{"os05a10", 0x36,  "cgu_cim", 24000000, {0x53, 0x05, 0x41}, 1, {0x300a, 0x300b, 0x300c}, 2, 3, NULL},
	{"sc2232", 0x30,  "cgu_cim", 24000000, {0x22, 0x32, 0x01}, 1, {0x3107, 0x3108, 0x3109}, 2, 3, NULL},
	{"sc2232h", 0x30,  "cgu_cim", 24000000, {0xcb, 0x07, 0x01}, 1, {0x3107, 0x3108, 0x3109}, 2, 3, NULL},
	{"sc2230", 0x30,  "cgu_cim", 24000000, {0x22, 0x32, 0x20}, 1, {0x3107, 0x3108, 0x3109}, 2, 3, NULL},
	{"sc4236", 0x30,  "cgu_cim", 24000000, {0x32, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc1245", 0x30,  "cgu_cim", 24000000, {0x12, 0x45, 0x03}, 1, {0x3107, 0x3108, 0x3020}, 2, 3, NULL},
	{"sc1245a", 0x30,  "cgu_cim", 24000000, {0x12, 0x45, 0x02}, 1, {0x3107, 0x3108, 0x3020}, 2, 3, NULL},
	{"gc1034", 0x21,  "cgu_cim", 24000000, {0x10, 0x34}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"sc1235", 0x30,  "cgu_cim", 24000000, {0x12, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"jxf23",  0x40,  "cgu_cim", 24000000, {0x0f, 0x23}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"ps5270", 0x48,  "cgu_cim", 24000000, {0x52, 0x70}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"sp140a", 0x3c,  "cgu_cim", 24000000, {0x14, 0x0a}, 1, {0x02, 0x03}, 1, 2, NULL},
	{"sc2310", 0x30,  "cgu_cim", 24000000, {0x23, 0x11}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"hm2131", 0x24,  "cgu_cim", 24000000, {0x14, 0x0a}, 1, {0x0000, 0x0001}, 2, 2, NULL},
	{"mis2003", 0x30,  "cgu_cim", 24000000, {0x20, 0x03}, 1, {0x3000, 0x3001}, 2, 2, NULL},
	{"jxk03",  0x40,  "cgu_cim", 24000000, {0x05, 0x03}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"sc5235", 0x30,  "cgu_cim", 24000000, {0x52, 0x35}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"ov5648", 0x36,  "cgu_cim", 24000000, {0x56, 0x48}, 1, {0x300a, 0x300b}, 2, 2, NULL},
	{"ps5280", 0x48,  "cgu_cim", 24000000, {0x52, 0x80}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"jxf23s", 0x40,  "cgu_cim", 24000000, {0x0f, 0x23}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"gc2053", 0x37,  "cgu_cim", 24000000, {0x20, 0x53}, 1, {0xf0, 0xf1}, 1, 2, NULL},
	{"sc4335", 0x30,  "cgu_cim", 27000000, {0xcd, 0x01}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"ps5260", 0x48,  "cgu_cim", 24000000, {0x52, 0x60}, 1, {0x00, 0x01}, 1, 2, NULL},
	{"os04b10", 0x3c,  "cgu_cim", 24000000, {0x43, 0x08, 0x01}, 1, {0x02, 0x03, 0x04}, 1, 3, NULL},
	{"jxk05",  0x40,  "cgu_cim", 24000000, {0x05, 0x05}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"jxh63",  0x40,  "cgu_cim", 24000000, {0x0a, 0x63}, 1, {0x0a, 0x0b}, 1, 2, NULL},
	{"sc2335", 0x30,  "cgu_cim", 24000000, {0xcb, 0x14}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"jxf37",  0x40,  "cgu_cim", 24000000, {0x0f, 0x37}, 1, {0xa, 0xb}, 1, 2, NULL},
	{"gc4653", 0x29,  "cgu_cim", 24000000, {0x46, 0x53}, 1, {0x03f0, 0x03f1}, 2, 2, NULL},
	{"c23a98", 0x36,  "cgu_cim", 24000000, {0x23, 0x98}, 1, {0x0000, 0x0001}, 2, 2, NULL},
	{"sc3335", 0x30,  "cgu_cim", 24000000, {0xcc, 0x1a}, 1, {0x3107, 0x3108}, 2, 2, NULL},
	{"sc3235", 0x30,  "cgu_cim", 24000000, {0xcc, 0x05}, 1, {0x3107, 0x3108}, 2, 2, NULL},
};

#endif /* __SENSOR_TABLE__ */
