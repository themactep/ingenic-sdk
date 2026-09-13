#ifndef HDMI_AIC_H__
#define HDMI_AIC_H__

#define AIC_TX_FIFO_DEPTH 64
#define AIC_RX_FIFO_DEPTH 32

#define MCLK_DIV_TO_SAMPLE 256

#define DEFAULT_REPLAY_CLK (8000*MCLK_DIV_TO_SAMPLE)

#define DEFAULT_REPLAY_TRIGGER 16

#define VALID_8BIT 8
#define VALID_16BIT 16
#define VALID_20BIT 20
#define VALID_24BIT 24

/** AIC registers **/
#define AICFR		0x0000
#define AICCR		0x0004
#define I2SCR		0x0010
#define AICSR		0x0014
#define I2SSR		0x001c
#define I2SDIV		0x0030
#define AICDR		0x0034
#define AICLR		0x0038
#define AICTFLR		0x003c


extern void __iomem * aic_iomem;

#define i2s_write_reg(addr,val)        \
	writel(val,aic_iomem+addr)

#define i2s_read_reg(addr)             \
	readl(aic_iomem+addr)

static void inline i2s_set_reg( unsigned int addr, unsigned int val, unsigned int mask, unsigned int offset)
{
	unsigned long tmp_val;
	unsigned long read_val;
	tmp_val = val;
	read_val = i2s_read_reg(addr);
	read_val &= (~mask);
	tmp_val = ((tmp_val << offset) & mask);
	tmp_val |= read_val;
	i2s_write_reg(addr,tmp_val);
}

#define i2s_get_reg(addr,mask,offset)  \
	((i2s_read_reg(addr) & mask) >> offset)

#define i2s_clear_reg(addr,mask)       \
	i2s_write_reg(addr,~mask)


/* AICFR */
#define I2S_ENB_OFFSET         (0)
#define I2S_ENB_MASK           (0x1 << I2S_ENB_OFFSET)
#define I2S_RMASTER_OFFSET			(1)
#define I2S_RMASTER_MASK			(0x1 << I2S_RMASTER_OFFSET)
#define I2S_TMASTER_OFFSET			(2)
#define I2S_TMASTER_MASK			(0x1 << I2S_TMASTER_OFFSET)
#define I2S_RST_OFFSET				(3)
#define I2S_RST_MASK           		(0X1 << I2S_RST_OFFSET)
#define I2S_AUSEL_OFFSET       		(4)
#define I2S_AUSEL_MASK         		(0x1 << I2S_AUSEL_OFFSET)
#define I2S_ICDC_OFFSET        		(5)
#define I2S_ICDC_MASK          		(0x1 << I2S_ICDC_OFFSET)
#define I2S_LSMP_OFFSET        		(6)
#define I2S_LSMP_MASK          		(0x1 << I2S_LSMP_OFFSET)
#define I2S_DMODE_OFFSET       		(8)
#define I2S_DMODE_MASK         		(0x1 << I2S_DMODE_OFFSET)
#define I2S_MSB_OFFSET         		(12)
#define I2S_MSB_MASK           		(0x1 << I2S_MSB_OFFSET)
#define I2S_RFIFOS_OFFSET         (13)	//receive fifo status after reset aic module
#define I2S_RFIFOS_MASK           (0x1 << I2S_RFIFOS_OFFSET)
#define I2S_TFIFOS_OFFSET         (14)	//transmit fifo status after reset aic module
#define I2S_TFIFOS_MASK           (0x1 << I2S_TFIFOS_OFFSET)
#define I2S_TFTH_OFFSET        		(16)
#define I2S_TFTH_MASK          		(0x1f << I2S_TFTH_OFFSET)
#define I2S_RFTH_OFFSET        		(24)
#define I2S_RFTH_MASK          		(0xf << I2S_RFTH_OFFSET)
//#define I2S_FIFO_RESET_STAT		(I2S_RFIFOS_MASK | I2S_TFIFOS_MASK)
#define I2S_FIFO_RESET_STAT		 (I2S_TFIFOS_MASK)

