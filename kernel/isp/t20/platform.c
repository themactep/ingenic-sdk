/*
 * Platform device support for Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <linux/i2c-gpio.h>
#include <soc/gpio.h>
#include <soc/base.h>
#include <soc/irq.h>

#include <mach/platform.h>

static u64 tx_isp_module_dma_mask = ~(u64)0;

/* the widget is the controller of  all sensors */
struct tx_isp_widget_descriptor tx_isp_widget_vin = {
	.type = TX_ISP_TYPE_WIDGET,
	.subtype = TX_ISP_SUBTYPE_INPUT_TERMINAL,
	.parentid = TX_ISP_SUBDEV_ID(0),
	.unitid = TX_ISP_WIDGET_ID(0),
	.clks_num = 0,
};

struct platform_device tx_isp_vin_platform_device = {
	.name = TX_ISP_VIN_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_widget_vin,
	},
	.num_resources = 0,
};


/* the controller is mipi interfaces*/
static struct resource tx_isp_csi_resource[] = {
	[0] = {
		/*.name = "mipi_csi",*/
		.name = TX_ISP_DEV_NAME,
		.start = MIPI_CSI_IOBASE,
		.end = MIPI_CSI_IOBASE + 0x1000,
		.flags = IORESOURCE_MEM,
	},
};

static struct tx_isp_device_clk isp_csi_clk_info[] = {
	{"csi", DUMMY_CLOCK_RATE},
};

struct tx_isp_widget_descriptor tx_isp_widget_csi = {
	.type = TX_ISP_TYPE_WIDGET,
	.subtype = TX_ISP_SUBTYPE_SELECTOR_UNIT,
	.parentid = TX_ISP_SUBDEV_ID(0),
	.unitid = TX_ISP_WIDGET_ID(1),
	.clk_num = ARRAY_SIZE(isp_csi_clk_info),
	.clks = isp_csi_clk_info,
};

struct platform_device tx_isp_csi_platform_device = {
	.name = TX_ISP_CSI_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_widget_csi,
	},
	.num_resources = ARRAY_SIZE(tx_isp_csi_resource),
	.resource = tx_isp_csi_resource,
};

/* It's a process unit that connects several interfaces of sensor */
static struct resource tx_isp_vic_resource[] = {
	[0] = {
		.name = TX_ISP_DEV_NAME,
		.start = ISP_VIC_IOBASE,
		.end = ISP_VIC_IOBASE + 0x10000,
		.flags = IORESOURCE_MEM,
	},
};

struct tx_isp_widget_descriptor tx_isp_widget_vic = {
	.type = TX_ISP_TYPE_WIDGET,
	.subtype = TX_ISP_SUBTYPE_CONTROLLER,
	.parentid = TX_ISP_SUBDEV_ID(0),
	.unitid = TX_ISP_WIDGET_ID(2),
	.clk_num = 0,
};

struct platform_device tx_isp_vic_platform_device = {
	.name = TX_ISP_VIC_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_widget_vic,
	},
	.num_resources = ARRAY_SIZE(tx_isp_vic_resource),
	.resource = tx_isp_vic_resource,
};

/* It's the first process unit */
static struct resource tx_isp_core_resource[] = {
	[0] = {
		.name = TX_ISP_DEV_NAME,
		/*.name = "isp_device",*/
		.start = ISP_CORE_IOBASE,
		.end = ISP_CORE_IOBASE + 0x80000,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		/*.name = "isp_device_irq",*/
		.name = TX_ISP_IRQ_NAME,
		.start = ISP_IRQ_IOBASE,
		.end = ISP_IRQ_IOBASE + 0x10000,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		/*.name = "isp_irq_id",*/
		.name = TX_ISP_IRQ_ID,
		.start = IRQ_ISP,
		.end = IRQ_ISP,
		.flags = IORESOURCE_IRQ,
	},
};

static struct tx_isp_device_clk isp_core_clk_info[] = {
	{"cgu_isp", 85000000},
	{"isp", DUMMY_CLOCK_RATE},
};

struct tx_isp_subdev_descriptor tx_isp_subdev_ispcore = {
	.type = TX_ISP_TYPE_SUBDEV,
	.subtype = TX_ISP_SUBTYPE_PROCESSING_UNIT,
	.parentid = TX_ISP_HEADER_ID(0),
	.unitid = TX_ISP_SUBDEV_ID(0),
	.clks_num = ARRAY_SIZE(isp_core_clk_info),
	.clks = isp_core_clk_info,
	.pads_num = 3,
	.pads = {
		{
			.type = TX_ISP_PADTYPE_UNDEFINE,
		},
		{
			.type = TX_ISP_PADTYPE_OUTPUT,
			.links_type = TX_ISP_LINKTYPE_DDR | TX_ISP_LINKTYPE_LFB,
		},
		{
			.type = TX_ISP_PADTYPE_OUTPUT,
			.links_type = TX_ISP_LINKTYPE_DDR,
		},
	},
};

struct platform_device tx_isp_core_platform_device = {
	.name = TX_ISP_CORE_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_subdev_ispcore,
	},
	.num_resources = ARRAY_SIZE(tx_isp_core_resource),
	.resource = tx_isp_core_resource,
};

struct tx_isp_subdev_descriptor tx_isp_subdev_framesource = {
	.type = TX_ISP_TYPE_SUBDEV,
	.subtype = TX_ISP_SUBTYPE_OUTPUT_TERMINAL,
	.parentid = TX_ISP_HEADER_ID(0),
	.unitid = TX_ISP_SUBDEV_ID(1),
	.clks_num = 0,
	.pads_num = 3,
	.pads = {
		{
			.type = TX_ISP_PADTYPE_OUTPUT,
			.links_type = TX_ISP_LINKTYPE_DDR,
		},
		{
			.type = TX_ISP_PADTYPE_OUTPUT,
			.links_type = TX_ISP_LINKTYPE_DDR,
		},
		{
			.type = TX_ISP_PADTYPE_OUTPUT,
			.links_type = TX_ISP_LINKTYPE_DDR,
		},
	},
};

struct platform_device tx_isp_fs_platform_device = {
	.name = TX_ISP_CORE_NAME,
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_isp_subdev_framesource,
	},
	.num_resources = 0,
};

struct platform_device *tx_isp_devices[] = {
	/* isp core and its widgets */
	&tx_isp_core_platform_device,
	&tx_isp_vic_platform_device,
	&tx_isp_csi_platform_device,
	&tx_isp_vin_platform_device,
	/* ldc module and its widgets */
	/* ncu module and its widgets */
	/* mscaler module and its widgets */
	/* framesource module and its widgets */
	&tx_isp_fs_platform_device,
};


/* It's the root device */
struct tx_isp_device_descriptor tx_video_device = {
	.type = TX_ISP_TYPE_HEADER,
	.subtype = TX_ISP_SUBTYPE_CONTROLLER,
	.parentid = -1,
	.unitid = TX_ISP_HEADER_ID(0),
	.entity_num = ARRAY_SIZE(tx_isp_devices),
	.entities = tx_isp_devices,
};

struct platform_device tx_isp_platform_device = {
	.name = "tx-isp",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = &tx_video_device,
	},
	.num_resources = 0,
};
