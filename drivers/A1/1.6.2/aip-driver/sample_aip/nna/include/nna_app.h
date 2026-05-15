/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : nna_insn.h
 * Authors    : jzhang@elim.ic.jz.com
 * Create Time: 2019-01-07:15:42:19
 * Description:
 *
 */

#ifndef __NNA_INSN_H__
#define __NNA_INSN_H__

#include <stdint.h>
#include "nna.h"
#include "mxu3.h"

//WRAM in 256-byte, FRAM in 1-Byte
#define WRAM_ENTRY              288
#define FRAM_SIZE               16384
#define FR_WALIGN_BITS          2
#define FR_WALIGN               (1<<FR_WALIGN_BITS)
#define MAC_PNUM_LG             2

#define FR_FP_W_BITS            5
#define FR_FP_STRIDE_BITS       8

/**********************************************************************/
/****** 1. struct                                         ******/
/**********************************************************************/
/* all in pixel
   Input feature FP: ifp_width, ifp_height
   MAC FP:
   mac_width = ifp_width - ifp_left_margin - ifp_right_margin
   mac_hight = ifp_height - ifp_top_margin - ifp_bottom_margin
   MAC FP corner:
       top-left:     ifp_top_margin, ifp_left_margin
       bottom-right: (ifp_height-1) - ifp_bottom_margin,
                     (ifp_width-1) - ifp_right_margin
   usually in case of S1, ifp_left_margin = ifp_right_margin = 0
   ifp_data_width >= ifp_width,
   not equal in case of ifp_width not meet data storage align requirement

   ofp_num: output fp number, all ofp buffers in oram are continued
*/
typedef struct {
  int ifp_width, ifp_height;
  int left_margin, right_margin, top_margin, bottom_margin;
  int mac_height, mac_line;
  int mac_width, mac_column;
  int mac_stride;
  int wbit;

  int ifp_num, ifp_num_upp, ofp_num;
}fp_cfg_t;
/* conv_with_dma: issue DMA along with convolution
   ?dma_blk_num: how many if/of blocks one DMA moves, 1 block contains a number
                 of lines in 1 block loop iteration convolution done
   ?dma_num:     how many IF/OF DMA will be issued in call to the convolution
                 function. idma_num for IF, odma_num for OF
   if_dma_entry[dma_num], of_dma_entry[dma_num]:
                 entry of every if and of DMA, used to start the DMA
   w/bt/of_or_ptr:ORAM pointer of W,BT,IF,OF buffer
   if_or_ptr:     ORAM pointer array of IF buffers
   if_orbuf_size:ORAM buffer size for 1 IFP
   of_orbuf_size:ORAM buffer size for 1 OFP
   iline_size:   IF line_size in byte
   oline_size:   OF line_size in byte
   mac_bln:      MAC block line number, how many line be done in 1 block iter
   ?f_orbuf_lnum:IF/OF ORAM buffer line number
   ?f_lstart:    work start line, 0:from the beginning
*/
typedef struct {
  int conv_with_dma, idma_blk_num, odma_blk_num, idma_num, odma_num;
  uint32_t *if_dma_entry, *of_dma_entry;
  uint8_t *w_or_ptr, *bt_or_ptr, *of_or_ptr;
  uint8_t **if_or_ptr;
  //int if_orbuf_size, of_orbuf_size;
  uint32_t iline_size, oline_size;
  int mac_bln, if_orbuf_lnum, of_orbuf_lnum, if_lstart, of_lstart;
}cnn_data_t;
/* buf_start, buf_end: 512-bit align
   linesize: size in byte of every line, 32 byte align
   data_linesize: size in byte of data in every line, data_linesize <= linesize
   total_line: buffer total line = (buf_end - buf_start)/linesize
   line: start line of current run
*/
typedef struct {
  uint8_t *buf_start, *buf_end;
  int linesize, data_linesize;
  int total_line, line;
}orif_buf_t;
/* all_buf_start: 512-bit align, start place of all ofp buffers
*/
typedef struct {
  uint8_t *all_buf_start;
  uint8_t **buf_start, **buf_end;
  int linesize, data_linesize;
  int total_line, line;
}orof_buf_t;

//DMA channel id
#define WBT_CHID 2
#define F_CHID   0

/**********************************************************************/
/****** 1. Base NNA insn                                         ******/
/**********************************************************************/
//NNCMD
#define NNRST NNCMD(NNACMD_G_TYPE<<NNACMD_TYPE_BIT|NNACMD_RESET)

#define NNCPOC   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_CPOC)
#define NNCLROC  NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_CLROC)
#define NNCPQR   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_CPQR)
#define NNQOCB   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_QOCB)
#define NNTLYP   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_TLYP)
#define NNTLYM   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_TLYM)
#define NNTLYMXP NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_TLYMXP)

#define NNCC       NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|    \
             NNACMD_CPOC|NNACMD_CLROC)
#if 0//without 4bit Q-OCB
#define NNCCQ       NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|    \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_QOCB)
#define NNCCCQ       NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_QOCB)
#endif
#define NNCCYP       NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|    \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_TLYP)
#define NNCCYM       NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|    \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_TLYM)
#define NNCCYMXP   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_TLYMXP)
#if 0//without 4bit Q-OCB
#define NNCCQYP    NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_QOCB|NNACMD_TLYP)
#define NNCCQYM    NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_QOCB|NNACMD_TLYM)
#define NNCCQYMXP  NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_QOCB|NNACMD_TLYMXP)
#define NNCCCQYP   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_QOCB| \
             NNACMD_TLYP)
#define NNCCCQYM   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_QOCB| \
             NNACMD_TLYM)
#define NNCCCQYMXP NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
             NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_QOCB| \
             NNACMD_TLYMXP)
#endif

#define NNQ0OCB      NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_Q0OCB)
#define NNQ1OCB      NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_Q1OCB)
#define NNQ32OCB     NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|NNACMD_Q32OCB)