#define __aic_select_i2s()             \
	i2s_set_reg( AICFR,1,I2S_AUSEL_MASK,I2S_AUSEL_OFFSET)
#define __aic_select_aclink()          \
	i2s_set_reg( AICFR,0,I2S_AUSEL_MASK,I2S_AUSEL_OFFSET)

#define __i2s_set_transmit_trigger(n)  \
	i2s_set_reg(AICFR,n,I2S_TFTH_MASK,I2S_TFTH_OFFSET)
#define __i2s_set_receive_trigger(n)   \
	i2s_set_reg(AICFR,n,I2S_RFTH_MASK,I2S_RFTH_OFFSET)

//internal only set as master
#define __i2s_internal_codec_master()              \
	i2s_set_reg(AICFR,1,I2S_ICDC_MASK,I2S_ICDC_OFFSET);	\

#define __i2s_external_codec()               \
	i2s_set_reg(AICFR,0,I2S_ICDC_MASK,I2S_ICDC_OFFSET);	\


#define __i2s_select_share_clk()		\
	i2s_set_reg(AICFR,0,I2S_DMODE_MASK,I2S_DMODE_OFFSET);\

#define __i2s_select_spilt_clk()		\
	i2s_set_reg(AICFR,1,I2S_DMODE_MASK,I2S_DMODE_OFFSET);\

#define __i2s_enable_24bitmsb()			\
	i2s_set_reg(AICFR,1,I2S_MSB_MASK,I2S_MSB_OFFSET)
#define __i2s_disable_24bitmsb()			\
	i2s_set_reg(AICFR,0,I2S_MSB_MASK,I2S_MSB_OFFSET)

#define __i2s_slave_clkset(i2s_dev)        \
	do {                                            \
		i2s_set_reg(AICFR,0,I2S_TMASTER_MASK,I2S_TMASTER_OFFSET); \
	}while(0)

#define __i2s_master_clkset(i2s_dev)        \
	do {                                            \
		i2s_set_reg(AICFR,1,I2S_TMASTER_MASK,I2S_TMASTER_OFFSET); \
	}while(0)

#define __i2s_play_zero()              \
	i2s_set_reg(AICFR,0,I2S_LSMP_MASK,I2S_LSMP_OFFSET)
#define __i2s_play_lastsample()        \
	i2s_set_reg(AICFR,1,I2S_LSMP_MASK,I2S_LSMP_OFFSET)

#define __i2s_reset()                  \
	i2s_set_reg(AICFR,1,I2S_RST_MASK,I2S_RST_OFFSET)

#define __i2s_enable()                 \
	i2s_set_reg(AICFR,1,I2S_ENB_MASK,I2S_ENB_OFFSET)
#define __i2s_disable()                \
	i2s_set_reg(AICFR,0,I2S_ENB_MASK,I2S_ENB_OFFSET)

/* AICCR */
#define I2S_EREC_OFFSET        (0)
#define I2S_EREC_MASK          (0x1 << I2S_EREC_OFFSET)
#define I2S_ERPL_OFFSET        (1)
#define I2S_ERPL_MASK          (0x1 << I2S_ERPL_OFFSET)
#define I2S_ENLBF_OFFSET       (2)
#define I2S_ENLBF_MASK         (0x1 << I2S_ENLBF_OFFSET)
#define I2S_ETFS_OFFSET        (3)
#define I2S_ETFS_MASK          (0X1 << I2S_ETFS_OFFSET)
#define I2S_ERFS_OFFSET        (4)
#define I2S_ERFS_MASK          (0x1 << I2S_ERFS_OFFSET)
#define I2S_ETUR_OFFSET        (5)
#define I2S_ETUR_MASK          (0x1 << I2S_ETUR_OFFSET)
#define I2S_EROR_OFFSET        (6)
#define I2S_EROR_MASK          (0x1 << I2S_EROR_OFFSET)
#define I2S_RFLUSH_OFFSET      (7)
#define I2S_RFLUSH_MASK        (0x1 << I2S_RFLUSH_OFFSET)
#define I2S_TFLUSH_OFFSET      (8)
#define I2S_TFLUSH_MASK        (0x1 << I2S_TFLUSH_OFFSET)
#define I2S_ASVTSU_OFFSET      (9)
#define I2S_ASVTSU_MASK        (0x1 << I2S_ASVTSU_OFFSET)
#define I2S_ENDSW_OFFSET       (10)
#define I2S_ENDSW_MASK         (0x1 << I2S_ENDSW_OFFSET)
#define I2S_M2S_OFFSET         (11)
#define I2S_M2S_MASK           (0x1 << I2S_M2S_OFFSET)

