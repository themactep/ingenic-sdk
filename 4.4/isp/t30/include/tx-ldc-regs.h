#ifndef __TX_LDC_REG_H__
#define __TX_LDC_REG_H__

#if (defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21))

#define LDC_CTR			0x0    //view_mode<<4 + int_en<<3 + 1
#define LDC_SAT 		0x4
#define LDC_RES_VAL		0x8    //frame_height<<16 + frame_width
#define LDC_DIS_COEFFX	0xc    //k2_coef_x<<16 + k1_coef_x
#define LDC_DIS_COEFFY	0x10    //k2_coef_y<<16 + k1_coef_y
#define LDC_UDIS_R		0x14    //udisr
#define LDC_LINE_LEN	0x18    //rd_side_burst_len<<24 + rd_burst_len<<16 + wr_side_burst_len<<8 + wr_burst_len
#define LDC_P1_VAL		0x1c    //p1_coef_y<<16 + p1_coef_x
#define LDC_R2_REP		0x20    //r2_coef
#define LDC_FILL_VAL	0x24    //y_fill_val<<16 + u_fill_val<<8 + v_fill_val
#define LDC_FRM_CLK		0x28
#define LDC_Y_DMAIN		0x2c    //y_org_phy
#define LDC_UV_DMAIN	0x30    //uv_org_phy
#define LDC_Y_DMAOUT	0x34    //y_cor_phy
#define LDC_UV_DMAOUT	0x38    //uv_cor_phy
#define LDC_Y_IN_STR	0x3c    //r_dma_str
#define LDC_UV_IN_STR	0x40    //r_dma_str
#define LDC_Y_OUT_STR	0x44    //w_dma_str
#define LDC_UV_OUT_STR	0x48    //w_dma_str

#define LDC_Y_ROW_POS_BADDR		0x100
#define LDC_UV_ROW_POS_BADDR	0x500


/* LDC control register */
#define LDC_CTRL_ENABLE_MASK 		(1<<0)
#define LDC_CTRL_ENABLE		 		(1<<0)
#define LDC_CTRL_RESET_MASK 		(1<<1)
#define LDC_CTRL_RESET_ENABLE 		(1<<1)
#define LDC_CTRL_STOP_MASK 			(1<<2)
#define LDC_CTRL_STOP_ENABLE		(1<<2)
#define LDC_CTRL_INTC_MASK 			(1<<3)
#define LDC_CTRL_INTC_ENABLE 		(1<<3)
#define LDC_CTRL_MODE_MASK 			(1<<4)
#define LDC_CTRL_MODE_FULL 			(0<<4)
#define LDC_CTRL_MODE_CROP 			(1<<4)
#define LDC_CTRL_STOP_MODE_MASK 	(1<<5)
#define LDC_CTRL_STOP_WAIT_AXI 		(1<<5)
#define LDC_CTRL_STOP_NOT_WAIT 		(0<<5)


/* LDC status register */
#define LDC_STAT_EOF_MASK 			(1<<0)
#define LDC_STAT_FRAME_DONE 		(1<<0)
#define LDC_STAT_ST_MASK 			(3<<1)
#define LDC_STAT_ST_RUN 			(1<<1)
#define LDC_STAT_STOP_MASK 			(1<<3)
#define LDC_STAT_STOP	 			(1<<3)

#endif/* CONFIG_SOC_T30 */
#endif/* __TX_LDC_REG_H__ */
