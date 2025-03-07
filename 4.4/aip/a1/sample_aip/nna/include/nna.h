/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : nna.h
 * Authors    : jzhang@elim.ic.jz.com
 * Create Time: 2018-12-11:10:52:52
 * Description:
 *
 */

#ifndef __NNA_H__
#define __NNA_H__

/**********************************/
/*** for NNA's register address ***/
/**********************************/
#define NNARA_TYPE_BIT          5
#define NNARA_TYPE_MASK         3
#define NNARA_G_TYPE            0
#define NNARA_M_TYPE            1
#define NNARA_D_TYPE            2

//====== DEBUG-Begin - simulator only
//for DEBUG used register address
#define NNARA_DBG_TYPE          3
#define NNARA_WRAM_ADDR         0
#define NNARA_FRAM_ADDR         1
#define NNARA_IC_NUM            2
#define NNARA_OC_NUM            3
//FRAM rdnum check
//#define NNA_FRFMT_H4_2X2    1
//#define NNA_FRFMT_W24_1X4    2
/* All DWR-F data must be used

   (1) kernel = 3x3, x/y-stride = 1, all pixels processed
   if N-line is processed, N+2 lines will be used, the use number for
   every pixel in the same line is the same, which are

   top/bottom lines: are (3...3)*wbit_num
   line below top or above bottom: are (6...6)*wbit_num
   all other lines: are (9...9)*wbit_num

   edge adjust: in left/right edge out, add 1 to edge pixel

   (2) kernel = 3x3, x/y-stride = 2, 1/4 pixels processed
   if N-line is processed, 2N+1 lines will be used, the use number for
   every pixel in the same line is in the following rules
   lines the pixel processed:     (...1 2 1 2 1 2 1...)*wbit_num
   lines the pixel not processed: (...2 4 2 4 2 4 2...)*wbit_num

   no edge adjust need

   (3) kernel = 1x1, x/y-stride = 1, all pixels processed
   every pixel is the same: (1...1)*wbit_num

   no edge adjust need

   m_nna_frfmt
   bit[2:0]: fmt-mode. 0:none, 1:K11, 2:K33
   bit[3]: disable DWRF
   bit[4]: stride. 0:S1, 1:S2
   bit[5]: Load outside data. 0:no, 1:yes
   bit[6]: BOTTOM extra data with mac. 0:no. 1:yes
   bit[7]: TOP margin. 0~1 line
   bit[8]: BOTTOM margin. 0~1 line
   bit[9]: RIGHT margin. 0~1 pixel
   bit[10~11]: LEFT margin. 0~3 pixel
   bit[12~14]: right extra data. 0~7 pixels
   bit[15]: has bottom extra data
   bit[19:16]: bottom extra line number. 0~15
 */
#define NNARA_FRFMT             4
#define NNA_FRFMT_NONE          0
#define NNA_FRFMT_MODE_MASK     0x7
#define NNA_FRFMT_K11           1
#define NNA_FRFMT_K33           2
#define NNA_FRFMT_NODWRF        8
#define NNA_FRFMT_S2            0x0010
#define NNA_FRFMT_LOD           0x0020
//#define NNA_FRFMT_BEXD        0x0040
#define NNA_FRFMT_TMGN          0x0080
#define NNA_FRFMT_BMGN          0x0100
#define NNA_FRFMT_RMGN          0x0200
#define NNA_FRFMT_LMGN          0x0C00
#define NNA_FRFMT_LMGN_BIT      10
#define NNA_FRFMT_REXD          0x7000
#define NNA_FRFMT_REXD_BIT      12
#define NNA_FRFMT_BEXD          0xf0000
#define NNA_FRFMT_BEXD_BIT      16

//give ofp number for FRAM RD use calculation
#define NNARA_FRFMT_OFPN        5
//for print every insn info on its execute
#define NNARA_INSN_INFO         6
#define NNARA_RDNUM_INFO        7

#define NNADRD_DBG_TYPE_BIT     6
#define NNADRD_DBG_TYPE_MASK    7

#define NNADRD_WRAM_IC          1
#define NNADRD_WRAM_OC          2
#define NNADRD_FRAM             3
#define NNADRD_OF_WP            4
#define NNADRD_OF_WPIC          5
#define NNADRD_OF_WPOC          6
#define NNADRD_BT               7

