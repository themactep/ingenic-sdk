#ifndef __SPI_TEST_H__
#define __SPI_TEST_H__

#define SFC1_BASE 	0x13490000

/* SFC register */
#define SFC_GLB			        (0x0000)
#define SFC_DEV_CONF          	(0x0004)
#define SFC_DEV_STA_EXP         (0x0008)
#define SFC_DEV_STA_RT        	(0x000c)
#define SFC_DEV_STA_MSK       	(0x0010)
#define SFC_TRAN_CONF(n)      	(0x0014 + (n * 4))
#define SFC_DEV_ADDR(n)       	(0x0030 + (n * 4))
#define SFC_DEV_ADDR_PLUS(n)  	(0x0048 + (n * 4))
#define SFC_TRAN_LEN	      	(0x002c)
#define SFC_MEM_ADDR	      	(0x0060)
#define SFC_TRIG		        (0x0064)
#define SFC_SR			        (0x0068)
#define SFC_SCR			        (0x006c)
#define SFC_INTC	            (0x0070)
#define SFC_FSM		            (0x0074)
#define SFC_CGE		            (0x0078)
#define SFC_CMD_IDX             (0x007c)
#define SFC_COL_ADDR            (0x0080)
#define SFC_ROW_ADDR            (0x0084)
#define SFC_STA_ADDR0           (0x0088)
#define SFC_STA_ADDR1           (0x008c)
#define SFC_DES_ADDR			(0x0090)
#define SFC_GLB1				(0x0094)
#define SFC_DEV1_STA_RT			(0x0098)
#define	SFC_TRAN_CONF1(n)		(0x009c + (n * 4))
#define SFC_LUT                 (0x0800)
#define SFC_RM_DR		        (0x1000)


//For SFC_GLB
#define GLB_POLL_TIME_OFFSET	(16) //T40 add
#define GLB_POLL_TIME_MSK       (0xffff << GLB_POLL_TIME_OFFSET) //T40 add
#define GLB_DES_EN				(1 << 15) //T40 add
#define GLB_CDT_EN				(1 << 14) //T40 add
#define	GLB_TRAN_DIR		    (1 << 13)
#define GLB_TRAN_DIR_OFFSET     (13)
#define GLB_TRAN_DIR_WRITE      (1)
#define GLB_TRAN_DIR_READ       (0)
#define	GLB_THRESHOLD_OFFSET	(7)
#define GLB_THRESHOLD_MSK		(0x3f << GLB_THRESHOLD_OFFSET)
#define GLB_OP_MODE			    (1 << 6)
#define GLB_OP_OFF				(6) //T40 add
#define SLAVE_MODE				(0x0) //T40 add
#define DMA_MODE				(0x1) //T40 add
#define GLB_PHASE_NUM_OFFSET	(3)
#define GLB_PHASE_NUM_MSK		(0x7 << GLB_PHASE_NUM_OFFSET)
#define GLB_WP_EN			    (1 << 2)
#define GLB_BURST_MD_OFFSET		(0)
#define GLB_BURST_MD_MSK		(0x3 << GLB_BURST_MD_OFFSET)

//For SFC_DEV_CONF
#define DEV_CONF_STA_ENDIAN			(31) //T40 add
#define STA_ENDIAN_LSB				(0 << DEV_CONF_STA_ENDIAN) //T40 add
#define STA_ENDIAN_MSB				(1 << DEV_CONF_STA_ENDIAN) //T40 add
#define	DEV_CONF_SMP_DELAY_OFFSET	(16)
#define	DEV_CONF_SMP_DELAY_MSK		(0x1f << DEV_CONF_SMP_DELAY_OFFSET) //T40 0x3->0x1f
#define DEV_CONF_SMP_DELAY_0		(0)
#define DEV_CONF_SMP_DELAY_45		(1) //T40 add
#define DEV_CONF_SMP_DELAY_90		(2) //T40 add
#define DEV_CONF_SMP_DELAY_135		(3) //T40 add
#define DEV_CONF_SMP_DELAY_180		(4) //T40 add
#define DEV_CONF_SMP_DELAY_225		(5) //T40 add
#define DEV_CONF_SMP_DELAY_270		(6) //T40 add
#define DEV_CONF_SMP_DELAY_315		(7) //T40 add
#define DEV_CONF_SMP_DELAY_1		(8) //T40 add
#define DEV_CONF_SMP_DELAY_2		(16) //T40 add
#define DEV_CONF_CMD_TYPE		    (1 << 15)
#define DEV_CONF_STA_TYPE_OFFSET	(13)
#define DEV_CONF_STA_TYPE_MSK		(1 << DEV_CONF_STA_TYPE_OFFSET) //x2000 1(2byte)->3(4byte)
#define DEV_CONF_THOLD_OFFSET		(11)
#define	DEV_CONF_THOLD_MSK		    (0x3 << DEV_CONF_THOLD_OFFSET)
#define DEV_CONF_TSETUP_OFFSET		(9)
#define DEV_CONF_TSETUP_MSK		    (0x0 << DEV_CONF_TSETUP_OFFSET)
#define DEV_CONF_TSH_OFFSET		    (5)
#define DEV_CONF_TSH_MSK			(0xf << DEV_CONF_TSH_OFFSET)
#define DEV_CONF_CPHA			    (1 << 4)
#define DEV_CONF_CPOL			    (1 << 3)
#define DEV_CONF_CEDL			    (1 << 2)
#define DEV_CONF_HOLDDL			    (1 << 1)
#define DEV_CONF_WPDL			    (1 << 0)