#define NNCCQ0         NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q0OCB)
#define NNCCQ1         NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q1OCB)
#define NNCCQ32         NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q32OCB)
#define NNCCCQ0         NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q0OCB)
#define NNCCCQ1         NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q1OCB)
#define NNCCCQ32     NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q32OCB)
#define NNCCQ0YP     NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q0OCB|NNACMD_TLYP)
#define NNCCQ1YP     NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q1OCB|NNACMD_TLYP)
#define NNCCQ32YP    NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q32OCB|NNACMD_TLYP)
#define NNCCQ0YM     NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q0OCB|NNACMD_TLYM)
#define NNCCQ1YM     NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q1OCB|NNACMD_TLYM)
#define NNCCQ32YM    NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q32OCB|NNACMD_TLYM)
#define NNCCQ0YMXP   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q0OCB|NNACMD_TLYMXP)
#define NNCCQ1YMXP   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q1OCB|NNACMD_TLYMXP)
#define NNCCQ32YMXP  NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_Q32OCB|NNACMD_TLYMXP)
#define NNCCCQ0YP    NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q0OCB| \
               NNACMD_TLYP)
#define NNCCCQ1YP    NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q1OCB| \
               NNACMD_TLYP)
#define NNCCCQ32YP   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q32OCB| \
               NNACMD_TLYP)
#define NNCCCQ0YM    NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q0OCB| \
               NNACMD_TLYM)
#define NNCCCQ1YM    NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q1OCB| \
               NNACMD_TLYM)
#define NNCCCQ32YM   NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q32OCB| \
               NNACMD_TLYM)
#define NNCCCQ0YMXP  NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q0OCB| \
               NNACMD_TLYMXP)
#define NNCCCQ1YMXP  NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q1OCB| \
               NNACMD_TLYMXP)
#define NNCCCQ32YMXP NNCMD(NNACMD_MACLC_TYPE<<NNACMD_TYPE_BIT|        \
               NNACMD_CPOC|NNACMD_CLROC|NNACMD_CPQR|NNACMD_Q32OCB| \
               NNACMD_TLYMXP)

#define NNLS2OC NNCMD(NNACMD_MACLUC_TYPE<<NNACMD_TYPE_BIT|NNACMD_LS2OC)
#define NNRS2OC NNCMD(NNACMD_MACLUC_TYPE<<NNACMD_TYPE_BIT|NNACMD_RS2OC)

//NNRWR
#define NNRWRG(vwrs,nnrn) NNRWR(vwrs,NNARA_G_TYPE<<NNARA_TYPE_BIT|(nnrn))
#define NNRWRM(vwrs,nnrn) NNRWR(vwrs,NNARA_M_TYPE<<NNARA_TYPE_BIT|(nnrn))
#define NNRWRD(vwrs,nnrn) NNRWR(vwrs,NNARA_D_TYPE<<NNARA_TYPE_BIT|(nnrn))

//NNRRD
#define NNRRDG(vrd)    NNRRD(vrd,NNARA_G_TYPE<<NNARA_TYPE_BIT|NNARA_RD_G)
#define NNRRDPCG0(vrd) NNRRD(vrd,NNARA_G_TYPE<<NNARA_TYPE_BIT|NNARA_RD_PCG0)
#define NNRRDPCG1(vrd) NNRRD(vrd,NNARA_G_TYPE<<NNARA_TYPE_BIT|NNARA_RD_PCG1)
#define NNRRDM(vrd)    NNRRD(vrd,NNARA_M_TYPE<<NNARA_TYPE_BIT)
#define NNRRDD(vrd)    NNRRD(vrd,NNARA_D_TYPE<<NNARA_TYPE_BIT)

//NNDWR
#define NNDWRW(vrp,inca,pn) \
  NNDWR(vrp,NNADWR_W_TYPE<<NNADWR_TYPE_BIT|(inca)|(pn)<<1)
#define NNDWRF(vrp,inca,half_offset,invmsb)                \
  NNDWR(vrp,NNADWR_F_TYPE<<NNADWR_TYPE_BIT|(inca)|(half_offset)<<2|(invmsb)<<4)
#define NNDWRF8(vrp,inca)                    \
  NNDWR(vrp,NNADWR_F_TYPE<<NNADWR_TYPE_BIT|(inca)|0<<2|0<<4)
#define NNDWRF4(vrp,inca)                    \
  NNDWR(vrp,NNADWR_F_TYPE<<NNADWR_TYPE_BIT|(inca)|1<<2|0<<4)
#define NNDWRFU(vrp,inca,offset)                \
  NNDWR(vrp,NNADWR_F_TYPE<<NNADWR_TYPE_BIT|(inca)|(offset)<<2|0<<4)
#define NNDWRFS(vrp,inca,offset)                \
  NNDWR(vrp,NNADWR_F_TYPE<<NNADWR_TYPE_BIT|(inca)|(offset)<<2|1<<4)
//#define NNDWRB(vrp) NNDWR(vrp,NNADWR_BT_TYPE<<NNADWR_TYPE_BIT|0)
//#define NNDWRT(vrp) NNDWR(vrp,NNADWR_BT_TYPE<<NNADWR_TYPE_BIT|1)
#define NNDWRB0(vrp) NNDWR(vrp,NNADWR_BT_TYPE<<NNADWR_TYPE_BIT|0)
#define NNDWRB1(vrp) NNDWR(vrp,NNADWR_BT_TYPE<<NNADWR_TYPE_BIT|1)
#define NNDWRT0(vrp) NNDWR(vrp,NNADWR_BT_TYPE<<NNADWR_TYPE_BIT|2)
#define NNDWRT1(vrp) NNDWR(vrp,NNADWR_BT_TYPE<<NNADWR_TYPE_BIT|3)

//NNDRD
#define NNDRDOCB(vrd,pixel,h)                    \
  NNDRD(vrd,NNADRD_OCB_TYPE<<NNADRD_TYPE_BIT|(pixel)<<1|(h))
#define NNDRDSF(vrd)  NNDRD(vrd,NNADRD_SF_TYPE<<NNADRD_TYPE_BIT)
#define NNDRDQR(vrd)  NNDRD(vrd,NNADRD_QR_TYPE<<NNADRD_TYPE_BIT|0)
#define NNDRDQRB(vrd) NNDRD(vrd,NNADRD_QR_TYPE<<NNADRD_TYPE_BIT|4)

