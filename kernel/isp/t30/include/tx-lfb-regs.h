#ifndef __TX_LFB_REG_H__
#define __TX_LFB_REG_H__

#if (defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21))

#define LFB_CONTROL		0x200
#define LFB_RESTART		0x204
#define LFB_HBLK_CFG	0x208
#define LFB_Y_CFG		0x210
#define LFB_Y_RES		0x214
#define LFB_Y_BUF0		0x218

#define LFB_UV_CFG		0x230
#define LFB_UV_RES		0x234
#define LFB_UV_BUF0		0x238

#define LFB_ERR_REG		0x24c
#define LFB_DEBUG7		0x27c
/* LFB control register */
#define LFB_CTRL_ENABLE(v)		((v)<<0)
#define LFB_CTRL_ENABLE_MASK	(1<<0)

#define LFB_CTRL_DISABLE_CHECK(v)	((v) << 1)
#define LFB_CTRL_DISABLE_CHECK_MASK	(1<< 1)

#define LFB_CTRL_RECORVER_MODE(v)	(((v)&0x01) << 2)
#define LFB_CTRL_RECORVER_MODE_MASK	(1<< 2)

#define LFB_CTRL_CHAN_SEL_FR	(0)
#define LFB_CTRL_CHAN_SEL_DS1	(1)
#define LFB_CTRL_CHAN_SEL(v)	((v) << 3)
#define LFB_CTRL_CHAN_SEL_MASK	(1<< 3)

#define LBF_CTRL_OUT_SEL_NCU	(0)
#define LBF_CTRL_OUT_SEL_MS		(1)
#define LFB_CTRL_OUT_SEL(v)		((v) << 6)
#define LFB_CTRL_OUT_SEL_MASK	(1<< 6)

#define LBF_CTRL_VSYNC_COND_NOTWAIT	(0)
#define LBF_CTRL_VSYNC_COND_WAIT	(1)
#define LFB_CTRL_VSYNC_COND(v)		((v) << 20)
#define LFB_CTRL_VSYNC_COND_MASK	(1<< 20)

#define LBF_CTRL_NCU_TO_MSC		(0)
#define LBF_CTRL_NCU_TO_DDR		(1)
#define LFB_CTRL_DISABLE_MSC(v)		((v) << 21)
#define LFB_CTRL_DISABLE_MSC_MASK	(1<< 21)

#define LBF_CTRL_FRAME_CHECK_ENABLE		(0)
#define LBF_CTRL_FRAME_CHECK_DISABLE	(1)
#define LFB_CTRL_FRAME_CHECK(v)		((v) << 22)
#define LFB_CTRL_FRAME_CHECK_MASK	(1<< 22)

/* LFB Y channel configure */
#define LFB_Y_CFG_STRIDE(v)			((v)<<0)
#define LFB_Y_CFG_STRIDE_MASK		((0xffff)<<0)
#define LFB_Y_CFG_BUF_NUM(v)		((v)<<16)
#define LFB_Y_CFG_BUF_NUM_MASK		((0x7)<<16)

/* LFB Y channel resolution */
#define LFB_Y_CFG_WIDTH(v)			(v)
#define LFB_Y_CFG_WIDTH_MASK		(0xffff)
#define LFB_Y_CFG_HEIGHT(v)			((v)<<16)
#define LFB_Y_CFG_HEIGHT_MASK		(0xffff << 16)

/* LFB UV channel configure */
#define LFB_UV_CFG_STRIDE(v)			((v)<<0)
#define LFB_UV_CFG_STRIDE_MASK		((0xffff)<<0)
#define LFB_UV_CFG_BUF_NUM(v)		((v)<<16)
#define LFB_UV_CFG_BUF_NUM_MASK		((0x7)<<16)

/* LFB UV channel resolution */
#define LFB_UV_CFG_WIDTH(v)			(v)
#define LFB_UV_CFG_WIDTH_MASK		(0xffff)
#define LFB_UV_CFG_HEIGHT(v)			((v)<<16)
#define LFB_UV_CFG_HEIGHT_MASK		(0xffff << 16)

#endif/* CONFIG_SOC_T30 */
#endif/* __TX_LFB_REG_H__ */
