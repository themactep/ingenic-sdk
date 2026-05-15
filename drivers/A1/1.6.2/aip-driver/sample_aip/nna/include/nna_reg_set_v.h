/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : nna_reg_setv.h
 * Authors    : jzhang@elim.ic.jz.com
 * Create Time: 2020-04-24:15:40:50
 * Description: set NNA reg variable
 *              use after include nna_reg_set_c.h
 *              set all needed REG_<reg-name>'s value by
 * #define REG_<reg-name> <value>
 *              Where <value> can be a variable or an expression. Then include
 *              this file. This may clob all VWR registers
 */

#ifndef __NNA_REG_SET_V_H__
#define __NNA_REG_SET_V_H__

reg_index = 0;

#define SET_REG_VALUE(reg) reg_value[reg_index] = REG_##reg; reg_index++

#ifdef REG_WR_RADDR
SET_REG_VALUE(WR_RADDR);
#endif
#ifdef REG_WR_RADDR_RST
SET_REG_VALUE(WR_RADDR_RST);
#endif
#ifdef REG_RCFL_E
SET_REG_VALUE(RCFL_E);
#endif
#ifdef REG_PAD_VALUE
SET_REG_VALUE(PAD_VALUE);
#endif
#ifdef REG_FP_H
SET_REG_VALUE(FP_H);
#endif
#ifdef REG_FP_W
SET_REG_VALUE(FP_W);
#endif
#ifdef REG_MACTL_FP_Y
SET_REG_VALUE(FP_MACTL_FP_Y);
#endif
#ifdef REG_MACTL_FP_X
SET_REG_VALUE(FP_MACTL_FP_X);
#endif
#ifdef REG_TLYP_ACC
SET_REG_VALUE(TLYP_ACC);
#endif
#ifdef REG_SF_MODE
SET_REG_VALUE(SF_MODE);
#endif
#ifdef REG_MIN_FPN_O
SET_REG_VALUE(MIN_FPN_O);
#endif
#ifdef REG_FR_RB_E
SET_REG_VALUE(FR_RB_E);
#endif
#ifdef REG_F_BIT_GSIZE
SET_REG_VALUE(F_BIT_GSIZE);
#endif
#ifdef REG_FR_FP_W
SET_REG_VALUE(FR_FP_W);
#endif
#ifdef REG_FR_FP_STRIDE
SET_REG_VALUE(FR_FP_STRIDE);
#endif
#ifdef REG_MAC_STRIDE
SET_REG_VALUE(MAC_STRIDE);
#endif
#ifdef REG_MAC_FP
SET_REG_VALUE(MAC_FP);
#endif
#ifdef REG_MAC_BIT
SET_REG_VALUE(MAC_BIT);
#endif
#ifdef REG_FP_NUM0
SET_REG_VALUE(FP_NUM0);
#endif
#ifdef REG_FP_NUM1
SET_REG_VALUE(FP_NUM1);
#endif
#ifdef REG_MAC_W_MASK
SET_REG_VALUE(MAC_W_MASK);
#endif
#ifdef REG_MAC_P_MASK
SET_REG_VALUE(MAC_P_MASK);
#endif
#ifdef REG_WR_WADDR
SET_REG_VALUE(WR_WADDR);
#endif
#ifdef REG_FR_WADDR
SET_REG_VALUE(FR_WADDR);
#endif
#ifdef REG_FR_WADDR_INC0
SET_REG_VALUE(FR_WADDR_INC0);
#endif
#ifdef REG_FR_WADDR_INC1
SET_REG_VALUE(FR_WADDR_INC1);
#endif
#ifdef REG_FR_WADDR_INC2
SET_REG_VALUE(FR_WADDR_INC2);
#endif
#ifdef REG_FR_WADDR_INC3
SET_REG_VALUE(FR_WADDR_INC3);
#endif
#ifdef REG_FR_ALWAYS_INC
SET_REG_VALUE(FR_ALWAYS_INC);
#endif
#ifdef REG_F_BIT
SET_REG_VALUE(F_BIT);
#endif
#ifdef REG_MSF_MODE
SET_REG_VALUE(MSF_MODE);
#endif
#ifdef REG_Q_BIT
SET_REG_VALUE(Q_BIT);
#endif

#undef SET_REG_VALUE

reg_index_max = reg_index;
if (reg_index > 0) LA(o,VR31,0,reg_value,0);
if (reg_index > 8) LA(o,VR31,1,reg_value,32);
if (reg_index > 16) {
  printf ("Error: reg number need to set is %d > 16\n", reg_index);
  exit(-1);
 }

#define NNRWR_VWR(type,reg)            \
  if (reg_index ==  0) NNRWR##type(VWR0, reg);    \
  if (reg_index ==  1) NNRWR##type(VWR1, reg);    \
  if (reg_index ==  2) NNRWR##type(VWR2, reg);    \
  if (reg_index ==  3) NNRWR##type(VWR3, reg);    \
  if (reg_index ==  4) NNRWR##type(VWR4, reg);    \
  if (reg_index ==  5) NNRWR##type(VWR5, reg);    \
  if (reg_index ==  6) NNRWR##type(VWR6, reg);    \
  if (reg_index ==  7) NNRWR##type(VWR7, reg);    \
  if (reg_index ==  8) NNRWR##type(VWR8, reg);    \
  if (reg_index ==  9) NNRWR##type(VWR9, reg);    \
  if (reg_index == 10) NNRWR##type(VWR10,reg);    \
  if (reg_index == 11) NNRWR##type(VWR11,reg);    \
  if (reg_index == 12) NNRWR##type(VWR12,reg);    \
  if (reg_index == 13) NNRWR##type(VWR13,reg);    \
  if (reg_index == 14) NNRWR##type(VWR14,reg);    \
  if (reg_index == 15) NNRWR##type(VWR15,reg)