//NNMAC
#define NNMACG(vwra,wraopt,sf,fpn_N) NNMAC(vwra,(wraopt)|(sf)<<1|(fpn_N)<<2)
#define NNMAC0(vwra,wraopt)          NNMAC(vwra,(wraopt)|   0<<1|      0<<2)
#define NNMAC1(vwra,wraopt)         NNMAC(vwra,(wraopt)|   0<<1|      1<<2)
#define NNMAC0S(vwra,wraopt)         NNMAC(vwra,(wraopt)|   1<<1|      0<<2)
#define NNMAC1S(vwra,wraopt)         NNMAC(vwra,(wraopt)|   1<<1|      1<<2)

/**********************************************************************/
/****** 2. NNA insn options                                      ******/
/**********************************************************************/
//NNDWR
#define OPT_NNDWRW_INCA    1
#define OPT_NNDWRW_NONINCA    0
#define OPT_NNDWRW_PN0    0
#define OPT_NNDWRW_PN1    1
#define OPT_NNDWRW_PN2    2
#define OPT_NNDWRW_PN3    3

#define OPT_NNDWRF_INC0    0
#define OPT_NNDWRF_INC1    1
#define OPT_NNDWRF_INC2    2
#define OPT_NNDWRF_INC3    3
#define OPT_NNDWRF_OFFSET0    0
#define OPT_NNDWRF_OFFSET1    1
#define OPT_NNDWRF_OFFSET2    2
#define OPT_NNDWRF_OFFSET3    3

//NNDRD
#define OPT_NNDRDOCB_PIXEL0    0
#define OPT_NNDRDOCB_PIXEL1    1
#define OPT_NNDRDOCB_PIXEL2    2
#define OPT_NNDRDOCB_PIXEL3    3

#define OPT_NNDRDOCB_LO    0
#define OPT_NNDRDOCB_HI    1

//NNMAC
#define OPT_NNMAC_WRARST    0
#define OPT_NNMAC_WRAINC    1
#define OPT_NNMAC_FP_NUM0   0
#define OPT_NNMAC_FP_NUM1   1

/**********************************************************************/
/****** 3. Write Config Regs                                     ******/
/**********************************************************************/
/* after NNRST, the config regs are as following
   ###### General #######
   CTRL            0    (PC not enable)
   INTE            0x3f    (All 1, all interrupt enable)
   PC_FIFO_TH        0
   PC_INSN0_SEL        0    NNA
   PC_INSN1_SEL        1    MAC
   PC_INSN2_SEL        9    DWRF
   PC_COUNT0_SEL    0    MAC-IDLE
   PC_COUNT1_SEL    1    MAC-op number
   PC_COUNT2_SEL    10    DWRF in wrbuf duty

   ###### MAC (4-pixel) #######
   WR_RADDR    0
   WR_RADDR_RST    0
   RCFL_E    1
   PAD_VALUE    0
   FP_H        2047
   FP_W        2047
   MACTL_FP_Y    -1
   MACTL_FP_X    -1
   TLYP_ACC    0
   SF_MODE    0
   MIN_FPN_O    0
   FR_RB_E    1
   F_BIT_GSIZE    1    (FRAM-read-buffer-number - 1)
   FR_FP_W    16
   FR_FP_STRIDE    64
   MAC_STRIDE    0
   MAC_FP    2
   MAC_BIT    0
   FP_NUM0    0
   FP_NUM1    0
   MAC_W_MASK    0x1ff
   MAC_P_MASK    0xf

   ###### DATA #######
   WR_WADDR        0
   FR_WADDR        0
   FR_WADDR_INC0    1
   FR_WADDR_INC1    1
   FR_WADDR_INC2    1
   FR_WADDR_INC3    1
   FR_ALWAYS_INC    0
   F_BIT        0
   MSF_MODE        0
   Q_BIT        1
*/

//###### General #######
#define ENABLE_PC(vwr) do {                     \
  LIWR(vwr,1<<NNARA_CTRL_PCEG0_BIT|1<<NNARA_CTRL_PCEG1_BIT); \
  NNRWRG(vwr,NNARA_CTRL_W1);} while(0)
#define DISABLE_PC(vwr) do {                     \
  LIWR(vwr,1<<NNARA_CTRL_PCEG0_BIT|1<<NNARA_CTRL_PCEG1_BIT); \
  NNRWRG(vwr,NNARA_CTRL_W0);} while(0)
/* in the following, where `c' can be one of:
   NNAPC_INSN_NNA
   NNAPC_INSN_MAC
   NNAPC_INSN_MACL
   NNAPC_INSN_MACFF
   NNAPC_INSN_BTOCB
   NNAPC_INSN_CPQR
   NNAPC_INSN_CPOC
   NNAPC_INSN_DWRW
   NNAPC_INSN_DWRF
   NNAPC_INSN_DWRBT
   NNAPC_INSN_RRD
   NNAPC_INSN_DRDOCB
   NNAPC_INSN_DRDQR
   NNAPC_INSN_DRDQRB
   NNAPC_INSN_DRDSF
*/
#define SET_PC_INSN0(c,vwr) do {                 \
  LIWR(vwr,c); NNRWRG(vwr,NNARA_PC_INSN0_SEL);} while(0)
#define SET_PC_INSN1(c,vwr) do {                 \
  LIWR(vwr,c); NNRWRG(vwr,NNARA_PC_INSN1_SEL);} while(0)
#define SET_PC_INSN2(c,vwr) do {                 \
  LIWR(vwr,c); NNRWRG(vwr,NNARA_PC_INSN2_SEL);} while(0)
/* in the following, where `c' can be one of:
   NNAPC_CNT_MAC_IDLE
   NNAPC_CNT_MAC_4BKOP
   NNAPC_CNT_MAC_MACOP
   NNAPC_CNT_FRROB_HIT
   NNAPC_CNT_FRBANK_RDUTY
   NNAPC_CNT_FRBANK_WDUTY
   NNAPC_CNT_FRBANK_RBLKW
   NNAPC_CNT_MACFF_INUM
   NNAPC_CNT_WRBUF_DUTY
   NNAPC_CNT_WRBUFW_DUTY
   NNAPC_CNT_WRBUFF_DUTY
   NNAPC_CNT_WRBUFBT_DUTY
   NNAPC_CNT_WR_RDUTY
   NNAPC_CNT_WR_WDUTY
   NNAPC_CNT_WR_RBLKW
 */
#define SET_PC_COUNT0(c,vwr) do {                 \
  LIWR(vwr,c); NNRWRG(vwr,NNARA_PC_COUNT0_SEL);} while(0)