//For SFC_TRAN_CONF0
#define	TRAN_CONF0_CLK_OFFSET			(29) //T40 MODE->CLK
#define	TRAN_CONF0_CLK_MSK				(0x7 << TRAN_CONF0_CLK_OFFSET) //T40 MODE->CLK
#define TRAN_CONF0_CLK_MODE_SSS     	(0) //T40 add
#define TRAN_CONF0_CLK_MODE_SSD     	(1) //T40 add
#define TRAN_CONF0_CLK_MODE_SDS     	(2) //T40 add
#define TRAN_CONF0_CLK_MODE_SDD     	(3) //T40 add
#define TRAN_CONF0_CLK_MODE_DSS     	(4) //T40 add
#define TRAN_CONF0_CLK_MODE_DSD     	(5) //T40 add
#define TRAN_CONF0_CLK_MODE_DDS     	(6) //T40 add
#define TRAN_CONF0_CLK_MODE_DDD     	(7) //T40 add
#define	TRAN_CONF0_ADDR_WIDTH_OFFSET	(26)
#define	TRAN_CONF0_ADDR_WIDTH_MSK		(0x7 << TRAN_CONF0_ADDR_WIDTH_OFFSET)
#define	TRAN_CONF0_POLL_EN         		(1 << 25)
#define TRAN_CONF0_POLL_OFFSET			(25) //T40 add
#define TRAN_CONF0_CMDEN_OFFSET			(24)
#define	TRAN_CONF0_CMD_EN         		(1 << 24)
#define	TRAN_CONF0_PHASE_FORMAT         (1 << 23)
#define TRAN_CONF0_FMAT_OFFSET			(23) //T40 add
#define TRAN_CONF0_DMYBITS_OFFSET		(17)
#define TRAN_CONF0_DMYBITS_MSK			(0x3f << TRAN_CONF0_DMYBITS_OFFSET)
#define TRAN_CONF0_DATEEN_OFFSET		(16)
#define TRAN_CONF0_DATEEN				(1 << 16)
#define	TRAN_CONF0_CMD_OFFSET			(0)
#define	TRAN_CONF0_CMD_MSK				(0xffff << TRAN_CONF0_CMD_OFFSET)

/* For SFC_TRAN_CONF1 */
#define TRAN_CONF1_DATA_ENDIAN			(1 << 18) //T40 add
#define TRAN_CONF1_DATA_ENDIAN_OFFSET	(18) //T40 add
#define TRAN_CONF1_DATA_ENDIAN_LSB		(0) //T40 add
#define TRAN_CONF1_DATA_ENDIAN_MSB		(1) //T40 add
#define TRAN_CONF1_WORD_UNIT_OFFSET		(16) //T40 add
#define TRAN_CONF1_WORD_UNIT_MSK		(3 << 16) //T40 add
#define TRAN_CONF1_TRAN_MODE_OFFSET		(4) //T40 CONFIG0->CONFIG1
#define TRAN_CONF1_TRAN_MODE_MSK		(0xf << TRAN_CONF1_TRAN_MODE_OFFSET) //T40 CONFIG0->CONFIG1
#define TRAN_CONF1_SPI_STANDARD			(0x0)
#define TRAN_CONF1_SPI_DUAL				(0x1)
#define TRAN_CONF1_SPI_QUAD				(0x5)
#define TRAN_CONF1_SPI_IO_QUAD			(0x6)
#define TRAN_CONF1_SPI_OCTAL			(0x9) //T40 add
#define TRAN_CONF1_SPI_IO_OCTAL			(0xa) //T40 add
#define TRAN_CONF1_SPI_FULL_OCTAL		(0xb) //T40 add