#define I2S_MONOCTR_RIGHT_OFFSET (12)
#define I2S_MONOCTR_RIGHT_MASK   (0x1 << I2S_MONOCTR_RIGHT_OFFSET)
#define I2S_STEREOCTR_MASK     (0x3 << I2S_MONOCTR_RIGHT_OFFSET)

#define I2S_MONOCTR_OFFSET     (13)
#define I2S_MONOCTR_MASK	   (0x1 << I2S_MONOCTR_OFFSET)

#define I2S_TDMS_OFFSET        (14)
#define I2S_TDMS_MASK          (0x1 << I2S_TDMS_OFFSET)
#define I2S_RDMS_OFFSET        (15)
#define I2S_RDMS_MASK          (0x1 << I2S_RDMS_OFFSET)
#define I2S_ISS_OFFSET         (16)
#define I2S_ISS_MASK           (0x7 << I2S_ISS_OFFSET)
#define I2S_OSS_OFFSET         (19)
#define I2S_OSS_MASK           (0x7 << I2S_OSS_OFFSET)

#define I2S_TLDMS_OFFSET       (22)
#define I2S_TLDMS_MASK         (0x1 << I2S_TLDMS_OFFSET)


#define I2S_ETFL_OFFSET        (23)  //ETFL
#define I2S_ETFL_MASK          (0x1 << I2S_ETFL_OFFSET)

#define I2S_CHANNEL_OFFSET     (24)
#define I2S_CHANNEL_MASK       (0x7 << I2S_CHANNEL_OFFSET)
#define I2S_PACK16_OFFSET      (28)
#define I2S_PACK16_MASK        (0x1 << I2S_PACK16_OFFSET)

#define __i2s_enable_pack16()          \
	i2s_set_reg(AICCR,1,I2S_PACK16_MASK,I2S_PACK16_OFFSET)
#define __i2s_disable_pack16()         \
	i2s_set_reg(AICCR,0,I2S_PACK16_MASK,I2S_PACK16_OFFSET)
#define __i2s_out_channel_select(n)    \
	i2s_set_reg(AICCR,n,I2S_CHANNEL_MASK,I2S_CHANNEL_OFFSET)
#define __i2s_set_oss_sample_size(n)   \
	i2s_set_reg(AICCR,n,I2S_OSS_MASK,I2S_OSS_OFFSET)
#define __i2s_set_iss_sample_size(n)   \
	i2s_set_reg(AICCR,n,I2S_ISS_MASK,I2S_ISS_OFFSET)

#define __i2s_enable_transmit_dma()    \
	i2s_set_reg(AICCR,1,I2S_TDMS_MASK,I2S_TDMS_OFFSET)
#define __i2s_disable_transmit_dma()   \
	i2s_set_reg(AICCR,0,I2S_TDMS_MASK,I2S_TDMS_OFFSET)
#define __i2s_enable_receive_dma()     \
	i2s_set_reg(AICCR,1,I2S_RDMS_MASK,I2S_RDMS_OFFSET)
#define __i2s_disable_receive_dma()    \
	i2s_set_reg(AICCR,0,I2S_RDMS_MASK,I2S_RDMS_OFFSET)

#define __i2s_enable_mono2stereo()     \
	i2s_set_reg(AICCR,1,I2S_M2S_MASK,I2S_M2S_OFFSET)