#define SET_PC_COUNT1(c,vwr) do {                 \
  LIWR(vwr,c); NNRWRG(vwr,NNARA_PC_COUNT1_SEL);} while(0)
#define SET_PC_COUNT2(c,vwr) do {                 \
  LIWR(vwr,c); NNRWRG(vwr,NNARA_PC_COUNT2_SEL);} while(0)

//###### MAC #######
//WC_*: write constant to a config register, where c = -512 ~ 510
#define WC_NNRWRM(c,reg,scrvwr) {LIWR(scrvwr,c); NNRWRM(scrvwr,reg);}
//WC_* write to a pair of regs to eliminate insn stall
#define WC_WR_RADDR_AND_RST(addr,addr_rst,vwr0,vwr1) do{        \
    LIWR(vwr0,addr); LIWR(vwr1,addr_rst);                \
    NNRWRM(vwr0,NNARA_WR_RADDR); NNRWRM(vwr1,NNARA_WR_RADDR_RST);} while(0)
//add 'c' to WR_RADDR_RST, where c = -128 ~ 127
#define ADD_WR_RADDR_RST(c,vwr) do{                \
    LIWR(vwr,c); NNRWRM(vwr,NNARA_WR_RADDR_RST_ADD);} while(0)
//WPTR_* write variable with its point to config reg
//WPTR_* write to a pair of regs to eliminate insn stall
// h,w to NNARA_FP_H,NNARA_FP_W
#define WPTR_FP_HW(h_ptr,w_ptr,vwr0,vwr1) do{                \
    LA(w,VR31,vwr0,h_ptr,0); LA(w,VR31,vwr1,w_ptr,0);            \
    NNRWRM(vwr0,NNARA_FP_H); NNRWRM(vwr1,NNARA_FP_W);} while(0)
#define WC_FP_YX(y,x,vwr0,vwr1) do{                    \
    LIWR(vwr0,y); LIWR(vwr1,x);                        \
    NNRWRM(vwr0,NNARA_MACTL_FP_Y); NNRWRM(vwr1,NNARA_MACTL_FP_X);} while(0)
#define WC_SF_MODE(c,vwr) do{LIWR(vwr,c); NNRWRM(vwr,NNARA_SF_MODE);} while(0)
#define WC_FR_FP_W_STRIDE(fp_w,fp_stride,vwr0,vwr1) do{            \
    LIWR(vwr0,fp_w); LIWR(vwr1,fp_stride);                \
    NNRWRM(vwr0,NNARA_FR_FP_W); NNRWRM(vwr1,NNARA_FR_FP_STRIDE);} while(0)
#define WC_FP_NUM(num0,num1,vwr0,vwr1) do{                \
    LIWR(vwr0,num0); LIWR(vwr1,num1);                    \
    NNRWRM(vwr0,NNARA_FP_NUM0); NNRWRM(vwr1,NNARA_FP_NUM1);} while(0)

//###### DATA #######
#define WC_NNRWRD(c,reg,scrvwr) {LIWR(scrvwr,c); NNRWRD(scrvwr,reg);}
#define WC_WR_FR_WADDR(wr_waddr,fr_waddr,vwr0,vwr1) do{            \
    LIWR(vwr0,wr_waddr); LIWR(vwr1,fr_waddr);                \
    NNRWRD(vwr0,NNARA_WR_WADDR); NNRWRD(vwr1,NNARA_FR_WADDR);} while(0)
#define WC_FR_WADDR_INC01(inc0,inc1,vwr0,vwr1) do{            \
    LIWR(vwr0,inc0); LIWR(vwr1,inc1);                \
    NNRWRD(vwr0,NNARA_FR_WADDR_INC0);NNRWRD(vwr1,NNARA_FR_WADDR_INC1);} while(0)
#define WC_FR_WADDR_INC23(inc2,inc3,vwr0,vwr1) do{            \
    LIWR(vwr0,inc2); LIWR(vwr1,inc3);                \
    NNRWRD(vwr0,NNARA_FR_WADDR_INC2);NNRWRD(vwr1,NNARA_FR_WADDR_INC3);} while(0)
#define WC_FR_WADDR_INC(N,inc,vwr0) do{                    \
    LIWR(vwr0,inc); NNRWRD(vwr0,NNARA_FR_WADDR_INC##N);} while(0)
#define WC_MSF_MODE(c,vwr) do{LIWR(vwr,c); NNRWRD(vwr,NNARA_MSF_MODE);} while(0)

#define WC_SFMSF_MODE(mode,vwr) do{LIWR(vwr,mode);             \
    NNRWRM(vwr,NNARA_SF_MODE); NNRWRD(vwr,NNARA_MSF_MODE);} while(0)

//WPTR_*: write a variable with its pointer a config register
//write INC0,1,2,3, use VWR<vwr0~3>
#define WPTR_FR_WADDR_INC0123(inc0_ptr,inc1_ptr,inc2_ptr,inc3_ptr,    \
                  vwr0,vwr1,vwr2,vwr3) do{            \
    LA(w,VR31,vwr0,inc0_ptr,0);                        \
    LA(w,VR31,vwr1,inc0_ptr,0);                        \
    LA(w,VR31,vwr2,inc0_ptr,0);                        \
    LA(w,VR31,vwr3,inc0_ptr,0);                        \
    NNRWRD(vwr0,NNARA_FR_WADDR_INC0);                    \
    NNRWRD(vwr1,NNARA_FR_WADDR_INC1);                    \
    NNRWRD(vwr2,NNARA_FR_WADDR_INC2);                    \
    NNRWRD(vwr3,NNARA_FR_WADDR_INC3);                    \
} while(0)
//write INC<n>, use VWR<vwr>
#define WPTR_FR_WADDR_INC(n,inc_ptr,vwr) do{    \
    LA(w,VR31,vwr,inc_ptr,0);            \
    if (n==0) NNRWRD(vwr,NNARA_FR_WADDR_INC0);    \
    if (n==1) NNRWRD(vwr,NNARA_FR_WADDR_INC1);    \
    if (n==2) NNRWRD(vwr,NNARA_FR_WADDR_INC2);    \
    if (n==3) NNRWRD(vwr,NNARA_FR_WADDR_INC3);    \
  } while(0)

