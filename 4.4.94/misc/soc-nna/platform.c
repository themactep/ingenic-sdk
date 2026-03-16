/*
 * Platform device soc_nna support for Ingenic series SoC.
 *
 * Copyright 2019, <pengtao.kang@ingenic.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "soc_nna_common.h"

static void soc_nna_device_release(struct device *dev)
{
    return;
}

/* bscaler */
static uint64_t soc_nna_dma_mask = ~((uint64_t)0);
static struct resource soc_nna_resources[] = {
	[0] = {
		.start = SOC_NNA_DMA_IOBASE,
		.end = SOC_NNA_DMA_IOBASE + SOC_NNA_DMA_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = SOC_NNA_DMA_DESRAM_ADDR,
		.end = SOC_NNA_DMA_DESRAM_ADDR + SOC_NNA_DMA_DESRAM_SIZE - 1,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device soc_nna_device = {
	.name = "soc-nna",
	.id = 0,
	.dev = {
		.dma_mask = &soc_nna_dma_mask,
		.coherent_dma_mask = 0xffffffff,
        .release= soc_nna_device_release,
	},
	.num_resources = ARRAY_SIZE(soc_nna_resources),
	.resource = soc_nna_resources,
};