#define __i2s_disable_mono2stereo()    \
	i2s_set_reg(AICCR,0,I2S_M2S_MASK,I2S_M2S_OFFSET)

#define __i2s_enable_monoctr_left() \
	i2s_set_reg(AICCR, 2, I2S_STEREOCTR_MASK, I2S_MONOCTR_RIGHT_OFFSET)
#define __i2s_disable_monoctr_left() \
	i2s_set_reg(AICCR, 0, I2S_STEREOCTR_MASK, I2S_MONOCTR_RIGHT_OFFSET)

#define __i2s_enable_monoctr_right()\
	i2s_set_reg(AICCR, 1, I2S_STEREOCTR_MASK, I2S_MONOCTR_RIGHT_OFFSET)
#define __i2s_disable_monoctr_right()\
	i2s_set_reg(AICCR, 0, I2S_STEREOCTR_MASK, I2S_MONOCTR_RIGHT_OFFSET)

#define __i2s_enable_stereo()\
	i2s_set_reg(AICCR, 0, I2S_STEREOCTR_MASK, I2S_MONOCTR_RIGHT_OFFSET)

#define __i2s_enable_tloop() \
	i2s_set_reg(AICCR, 1, I2S_TLDMS_MASK, I2S_TLDMS_OFFSET)
#define __i2s_disable_tloop() \
	i2s_set_reg(AICCR, 0, I2S_TLDMS_MASK, I2S_TLDMS_OFFSET)

#define __i2s_enable_etfl() \
	i2s_set_reg(AICCR, 1, I2S_ETFL_MASK, I2S_ETFL_OFFSET)
#define __i2s_disable_etfl() \
	i2s_set_reg(AICCR, 0, I2S_ETFL_MASK, I2S_ETFL_OFFSET)

#define __i2s_enable_byteswap()        \
	i2s_set_reg(AICCR,1,I2S_ENDSW_MASK,I2S_ENDSW_OFFSET)
#define __i2s_disable_byteswap()       \
	i2s_set_reg(AICCR,0,I2S_ENDSW_MASK,I2S_ENDSW_OFFSET)

#define __i2s_enable_signadj()       \
	i2s_set_reg(AICCR,1,I2S_ASVTSU_MASK,I2S_ASVTSU_OFFSET)
#define __i2s_disable_signadj()      \
	i2s_set_reg(AICCR,0,I2S_ASVTSU_MASK,I2S_ASVTSU_OFFSET)

#define __i2s_flush_tfifo()            \
	i2s_set_reg(AICCR,1,I2S_TFLUSH_MASK,I2S_TFLUSH_OFFSET)
#define __i2s_flush_rfifo()            \
	i2s_set_reg(AICCR,1,I2S_RFLUSH_MASK,I2S_RFLUSH_OFFSET)
#define __i2s_test_flush_tfifo()               \
	i2s_get_reg(AICCR,I2S_TFLUSH_MASK,I2S_TFLUSH_OFFSET)
#define __i2s_test_flush_rfifo()               \
	i2s_get_reg(AICCR,I2S_RFLUSH_MASK,I2S_RFLUSH_OFFSET)

#define __i2s_enable_overrun_intr()    \
	i2s_set_reg(AICCR,1,I2S_EROR_MASK,I2S_EROR_OFFSET)
#define __i2s_disable_overrun_intr()   \
	i2s_set_reg(AICCR,0,I2S_EROR_MASK,I2S_EROR_OFFSET)

#define __i2s_enable_underrun_intr()   \
	i2s_set_reg(AICCR,1,I2S_ETUR_MASK,I2S_ETUR_OFFSET)
#define __i2s_disable_underrun_intr()  \
	i2s_set_reg(AICCR,0,I2S_ETUR_MASK,I2S_ETUR_OFFSET)

#define __i2s_enable_transmit_intr()   \
	i2s_set_reg(AICCR,1,I2S_ETFS_MASK,I2S_ETFS_OFFSET)