#define FR_WADDR_ADD(vwr) NNRWRD(vwr,NNARA_FR_WADDR_ADD)

/**********************************************************************/
/****** 4. Debug                                                 ******/
/**********************************************************************/
//////DEBUG-Begin - simulator only
#ifdef SIM_INFO
//NNRWRDBG
#define NNRWRDBG(vwrs,nnrn) NNRWR(vwrs,NNARA_DBG_TYPE<<NNARA_TYPE_BIT|nnrn)
//NNMAC1_DBG
//#define NNMAC1_DBG(vwra,ls2oc,wopt) NNMAC(vwra,ls2oc|wopt<<1|1<<7)
//Read W of a OC
#define NNDRDW_OC(vrd) NNDRD(vrd,NNADRD_WRAM_OC<<NNADRD_DBG_TYPE_BIT)
//Read W of a IC
#define NNDRDW_IC(vrd) NNDRD(vrd,NNADRD_WRAM_IC<<NNADRD_DBG_TYPE_BIT)
//Read F
#define NNDRDF(vrd) NNDRD(vrd,NNADRD_FRAM<<NNADRD_DBG_TYPE_BIT)
//Read FR USE-NUM
#define NNDRDFNUM(vrd) NNDRD(vrd,(NNADRD_FRAM<<NNADRD_DBG_TYPE_BIT)|1)
//Read OF of a W & a pixel
#define NNDRDOF_WP(vrd,h,w,pixel)                                       \
  NNDRD(vrd,((NNADRD_OF_WP<<NNADRD_DBG_TYPE_BIT)|(h<<4)|(w<<2)|pixel)
//Read OF of a W & a pixel & a ic
#define NNDRDOF_WPIC(vrd,h,w,pixel)                                     \
  NNDRD(vrd,((NNADRD_OF_WPIC<<NNADRD_DBG_TYPE_BIT)|(h<<4)|(w<<2)|pixel)
//Read OF of a W & a pixel & a oc
#define NNDRDOF_WPOC(vrd,h,w,pixel)                                     \
  NNDRD(vrd,((NNADRD_OF_WPOC<<NNADRD_DBG_TYPE_BIT)|(h<<4)|(w<<2)|pixel)
//Read B
#define NNDRDB(vrd) NNDRD(vrd,NNADRD_BT<<NNADRD_DBG_TYPE_BIT|0)
//Read T
#define NNDRDT(vrd) NNDRD(vrd,NNADRD_BT<<NNADRD_DBG_TYPE_BIT|1)

//Set NNFRFMT_NONE
#define NNFRFMT_NONE do{                                        \
    SAVE_VWR0;                                                  \
    LIWR(VWR0,NNA_FRFMT_NONE); NNRWRDBG(VWR0,NNARA_FRFMT);      \
    RESTORE_VWR0;} while(0)
#define NNFRFMT(fmt_ptr,ofpn_ptr) do{                \
    SAVE_VWR0;                                                  \
    LA(w,VR31,0,fmt_ptr,0); NNRWRDBG(VWR0,NNARA_FRFMT);        \
    LA(w,VR31,0,ofpn_ptr,0); NNRWRDBG(VWR0,NNARA_FRFMT_OFPN);    \
    RESTORE_VWR0;} while(0)
//Set NNFRFMT_K33S1
#define NNFRFMT_K33S1(n_ptr) do{                \
    SAVE_VWR0;                                                  \
    LIWR(VWR0,NNA_FRFMT_K33S1); NNRWRDBG(VWR0,NNARA_FRFMT);    \
    LA(w,VR31,0,n_ptr,0); NNRWRDBG(VWR0,NNARA_FRFMT_OFPN);    \
    RESTORE_VWR0;} while(0)
#define NNFRFMT_K33S1LO(n_ptr) do{                \
    SAVE_VWR0;                                                  \
    LIWR(VWR0,NNA_FRFMT_K33S1LO); NNRWRDBG(VWR0,NNARA_FRFMT);    \
    LA(w,VR31,0,n_ptr,0); NNRWRDBG(VWR0,NNARA_FRFMT_OFPN);    \
    RESTORE_VWR0;} while(0)
//Set NNFRFMT_K33S2
#define NNFRFMT_K33S2(n_ptr) do{                \
    SAVE_VWR0;                                                  \
    LIWR(VWR0,NNA_FRFMT_K33S2); NNRWRDBG(VWR0,NNARA_FRFMT);    \
    LA(w,VR31,0,n_ptr,0); NNRWRDBG(VWR0,NNARA_FRFMT_OFPN);    \
    RESTORE_VWR0;} while(0)
#define NNFRFMT_K33S2LO(n_ptr) do{                \
    SAVE_VWR0;                                                  \
    LIWR(VWR0,NNA_FRFMT_K33S2LO); NNRWRDBG(VWR0,NNARA_FRFMT);    \
    LA(w,VR31,0,n_ptr,0); NNRWRDBG(VWR0,NNARA_FRFMT_OFPN);    \
    RESTORE_VWR0;} while(0)
//Set NNFRFMT_K11S1
#define NNFRFMT_K11S1(n_ptr) do{                \
    SAVE_VWR0;                                                  \
    LIWR(VWR0,NNA_FRFMT_K11S1); NNRWRDBG(VWR0,NNARA_FRFMT);    \
    LA(w,VR31,0,n_ptr,0); NNRWRDBG(VWR0,NNARA_FRFMT_OFPN);    \
    RESTORE_VWR0;} while(0)

//enable insn info: level=0,1,2,3 0:no info, 1~n:info increasing
#define NNINSN_INFO(level) do{                     \
    SAVE_VWR0; LIWR(VWR0,level); NNRWRDBG(VWR0,NNARA_INSN_INFO); \
    RESTORE_VWR0;} while(0)

//enable rdnum info
#define ENABLE_RDNUM_INFO do{                        \
    SAVE_VWR0; LIWR(VWR0,1); NNRWRDBG(VWR0,NNARA_RDNUM_INFO); RESTORE_VWR0; \
  } while(0)
//disable rdnum info
#define DISABLE_RDNUM_INFO do{                        \
    SAVE_VWR0; LIWR(VWR0,0); NNRWRDBG(VWR0,NNARA_RDNUM_INFO); RESTORE_VWR0; \
  } while(0)