#define NNRWR_REG_VALUE(type,reg) NNRWR_VWR(type,NNARA_##reg); reg_index++

reg_index = 0;

#ifdef REG_WR_RADDR
NNRWR_REG_VALUE(M,WR_RADDR);
#undef REG_WR_RADDR
#endif
#ifdef REG_WR_RADDR_RST
NNRWR_REG_VALUE(M,WR_RADDR_RST);
#undef REG_WR_RADDR_RST
#endif
#ifdef REG_RCFL_E
NNRWR_REG_VALUE(M,RCFL_E);
#undef REG_RCFL_E
#endif
#ifdef REG_PAD_VALUE
NNRWR_REG_VALUE(M,PAD_VALUE);
#undef REG_PAD_VALUE
#endif
#ifdef REG_FP_H
NNRWR_REG_VALUE(M,FP_H);
#undef REG_FP_H
#endif
#ifdef REG_FP_W
NNRWR_REG_VALUE(M,FP_W);
#undef REG_FP_W
#endif
#ifdef REG_MACTL_FP_Y
NNRWR_REG_VALUE(M,MACTL_FP_Y);
#undef REG_MACTL_FP_Y
#endif
#ifdef REG_MACTL_FP_X
NNRWR_REG_VALUE(M,MACTL_FP_X);
#undef REG_MACTL_FP_X
#endif
#ifdef REG_TLYP_ACC
NNRWR_REG_VALUE(M,TLYP_ACC);
#undef REG_TLYP_ACC
#endif
#ifdef REG_SF_MODE
NNRWR_REG_VALUE(M,SF_MODE);
#undef REG_SF_MODE
#endif
#ifdef REG_MIN_FPN_O
NNRWR_REG_VALUE(M,MIN_FPN_O);
#undef REG_MIN_FPN_O
#endif
#ifdef REG_FR_RB_E
NNRWR_REG_VALUE(M,FR_RB_E);
#undef REG_FR_RB_E
#endif
#ifdef REG_F_BIT_GSIZE
NNRWR_REG_VALUE(M,F_BIT_GSIZE);
#undef REG_F_BIT_GSIZE
#endif
#ifdef REG_FR_FP_W
NNRWR_REG_VALUE(M,FR_FP_W);
#undef REG_FR_FP_W
#endif
#ifdef REG_FR_FP_STRIDE
NNRWR_REG_VALUE(M,FR_FP_STRIDE);
#undef REG_FR_FP_STRIDE
#endif
#ifdef REG_MAC_STRIDE
NNRWR_REG_VALUE(M,MAC_STRIDE);
#undef REG_MAC_STRIDE
#endif
#ifdef REG_MAC_FP
NNRWR_REG_VALUE(M,MAC_FP);
#undef REG_MAC_FP
#endif
#ifdef REG_MAC_BIT
NNRWR_REG_VALUE(M,MAC_BIT);
#undef REG_MAC_BIT
#endif
#ifdef REG_FP_NUM0
NNRWR_REG_VALUE(M,FP_NUM0);
#undef REG_FP_NUM0
#endif
#ifdef REG_FP_NUM1
NNRWR_REG_VALUE(M,FP_NUM1);
#undef REG_FP_NUM1
#endif
#ifdef REG_MAC_W_MASK
NNRWR_REG_VALUE(M,MAC_W_MASK);
#undef REG_MAC_W_MASK
#endif
#ifdef REG_MAC_P_MASK
NNRWR_REG_VALUE(M,MAC_P_MASK);
#undef REG_MAC_P_MASK
#endif
#ifdef REG_WR_WADDR
NNRWR_REG_VALUE(D,WR_WADDR);
#undef REG_WR_WADDR
#endif
#ifdef REG_FR_WADDR
NNRWR_REG_VALUE(D,FR_WADDR);
#undef REG_FR_WADDR
#endif
#ifdef REG_FR_WADDR_INC0
NNRWR_REG_VALUE(D,FR_WADDR_INC0);
#undef REG_FR_WADDR_INC0
#endif
#ifdef REG_FR_WADDR_INC1
NNRWR_REG_VALUE(D,FR_WADDR_INC1);
#undef REG_FR_WADDR_INC1
#endif
#ifdef REG_FR_WADDR_INC2
NNRWR_REG_VALUE(D,FR_WADDR_INC2);
#undef REG_FR_WADDR_INC2
#endif
#ifdef REG_FR_WADDR_INC3
NNRWR_REG_VALUE(D,FR_WADDR_INC3);
#undef REG_FR_WADDR_INC3
#endif
#ifdef REG_FR_ALWAYS_INC
NNRWR_REG_VALUE(D,FR_ALWAYS_INC);
#undef REG_FR_ALWAYS_INC
#endif
#ifdef REG_F_BIT
NNRWR_REG_VALUE(D,F_BIT);
#undef REG_F_BIT
#endif
#ifdef REG_MSF_MODE
NNRWR_REG_VALUE(D,MSF_MODE);
#undef REG_MSF_MODE
#endif
#ifdef REG_Q_BIT
NNRWR_REG_VALUE(D,Q_BIT);
#undef REG_Q_BIT
#endif

#undef NNRWR_REG_VALUE
#undef NNRWR_VWR

#endif /* __NNA_REG_SET_V_H__ */

