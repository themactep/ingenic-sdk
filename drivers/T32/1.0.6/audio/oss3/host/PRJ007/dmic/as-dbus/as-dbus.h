#ifndef __AS_DBUS_H__
#define __AS_DBUS_H__

#include <linux/bitops.h>
/* DBUS */
#define BTSET		0x0	/* BUS TimeSlot Set Register */
#define BTCLR		0x4	/* BUS TimeSlot Clear Register */
#define BTSR		0x8	/* BUS TimeSlot Status Register */
#define BFSR		0xc	/* BUS FIFO Status Register(TUR/ROR)*/
#define BFCR0		0x10	/* BUS FIFO Control Register0(FLUSH FIFO)*/
#define BFCR1		0X14	/* BUS FIFO Control Register1(TX ZERO SAMPLE<underrun>)*/
#define BFCR2		0x18	/* BUS FIFO Control Register2(BLOKING RX FIFO<overrun>)*/
#define BST0		0x1c	/* BUS Source Timeslot0 */
#define BST1		0x20	/* BUS Source Timeslot1 */
#define BST2		0x24	/* BUS Source Timeslot2 */
#define BTT0		0x28	/* BUS Target Timeslot0 */
#define BTT1		0x2c	/* BUS Target Timeslot1 */

#define BT_TSLOT(slot)	BIT(slot)

#define BFSR_DEV_TUR_SFT	(16)
#define BFSR_DEV_TUR(lo_dev_id)	BIT(((lo_dev_id) + BFSR_DEV_TUR_SFT))
#define BFSR_DEV_ROR_SFT	(0)
#define BFSR_DEV_ROR(li_dev_id)	BIT(((li_dev_id) + BFSR_DEV_ROR_SFT))

#define	BFCR0_DEV_TF_SFT	(16)
#define BFCR0_DEV_TF_MSK	GENMASK(31, BFCR0_DEV_TF_SFT)
#define	BFCR0_DEV_TF(lo_dev_id)	BIT(((lo_dev_id) + BFCR0_DEV_TF_SFT))
#define BFCR0_DEV_RF_SFT	(0)
#define BFCR0_DEV_RF_MSK	GENMASK(15, BFCR0_DEV_RF_SFT)
#define BFCR0_DEV_RF(li_dev_id)	BIT(((li_dev_id) + BFCR0_DEV_RF_SFT))

#define BFCR1_DEV_DBE_MSK	GENMASK(11, 0)
#define BFCR1_DEV_LSMP(lo_dev_id)	BIT(lo_dev_id)

#define BFCR2_DEV_DBE_MSK	GENMASK(14, 0)
#define BFCR2_DEV_DBE(li_dev_id)	BIT(li_dev_id)

#define BST0_DEV0_SUR_SFT	(0)
#define BST0_DEV1_SUR_SFT	(5)
#define BST0_DEV2_SUR_SFT	(10)
#define BST0_DEV3_SUR_SFT	(16)
#define BST0_DEV4_SUR_SFT	(21)
#define BST0_DEV5_SUR_SFT	(26)

#define BST1_DEV6_SUR_SFT	(0)
#define BST1_DEV7_SUR_SFT	(5)
#define BST1_DEV8_SUR_SFT	(10)
#define BST1_DEV9_SUR_SFT	(16)
#define BST1_DEV10_SUR_SFT	(21)

#define BST2_DEV11_SUR_SFT	(0)
#define BST2_DEV12_SUR_SFT	(5)
#define BST2_DEV13_SUR_SFT	(10)
#define BST2_DEV14_SUR_SFT	(16)

#define BTT0_DEV0_TAR_SFT	(0)
#define BTT0_DEV1_TAR_SFT	(5)
#define BTT0_DEV2_TAR_SFT	(10)
#define BTT0_DEV3_TAR_SFT	(16)
#define BTT0_DEV4_TAR_SFT	(21)
#define BTT0_DEV5_TAR_SFT	(26)