//Print W-data of a OC at waddr
#define PRINTW_OC(waddr,oc_num) do{                    \
    SAVE_VR31; uint32_t data[2]; data[0] = waddr; data[1] = oc_num;    \
    LA(w,VR31,VWR0,data,0); NNRWRDBG(VWR0,NNARA_WRAM_ADDR);        \
    LA(w,VR31,VWR0,data,1); NNRWRDBG(VWR0,NNARA_OC_NUM);        \
                                        \
    NNDRDW_OC(VR31);                                                    \
    printf("W-RAM addr=%d oc-num=%d\nic-num=0~31\t", waddr,oc_num);     \
    PRINT2_VR(VR31,32);                                                 \
    RESTORE_VR31;                                       \
  } while(0)

//Print a W of a IC
#define PRINTW_IC(waddr,ic_num) do{                    \
    SAVE_VR31;                                          \
    MFCPUW(VWR0,waddr); NNRWRDBG(VWR0,NNARA_WRAM_ADDR); \
    MFCPUW(VWR0,ic_num); NNRWRDBG(VWR0,NNARA_IC_NUM);   \
                                                        \
    NNDRDW_IC(VR31);                                                    \
    printf("W-RAM addr=%d ic-num=%d\noc-num=0~31\t", waddr,ic_num);     \
    PRINT16_VR_W1(VR31);                                                \
    RESTORE_VR31;                            \
  } while(0)

//Print a F
#define PRINTF(faddr) do{                    \
    SAVE_VR31; uint32_t data[1]; data[0] = faddr;        \
    LA(w,VR31,VWR0,data,0); NNRWRDBG(VWR0,NNARA_FRAM_ADDR);    \
                                    \
    NNDRDF(VR31);                        \
    printf("F-RAM addr=%d\nic-num=0~31\t", faddr);        \
    PRINT2_VR(VR31,32);                        \
    RESTORE_VR31;                        \
  } while(0)

//Print 64 F-RAM use num, at (faddr>>6)+0 ~ (faddr>>6)+63
#define PRINTFNUM(faddr) do{                            \
    SAVE_VR31;                                          \
    MFCPUW(VWR0,faddr); NNRWRDBG(VWR0,NNARA_FRAM_ADDR); \
                                                        \
    NNDRDFNUM(VR31);                                                    \
    uint32_t addr = (faddr>>6)<<6;                                      \
    printf("F-RAM addr=%d~%d %x~%x\tuse-num=\n", addr,addr+63, addr,addr+63); \
    PRINT8_VR(VR31,64);                                                 \
    RESTORE_VR31;                                                       \
  } while(0)

//Print a OF of a W & a pixel
#define PRINTOF_WP(h,w,pixel) do{                                       \
    SAVE_VR31;                                                          \
    NNDRDOF_WP(VR31,h,w,pixel);                                         \
    printf("OF of W[%d,%d] for pixel=%d oc-num=0~31\t", h,w,pixel);     \
    PRINT16_VR(VR31);                                                   \
    RESTORE_VR31;                                                       \
  } while(0)

//Print a OF of a W & a pixel & a ic
#define PRINTOF_WPIC(h,w,pixel,ic_num) do{                              \
    SAVE_VR31;                                                          \
    MFCPUW(VWR0,ic_num); NNRWRDBG(VWR0,NNARA_IC_NUM);                   \
    NNDRDOF_WPIC(VR31,h,w,pixel);                                       \
    printf("OF of W[%d,%d] for pixel=%d ic-num=%d oc-num=0~31\t",       \
           h,w,ic_num,pixel);                                           \
    PRINT16_VR(VR31);                                                   \
    RESTORE_VR31;                                                       \
  } while(0)

//Print a OF of a W & a pixel & a oc
#define PRINTOF_WPOC(h,w,pixel,oc_num) do{                              \
    SAVE_VR31;                                                          \
    MFCPUW(VWR0,oc_num); NNRWRDBG(VWR0,NNARA_OC_NUM);                   \
    NNDRDOF_WPOC(VR31,h,w,pixel);                                       \
    printf("OF of W[%d,%d] for pixel=%d oc-num=%d ic-num=0~31\t",       \
           h,w,oc_num,pixel);                                           \
    PRINT16_VR(VR31);                                                   \
    RESTORE_VR31;                                                       \
  } while(0)

#else

#define NNRWRDBG(vwrs,nnrn)
#define NNDRDW_OC(vrd)
#define NNDRDW_IC(vrd)
#define NNDRDF(vrd)
#define NNDRDFNUM(vrd)
#define NNDRDOF_WP(vrd,h,w,pixel)
#define NNDRDOF_WPIC(vrd,h,w,pixel)
#define NNDRDOF_WPOC(vrd,h,w,pixel)
#define NNDRDB(vrd)
#define NNDRDT(vrd)
#define NNFRFMT_NONE
#define NNFRFMT_K33S1(n_ptr)
#define NNFRFMT_K33S2(n_ptr)
#define NNFRFMT_K11S1(n_ptr)
#define NNINSN_INFO(level)
#define ENABLE_RDNUM_INFO
#define DISABLE_RDNUM_INFO
#define PRINTW_OC(waddr,oc_num)
#define PRINTF(faddr)

#endif
//print all 16-bit elements in a VR-reg
#define PRINT16_VR_BT(vr) do{                           \
    int16_t print_data[32];                             \
    SA(o,vr,0,print_data,0);                \
    SA(o,vr,1,print_data+16,0);                \
    int i;                                              \
    printf("\n");                                       \
    for (i=0; i<32; i++) {                              \
      printf("%7d ",print_data[i]);                     \
      if (i%8==7) printf("\n");                         \
    }                                                   \
  }while(0)

//Print B
#define PRINTB do{                                                      \
    SAVE_VR31;                                                          \
    NNDRDB(VR31);                                                       \
    printf("Bias oc-num=0~31\t");                                       \
    PRINT16_VR_BT(VR31);                                                \
    RESTORE_VR31;                                                       \
  } while(0)

//Print T
#define PRINTT do{                                                      \
    SAVE_VR31;                                                          \
    NNDRDT(VR31);                                                       \
    printf("Threshold oc-num=0~31\t");                                  \
    PRINT16_VR_BT(VR31);                                                \
    RESTORE_VR31;                                                       \
  } while(0)