#define __i2s_disable_transmit_intr()  \
	i2s_set_reg(AICCR,0,I2S_ETFS_MASK,I2S_ETFS_OFFSET)

#define __i2s_enable_receive_intr()    \
	i2s_set_reg(AICCR,1,I2S_ERFS_MASK,I2S_ERFS_OFFSET)
#define __i2s_disable_receive_intr()   \
	i2s_set_reg(AICCR,0,I2S_ERFS_MASK,I2S_ERFS_OFFSET)

#define __i2s_enable_loopback()        \
	i2s_set_reg(AICCR,1,I2S_ENLBF_MASK,I2S_ENLBF_OFFSET)
#define __i2s_disable_loopback()       \
	i2s_set_reg(AICCR,0,I2S_ENLBF_MASK,I2S_ENLBF_OFFSET)

#define __i2s_enable_replay()          \
	i2s_set_reg(AICCR,1,I2S_ERPL_MASK,I2S_ERPL_OFFSET)
#define __i2s_disable_replay()         \
	i2s_set_reg(AICCR,0,I2S_ERPL_MASK,I2S_ERPL_OFFSET)

#define __i2s_enable_record()          \
	i2s_set_reg(AICCR,1,I2S_EREC_MASK,I2S_EREC_OFFSET)
#define __i2s_disable_record()         \
	i2s_set_reg(AICCR,0,I2S_EREC_MASK,I2S_EREC_OFFSET)

/* I2SCR */
#define I2S_AMSL_OFFSET        (0)
#define I2S_AMSL_MASK          (0x1 << I2S_AMSL_OFFSET)
#define I2S_SWLH_OFFSET        (16)
#define I2S_SWLH_MASK          (0x1 << I2S_SWLH_OFFSET)
#define I2S_RFIRST_OFFSET      (17)
#define I2S_RFIRST_MASK        (0x1 << I2S_RFIRST_OFFSET)

#define __i2s_send_rfirst()            \
	i2s_set_reg(I2SCR,1,I2S_RFIRST_MASK,I2S_RFIRST_OFFSET)
#define __i2s_send_lfirst()            \
	i2s_set_reg(I2SCR,0,I2S_RFIRST_MASK,I2S_RFIRST_OFFSET)

#define __i2s_switch_lr()              \
	i2s_set_reg(I2SCR,1,I2S_SWLH_MASK,I2S_SWLH_OFFSET)
#define __i2s_unswitch_lr()            \
	i2s_set_reg(I2SCR,0,I2S_SWLH_MASK,I2S_SWLH_OFFSET)

#define __i2s_select_i2s()             \
	i2s_set_reg(I2SCR,0,I2S_AMSL_MASK,I2S_AMSL_OFFSET)
#define __i2s_select_msbjustified()    \
	i2s_set_reg(I2SCR,1,I2S_AMSL_MASK,I2S_AMSL_OFFSET)

/* AICSR*/
#define I2S_TFS_OFFSET         (3)
#define I2S_TFS_MASK           (0x1 << I2S_TFS_OFFSET)
#define I2S_RFS_OFFSET         (4)
#define I2S_RFS_MASK           (0x1 << I2S_RFS_OFFSET)
#define I2S_TUR_OFFSET         (5)
#define I2S_TUR_MASK           (0x1 << I2S_TUR_OFFSET)
#define I2S_ROR_OFFSET         (6)
#define I2S_ROR_MASK           (0X1 << I2S_ROR_OFFSET)
#define I2S_TFL_OFFSET         (8)
#define I2S_TFL_MASK           (0x3f << I2S_TFL_OFFSET)
#define I2S_RFL_OFFSET         (24)
#define I2S_RFL_MASK           (0x3f << I2S_RFL_OFFSET)

#define __i2s_clear_tur()	\
	i2s_set_reg(AICSR,0,I2S_TUR_MASK,I2S_TUR_OFFSET)