//#define NNACMD_WICOC_TYPE_BIT    11
//#define NNACMD_WICOC_VRD    1
//#define NNACMD_WICOC_RD        2
//====== DEBUG-End

//MAC
#define NNARA_WR_RADDR          0x10
#define NNARA_WR_RADDR_RST      0x11
#define NNARA_WR_RADDR_RST_ADD  0x12
#define NNARA_RCFL_E            0x13

#define NNARA_FR_RADDR          0xd
#define NNARA_PAD_VALUE         0xf
#define NNARA_FP_H              0xc
#define NNARA_FP_W              0xe
#define NNARA_MACTL_FP_Y        0x9
#define NNARA_MACTL_FP_X        0xb
//#define NNARA_MACTL_FP_ADD    0x2
#define NNARA_TLYP_ACC          0xa
#define NNARA_SF_MODE           0x8
//#define NNARA_OC_BIT_SIG    0x5
#define NNARA_MIN_FPN_O         0x1
#define NNARA_FR_RB_E           0x0
#define NNARA_F_BIT_GSIZE       0x3

#define NNARA_FR_FP_W           0x19
#define NNARA_FR_FP_STRIDE      0x17
#define NNARA_MAC_STRIDE        0x1a
#define NNARA_MAC_FP            0x18
#define NNARA_MAC_BIT           0x1b
#define NNARA_FP_NUM0           0x1c
#define NNARA_FP_NUM1           0x1d
#define NNARA_MAC_W_MASK        0x1e
#define NNARA_MAC_P_MASK        0x1f

//DATA
#define NNARA_WR_WADDR          0x00
#define NNARA_FR_WADDR          0x17
#define NNARA_FR_WADDR_ADD      0x16
#define NNARA_FR_WADDR_INC0     0x10
#define NNARA_FR_WADDR_INC1     0x11
#define NNARA_FR_WADDR_INC2     0x12
#define NNARA_FR_WADDR_INC3     0x13
#define NNARA_FR_ALWAYS_INC     0x14
#define NNARA_F_BIT             0x15
#define NNARA_MSF_MODE          0x6
#define NNARA_Q_BIT             0x4

//G&PC
#define NNARA_RD_G              0
#define NNARA_RD_PCG0           1
#define NNARA_RD_PCG1           2

#define NNARA_EWSR              0x0
#define NNARA_CTRL              0x2
#define NNARA_CTRL_W0           0x2
#define NNARA_CTRL_W1           0x3
#define NNARA_INTE              0x4
#define NNARA_INTE_W0           0x4
#define NNARA_INTE_W1           0x5
#define NNARA_PC_FIFO_TH        0x6
#define NNARA_PC_INSN0_SEL      0x8
#define NNARA_PC_INSN1_SEL      0x9
#define NNARA_PC_INSN2_SEL      0xa
#define NNARA_PC_COUNT0_SEL     0xc
#define NNARA_PC_COUNT1_SEL     0xd
#define NNARA_PC_COUNT2_SEL     0xe

//bit for CTRL
#define NNARA_CTRL_CLREW_BIT    0
#define NNARA_CTRL_CLRPC_BIT    1
#define NNARA_CTRL_PCEG0_BIT    2
#define NNARA_CTRL_PCEG1_BIT    3

//bit for both EWSR and INTE
#define NNARA_IVLD_INSN_BIT     0
#define NNARA_RUN_ERROR_BIT     1
#define NNARA_RAM_CFL_BIT       2
#define NNARA_RCFL_TIMEOUT_BIT  3
#define NNARA_BIT_SIG_IVLD_BIT  4
#define NNARA_BIT_SIG_CPOC_BIT  5

//REG-PC-INSN-SEL
#define NNAPC_INSN_NNA          0x0
#define NNAPC_INSN_MAC          0x1
#define NNAPC_INSN_MACL         0x2
#define NNAPC_INSN_MACFF        0x3
#define NNAPC_INSN_QOCB         0x4
#define NNAPC_INSN_CPQR         0x5
#define NNAPC_INSN_CPOC         0x6
#define NNAPC_INSN_DWRW         0x8
#define NNAPC_INSN_DWRF         0x9
#define NNAPC_INSN_DWRBT        0xa
#define NNAPC_INSN_RRD          0xb
#define NNAPC_INSN_DRDOCB       0xc
#define NNAPC_INSN_DRDQR        0xd
#define NNAPC_INSN_DRDQRB       0xe
#define NNAPC_INSN_DRDSF        0xf