//////DEBUG-End

/**********************************************************************/
/****** 5. Others                                                ******/
/**********************************************************************/
//save VWR0
#define SAVE_VWR0 uint32_t __nna_save_vwr0[1]; SA(w,VR31,0,__nna_save_vwr0,0)
//restore VWR0
#define RESTORE_VWR0 LA(w,VR31,0,__nna_save_vwr0,0)

//save VR31
#define SAVE_VR31                                                       \
  uint32_t save_vr31[16];                                               \
  SA(o,VR31,0,save_vr31,0);                        \
  SA(o,VR31,1,save_vr31+8,0)
//restore VR31
#define RESTORE_VR31                                                    \
  LA(o,VR31,0,save_vr31,0);                        \
  LA(o,VR31,1,save_vr31+8,0)

//print a VWR-reg
#define PRINT_VWR(vwr) do{                    \
    int32_t print_data[1];                    \
    SA(w,VR31,vwr,print_data,0);                \
    printf("VWR%d = %d %x\n",vwr,print_data[0],print_data[0]);    \
  }while(0)

//print all 16-bit elements in a VR-reg in width=1, end with \n
#define PRINT16_VR_W1(vr) do{                           \
    int16_t print_data[32];                             \
    SA(o,vr,0,print_data,0);                \
    SA(o,vr,1,print_data+16,0);                \
    int i;                                              \
    for (i=0; i<32; i++) {                              \
      printf("%1d",print_data[i]);                      \
      if (i%4==3) printf(" ");                          \
    }                                                   \
    printf("\n");                                       \
  }while(0)

//print all 16-bit elements in a VR-reg
#define PRINT16_VR(vr) do{                              \
    uint16_t print_data[32];                \
    SA(o,vr,0,print_data,0);                \
    SA(o,vr,1,print_data+16,0);                \
    int i;                                              \
    printf("\n");                                       \
    for (i=0; i<32; i++) {                              \
      printf(" %.4x",print_data[i]);            \
      if (i%16==15) printf("\n");            \
    }                                                   \
  }while(0)
//print all 32-bit elements in a VR-reg
#define PRINT32_VR(vr) do{                              \
    int32_t print_data[16];                             \
    SA(o,vr,0,print_data,0);                \
    SA(o,vr,1,print_data+8,0);                \
    int i;                                              \
    printf("\n");                                       \
    for (i=0; i<16; i++) {                              \
      printf(" %4d",print_data[i]);            \
      if (i%8==7) printf("\n");                         \
    }                                                   \
  }while(0)

//print n 2-bit elements in a VR-reg in width=1
typedef struct {
  uint8_t   a:2;
  uint8_t   b:2;
  uint8_t   c:2;
  uint8_t   d:2;
}ubit2_t;
#define PRINT2_VR(vr,n) do{                                             \
    ubit2_t print_data[64];                                             \
    SA(o,vr,0,print_data,0);                        \
    SA(o,vr,1,print_data+32,0);                        \
    int i;                                                              \
    for (i=0; i<n/4; i++) {                                             \
      ubit2_t data = print_data[i];                                     \
      printf("%1d%1d%1d%1d ", data.a,data.b,data.c,data.d);             \
      if (i%16==15 && i!=(n/4-1)) printf("\n");                         \
    }                                                                   \
    printf("\n");                                                       \
  }while(0)

typedef struct {
  uint8_t   a:4;
  uint8_t   b:4;
}ubit4_t;
#define PRINT4_VR(vr,n) do{                                             \
    ubit4_t print_data[64];                                             \
    SA(o,vr,0,print_data,0);                        \
    SA(o,vr,1,print_data+32,0);                        \
    int i;                                                              \
    for (i=0; i<n/2; i++) {                                             \
      ubit4_t data = print_data[i];                                     \
      printf("%4d %4d ", data.a,data.b);                                \
      if (i%16==15 && i!=(n/2-1)) printf("\n");                         \
    }                                                                   \
    printf("\n");                                                       \
  }while(0)

//print n uint8 elements in a VR-reg in width=2
#define PRINT8_VR(vr,n) do{                                             \
    uint8_t print_data[64];                                             \
    SA(o,vr,0,print_data,0);                        \
    SA(o,vr,1,print_data+32,0);                        \
    int i;                                                              \
    for (i=0; i<n; i++) {                                               \
      printf(" %.2x", print_data[i]);                    \
      if (i%32==31) printf("\n");                                       \
    }                                                                   \
    printf("\n");                                                       \
  }while(0)

//print n 16-bit elements in a VR-reg
#define PRINTN16_VR(vr,n) do{                  \
    uint16_t print_data[32];                  \
    SA(o,vr,0,print_data,0);                  \
    SA(o,vr,1,print_data+16,0);                  \
    int i;                          \
    for (i=0; i<n; i++) printf(" %.4x ",print_data[i]);      \
    printf("\n");                      \
  }while(0)

//Print OCB
#define PRINTOCB(pixel,n) do{                                           \
    SAVE_VR31;                                                          \
    NNDRDOCB(VR31,pixel,n);                                             \
    if(n==0)                                                            \
        printf("OCB pixel=%d oc-num=0~15\t",pixel);                     \
    else                                                                \
        printf("OCB pixel=%d oc-num=16~31\t",pixel);                    \
    PRINT32_VR(VR31);                                                   \
    RESTORE_VR31;                                                       \
  } while(0)

//Print QR
#define PRINTQR do{                \
    SAVE_VR31;                    \
    NNDRDQR(VR31);                \
    printf("Result of QR for pixel 0~3\n");    \
    PRINT2_VR(VR31,128);            \
    RESTORE_VR31;                \
  } while(0)