#define BTT1_DEV6_TAR_SFT	(0)
#define BTT1_DEV7_TAR_SFT	(5)
#define BTT1_DEV8_TAR_SFT	(10)
#define BTT1_DEV9_TAR_SFT	(16)
#define BTT1_DEV10_TAR_SFT	(21)
#define BTT1_DEV11_TAR_SFT	(26)

#define BSORTT_WITDH		(5)
#define BSORTT_MSK(shift)	GENMASK(((shift) + BSORTT_WITDH - 1), shift)
#define BSORTT(slot, shift)	(((slot) << (shift)) & BSORTT_MSK(shift))

static inline unsigned long ingenic_get_dbus_port_register(int dev_id,
		unsigned long *shift, bool is_out)
{
	if (is_out) {
		u8 reg[12] = {BTT0, BTT0, BTT0, BTT0, BTT0, BTT0,
			BTT1, BTT1, BTT1, BTT1, BTT1, BTT1};
		u8 sft[12] = {BTT0_DEV0_TAR_SFT, BTT0_DEV1_TAR_SFT, BTT0_DEV2_TAR_SFT,
			BTT0_DEV3_TAR_SFT, BTT0_DEV4_TAR_SFT, BTT0_DEV5_TAR_SFT,
			BTT1_DEV6_TAR_SFT, BTT1_DEV7_TAR_SFT, BTT1_DEV8_TAR_SFT,
			BTT1_DEV9_TAR_SFT, BTT1_DEV10_TAR_SFT, BTT1_DEV11_TAR_SFT};
		*shift = (unsigned long)sft[dev_id];
		return (unsigned long)reg[dev_id];
	} else {
		u8 reg[15]  = {BST0, BST0, BST0, BST0, BST0, BST0,
			BST1, BST1, BST1, BST1, BST1,
			BST2, BST2, BST2, BST2};
		u8 sft[15] = {BST0_DEV0_SUR_SFT, BST0_DEV1_SUR_SFT, BST0_DEV2_SUR_SFT,
			BST0_DEV3_SUR_SFT, BST0_DEV4_SUR_SFT, BST0_DEV5_SUR_SFT,
			BST1_DEV6_SUR_SFT, BST1_DEV7_SUR_SFT, BST1_DEV8_SUR_SFT,
			BST1_DEV9_SUR_SFT, BST1_DEV10_SUR_SFT, BST2_DEV11_SUR_SFT,
			BST2_DEV12_SUR_SFT, BST2_DEV13_SUR_SFT, BST2_DEV14_SUR_SFT
		};
		*shift = (unsigned long)sft[dev_id];
		return (unsigned long)reg[dev_id];
	}
}

struct ingenic_dbus_port {
	union {
		struct list_head node;	/* for lineout*/
		struct list_head head;	/* for linein */
	};
	u8 dev_id;			/*port id*/
	u8 timeslot;
	bool is_out;
	bool vaild;
	void *private;
};

#define LI_PORT_MAX_NUM 16
#define LO_PORT_MAX_NUM 16
#define PORT_MAX_NUM (LI_PORT_MAX_NUM + LO_PORT_MAX_NUM)

struct ingenic_dmic_dbus {
	void * __iomem base_addr;
	struct regmap *regmap;
	struct resource *res;
	struct device *dev;
#define SLOT_NUM (31)
	unsigned long slot_bitmap[BITS_TO_LONGS(SLOT_NUM)];
#define LI_DEVID_TO_IDX(dev_id)	(dev_id)
#define LO_DEVID_TO_IDX(dev_id)	((dev_id) + LI_PORT_MAX_NUM)
	struct ingenic_dbus_port port[PORT_MAX_NUM];

};


int ingenic_dmic_dbus_init(int dma_dev_id, int dmic_item);
int ingenic_dmic_dbus_deinit(int dma_dev_id);

#endif	/*__AS_DBUS_H__*/