#define __i2s_test_tur()               \
	i2s_get_reg(AICSR,I2S_TUR_MASK,I2S_TUR_OFFSET)
#define __i2s_clear_ror()	\
	i2s_set_reg(AICSR,0,I2S_ROR_MASK,I2S_ROR_OFFSET)
#define __i2s_test_ror()               \
	i2s_get_reg(AICSR,I2S_ROR_MASK,I2S_ROR_OFFSET)
#define __i2s_test_tfs()               \
	i2s_get_reg(AICSR,I2S_TFS_MASK,I2S_TFS_OFFSET)
#define __i2s_test_rfs()               \
	i2s_get_reg(AICSR,I2S_RFS_MASK,I2S_RFS_OFFSET)
#define __i2s_test_tfl()               \
	i2s_get_reg(AICSR,I2S_TFL_MASK,I2S_TFL_OFFSET)
#define __i2s_test_rfl()               \
	i2s_get_reg(AICSR,I2S_RFL_MASK,I2S_RFL_OFFSET)

/* I2SSR */
#define I2S_BSY_OFFSET         (2)
#define I2S_BSY_MASK           (0x1 << I2S_BSY_OFFSET)
#define I2S_RBSY_OFFSET        (3)
#define I2S_RBSY_MASK          (0x1 << I2S_RBSY_OFFSET)
#define I2S_TBSY_OFFSET        (4)
#define I2S_TBSY_MASK          (0x1 << I2S_TBSY_OFFSET)
#define I2S_CHBSY_OFFSET       (5)
#define I2S_CHBSY_MASK         (0X1 << I2S_CHBSY_OFFSET)

#define __i2s_is_busy()                \
	i2s_get_reg(I2SSR,I2S_BSY_MASK,I2S_BSY_OFFSET)
#define __i2s_rx_is_busy()             \
	i2s_get_reg(I2SSR,I2S_RBSY_MASK,I2S_RBSY_OFFSET)
#define __i2s_tx_is_busy()             \
	i2s_get_reg(I2SSR,I2S_TBSY_MASK,I2S_TBSY_OFFSET)
#define __i2s_channel_is_busy()        \
	i2s_get_reg(I2SSR,I2S_CHBSY_MASK,I2S_CHBSY_OFFSET)
/* AICDR */
#define I2S_DATA_OFFSET        (0)
#define I2S_DATA_MASK          (0xffffff << I2S_DATA_OFFSET)

#define __i2s_write_tfifo(v)           \
	i2s_set_reg(AICDR,v,I2S_DATA_MASK,I2S_DATA_OFFSET)
#define __i2s_read_rfifo()             \
	i2s_get_reg(AICDR,I2S_DATA_MASK,I2S_DATA_OFFSET)


/*AICLR*/
#define I2S_LOOP_DATA_OFFSET (0)
#define I2S_LOOP_DATA_MASK    (0xffffff << I2S_LOOP_DATA_OFFSET)

#define __i2s_read_loopdata() \
	i2s_get_reg(AICLR,I2S_LOOP_DATA_MASK,I2S_LOOP_DATA_OFFSET)

/*AICTFLR*/
#define I2S_TFIFO_LOOP_OFFSET (0)
#define I2S_TFIFO_LOOP_MASK   (0xf << I2S_TFIFO_LOOP_OFFSET)

#define __i2s_read_tfifo_loop()\
	i2s_get_reg(AICTFLR, I2S_TFIFO_LOOP_MASK, I2S_LOOP_DATA_OFFSET)
#define __i2s_write_tfifo_loop(v) \
	i2s_set_reg(AICTFLR, v, I2S_TFIFO_LOOP_MASK, I2S_LOOP_DATA_OFFSET)

/* I2SDIV */
#define I2S_DV_OFFSET          (0)
#define I2S_DV_MASK            (0xf << I2S_DV_OFFSET)
#define I2S_IDV_OFFSET         (8)
#define I2S_IDV_MASK           (0xf << I2S_IDV_OFFSET)

#endif /* AUDIO_AIC_H__ */