//For SFC_TRIG
#define TRIG_FLUSH			(1 << 2)
#define TRIG_STOP			(1 << 1)
#define TRIG_START			(1 << 0)

//For SFC_SR
#define FIFONUM_OFFSET	(16)
#define FIFONUM_MSK		(0x7f << FIFONUM_OFFSET)
#define BUSY_OFFSET		(5)
#define BUSY_MSK		(0x3 << BUSY_OFFSET)
#define END			    (1 << 4)
#define TRAN_REQ		(1 << 3)
#define RECE_REQ		(1 << 2)
#define OVER			(1 << 1)
#define UNDER			(1 << 0)

//For SFC_SCR
#define	CLR_END			(1 << 4)
#define CLR_TREQ		(1 << 3)
#define CLR_RREQ		(1 << 2)
#define CLR_OVER		(1 << 1)
#define CLR_UNDR		(1 << 0)

//For SFC_INTC
#define	MASK_END		(1 << 4) //T40 add
#define MASK_TREQ		(1 << 3) //T40 add
#define MASK_RREQ		(1 << 2) //T40 add
#define MASK_OVER		(1 << 1) //T40 add
#define MASK_UNDR		(1 << 0) //T40 add

//SFC_CMD_IDX
#define CMD_IDX_MSK                     (0x3f) //T40 add
#define LUT_DATAEN_MSK                  (0x1 << 31) //T40 add
#define LUT_DATAEN_OFF                  (31) //T40 add
#define LUT_DIR_MSK                     (0x1 << 30) //T40 add
#define LUT_DIR_OFF                     (30) //T40 add

/* For SFC_GLB */
#define GLB1_DQS_EN				(1 << 2) //T40 add
#define GLB1_CHIP_SEL_OFFSET	(0) //T40 add
#define GLB1_CHIP_SEL_MSK		(0x3) //T40 add
#define GLB1_CHIP_SEL_0			(0) //T40 add
#define GLB1_CHIP_SEL_1			(1) //T40 add
#define GLB1_CHIP_SEL_01		(2) //T40 add


//For SFC_FSM
#define FSM_AHB_OFFSET		(16)
#define FSM_AHB_MSK		    (0xf << FSM_AHB_OFFSET)
#define FSM_SPI_OFFSET		(11)
#define FSM_SPI_MSK		    (0x1f << FSM_SPI_OFFSET)
#define FSM_CLK_OFFSET		(6)
#define FSM_CLK_MSK		    (0xf << FSM_CLK_OFFSET)
#define FSM_DMAC_OFFSET		(3)
#define FSM_DMAC_MSK		(0x7 << FSM_DMAC_OFFSET)
#define FSM_RMC_OFFSET		(0)
#define FSM_RMC_MSK		    (0x7 << FSM_RMC_OFFSET)

//For SFC_CGE
#define CG_EN				(1 << 0)
#define SFC_FIFO_LEN		(63)

#define N_MAX				6

#define ENABLE				1
#define DISABLE				0

#define COM_CMD				1	// common cmd
#define POLL_CMD			2	// the cmd will poll the status of flash,ext: read status

#define DMA_OPS				1
#define CPU_OPS				0

#define TM_STD_SPI			0
#define TM_DI_DO_SPI		1
#define TM_DIO_SPI			2
#define TM_FULL_DIO_SPI		3
#define TM_QI_QO_SPI		5
#define TM_QIO_SPI			6
#define	TM_FULL_QIO_SPI		7
#define TM_OCTAL_SPT		9
#define TM_OCTAL_IO_SPI		10
#define TM_OCTAL_FULL_SPI	11

#define DEFAULT_ADDRSIZE	3

#define THRESHOLD			(1)

#define DEF_ADDR_LEN    	3
#define DEF_TCHSH       	5
#define DEF_TSLCH       	5
#define DEF_TSHSL_R     	20
#define DEF_TSHSL_W     	50


struct jz_sfc {
	unsigned int  addr;
	unsigned int  len;
	unsigned int  cmd;
	unsigned int  addr_plus;
	unsigned int  sfc_mode;
	unsigned char daten;
	unsigned char addr_len;
	unsigned char pollen;
	unsigned char phase;
	unsigned char dummy_byte;
};


#endif /* __JZ_SFC_H__ */