//Print all NN Reg for MAC
#define PRINTREGM do{                                                   \
    SAVE_VR31;                                                          \
    NNRRDM(VR31);                                                       \
    uint16_t data[32];                                                  \
    SA(o,VR31,0,data,0);                        \
    SA(o,VR31,1,data+16,0);                        \
    RESTORE_VR31;                                                       \
    uint32_t addr[] =                                                   \
      {NNARA_WR_RADDR, NNARA_WR_RADDR_RST, NNARA_RCFL_E, NNARA_FR_RADDR, \
       NNARA_PAD_VALUE, NNARA_FP_H, NNARA_FP_W, \
       NNARA_MACTL_FP_Y, NNARA_MACTL_FP_X, NNARA_TLYP_ACC, NNARA_SF_MODE, \
       NNARA_MIN_FPN_O, NNARA_FR_RB_E, NNARA_F_BIT_GSIZE, NNARA_FR_FP_W, \
       NNARA_FR_FP_STRIDE, NNARA_MAC_STRIDE, NNARA_MAC_FP, NNARA_MAC_BIT, \
       NNARA_FP_NUM0, NNARA_FP_NUM1,                    \
       NNARA_MAC_W_MASK, NNARA_MAC_P_MASK};                \
    char name[][20] =                                                   \
      {"WR_RADDR", "WR_RADDR_RST", "RCFL_E", "FR_RADDR",        \
       "PAD_VALUE", "FP_H", "FP_W", "MACTL_FP_Y", "MACTL_FP_X",        \
       "TLYP_ACC", "SF_MODE", "MIN_FPN_O", "FR_RB_E", "F_BIT_GSIZE",     \
       "FR_FP_W", "FR_FP_STRIDE", "MAC_STRIDE", "MAC_FP", "MAC_BIT",    \
       "FP_NUM0", "FP_NUM1",                        \
       "MAC_W_MASK", "MAC_P_MASK"};                    \
    printf("All Registers for MAC are:\n");                             \
    int i;                                                              \
    for (i=0;i<sizeof(addr)/4;i++)                                      \
      printf("\t%s =\t0x%x\t%d\n", name[i],data[addr[i]],data[addr[i]]); \
  } while(0)

//Print all NN Reg for DATA
#define PRINTREGD do{                                                   \
    SAVE_VR31;                                                          \
    NNRRDD(VR31);                                                       \
    uint16_t data[32];                                                  \
    SA(o,VR31,0,data,0);                        \
    SA(o,VR31,1,data+16,0);                        \
    RESTORE_VR31;                                                       \
    uint32_t addr[] =                                                   \
      {NNARA_WR_WADDR,NNARA_FR_WADDR,NNARA_FR_WADDR_INC0,NNARA_FR_WADDR_INC1, \
       NNARA_FR_WADDR_INC2,NNARA_FR_WADDR_INC3,NNARA_FR_ALWAYS_INC, \
       NNARA_F_BIT, NNARA_MSF_MODE, NNARA_Q_BIT};            \
    char name[][20] =                                                   \
      {"WR_WADDR","FR_WADDR","FR_WADDR_INC0","FR_WADDR_INC1",        \
       "FR_WADDR_INC2","FR_WADDR_INC3","FR_ALWAYS_INC",            \
       "F_BIT", "MSF_MODE", "Q_BIT"};                    \
    printf("All Registers for W and F are:\n");                         \
    int i;                                                              \
    for (i=0;i<sizeof(addr)/4;i++)                                      \
      printf("\t%s =\t0x%x\t%d\n", name[i],data[addr[i]],data[addr[i]]); \
  } while(0)

//Print all NN Reg for General
#define PRINTREGG do{                                                   \
    SAVE_VR31;                                                          \
    NNRRDG(VR31);                                                       \
    uint16_t data[32];                                                  \
    SA(o,VR31,0,data,0);                        \
    SA(o,VR31,1,data+16,0);                        \
    RESTORE_VR31;                                                       \
    uint32_t addr[] =                            \
      {NNARA_EWSR, NNARA_CTRL, NNARA_INTE, NNARA_PC_FIFO_TH,        \
       NNARA_PC_INSN0_SEL, NNARA_PC_INSN1_SEL, NNARA_PC_INSN2_SEL,    \
       NNARA_PC_COUNT0_SEL, NNARA_PC_COUNT1_SEL, NNARA_PC_COUNT2_SEL};    \
    char name[][20] =                            \
      {"EWSR","CTRL","INTE","FIFO_TH",                    \
       "INSN0_SEL","INSN1_SEL","INSN2_SEL","CNT0_SEL","CNT1_SEL","CNT2_SEL",}; \
    printf("All General Registers are:\n");                             \
    int i;                                                              \
    for (i=0;i<sizeof(addr)/4;i++)                                      \
      printf("\t%s =\t0x%x\t%d\n", name[i],data[addr[i]],data[addr[i]]); \
  } while(0)

//Print PC G0
#define PRINTPCG0 do{                                                   \
    SAVE_VR31;                                                          \
    NNRRDPCG0(VR31);                                                    \
    uint32_t data[16];                                                  \
    SA(o,VR31,0,data,0);                        \
    SA(o,VR31,1,data+8,0);                        \
    RESTORE_VR31;                                                       \
    uint32_t addr[]= {NNAPCG0_MACOP_FREX_NUM,NNAPCG0_WAIT_LSRS};        \
    char name[][20] = {"MACOP_FREX_NUM","WAIT_LSRS"};            \
    printf("Performance Counter Group 0 are:\n");                       \
    int i;                                                              \
    for (i=0;i<sizeof(addr)/4;i++)                                      \
      printf("\t%s =\t%d\n", name[i],data[addr[i]]);                    \
  } while(0)

//Print PC G1

#define PRINTPCG1 do{                                                   \
    SAVE_VR31;                                                          \
    NNRRDPCG1(VR31);                                                    \
    uint64_t data[8];                                                   \
    SA(o,VR31,0,data,0);                        \
    SA(o,VR31,1,data+4,0);                        \
    RESTORE_VR31;                                                       \
    uint32_t addr[] =                            \
      {NNAPCG1_CYCLE_D,NNAPCG1_INSN0_D,NNAPCG1_INSN1_D,NNAPCG1_INSN2_D,    \
       NNAPCG1_COUNT0_D,NNAPCG1_COUNT1_D,NNAPCG1_COUNT2_D};        \
    char name[][20] =                            \
      {"CYCLE","INSN0","INSN1","INSN2","COUNT0","COUNT1","COUNT2"};    \
    printf("Performance Counter Group 1 are:\n");                       \
    int i;                                                              \
    for (i=0;i<sizeof(addr)/4;i++)                                      \
      printf("\t%s =\t%d\n", name[i],data[addr[i]]);                    \
  } while(0)

#endif /* __NNA_INSN_H__ */