//REG-PC-COUNT-SEL
#define NNAPC_CNT_MAC_IDLE      0x0
#define NNAPC_CNT_MAC_4BKOP     0x1
#define NNAPC_CNT_MAC_MACOP     0x2
#define NNAPC_CNT_FRROB_HIT     0x3
#define NNAPC_CNT_FRBANK_RDUTY  0x4
#define NNAPC_CNT_FRBANK_WDUTY  0x5
#define NNAPC_CNT_FRBANK_RBLKW  0x6
#define NNAPC_CNT_MACFF_INUM    0x7
#define NNAPC_CNT_WRBUF_DUTY    0x8
#define NNAPC_CNT_WRBUFW_DUTY   0x9
#define NNAPC_CNT_WRBUFF_DUTY   0xa
#define NNAPC_CNT_WRBUFBT_DUTY  0xb
#define NNAPC_CNT_WR_RDUTY      0xc
#define NNAPC_CNT_WR_WDUTY      0xd
#define NNAPC_CNT_WR_RBLKW      0xe

//REG-PC-G0
#define NNAPCG0_WAIT_MACFF      0
#define NNAPCG0_WAIT_INBUF      1
#define NNAPCG0_WAIT_DWRBT      2
#define NNAPCG0_WAIT_QOCB       3
#define NNAPCG0_WAIT_CPOC       4
#define NNAPCG0_WAIT_CPQR       5
#define NNAPCG0_WAITFF_DRD      6
#define NNAPCG0_WAITFF_QOCB     7
#define NNAPCG0_MAC_WAIT_RCFL   8
#define NNAPCG0_DWRF_WAIT_RCFL  9
#define NNAPCG0_DWRW_WAIT_RCFL  10
#define NNAPCG0_MACOP_FREX_NUM  11
#define NNAPCG0_WAIT_LSRS       12

//REG-PC-G1
#define NNAPCG1_CYCLE_D         0
#define NNAPCG1_INSN0_D         1
#define NNAPCG1_INSN1_D         2
#define NNAPCG1_INSN2_D         3
#define NNAPCG1_COUNT0_D        4
#define NNAPCG1_COUNT1_D        5
#define NNAPCG1_COUNT2_D        6

/**************************/
/*** for NNA's commands ***/
/**************************/
#define NNACMD_TYPE_BIT         6
#define NNACMD_TYPE_MASK        3
#define NNACMD_G_TYPE           0
#define NNACMD_MACLUC_TYPE      1
#define NNACMD_MACLC_TYPE       2
#define NNACMD_MACLC0_TYPE      2
#define NNACMD_MACLC1_TYPE      3
//G CMD
#define NNACMD_RESET            0
//MAC CMD
//#define NNACMD_BLOCK_DWR    0
//OC
#define NNACMD_CPOC             1
#define NNACMD_CLROC            2
#define NNACMD_CPQR             4
//#define NNACMD_BTOCB        8
#define NNACMD_Q_MASK           0x18
#define NNACMD_Q0OCB            0x08
#define NNACMD_Q1OCB            0x10
#define NNACMD_Q32OCB           0x18
#define NNACMD_TL_MASK          0x60
#define NNACMD_TLYP             0x20
#define NNACMD_TLYM             0x60
#define NNACMD_TLYMXP           0x40

#define NNACMD_LS2OC            0
#define NNACMD_RS2OC            1

/****************************/
/*** for NNA's DATA Write ***/
/****************************/
#define NNADWR_TYPE_BIT         5
#define NNADWR_TYPE_MASK        3

#define NNADWR_W_TYPE           0
#define NNADWR_F_TYPE           1
#define NNADWR_BT_TYPE          2

/***************************/
/*** for NNA's DATA Read ***/
/***************************/
#define NNADRD_TYPE_BIT         5
#define NNADRD_TYPE_MASK        3

#define NNADRD_OCB_TYPE         0
#define NNADRD_QR_TYPE          1
#define NNADRD_SF_TYPE          2

#endif /* __NNA_H__ */

