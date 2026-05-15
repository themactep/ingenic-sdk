/*
 *        (C) COPYRIGHT Ingenic Limited.
 *             ALL RIGHTS RESERVED
 *
 * File       : nna_regs.h
 * Authors    : jzhang@elim.ic.jz.com
 * Create Time: 2018-12-12:15:45:37
 * Description: NNA register config and read macro
 *
 */

#ifndef __NNA_REGS_H__
#define __NNA_REGS_H__

#include "nna.h"
#include "nna_insn.h"
/*
   1. write v or vx,vy to NN register, vwr used
     (1) write a constant: WC_reg-name
     (2) write a variable: WV_reg-name
   2. read a NN register, vr31' word0 or vwr0 used
         R_reg-name
*/
/****** 1.(1) write a constant to NNA register ******/
//MAC
#define WC_WR_RADDR(c,vwr) do{LIWR(vwr,c); NNRWRM(vwr,NNARA_WR_RADDR);} while(0)
#define WC_WR_RADDR_RST(c,vwr) do{                              \
    LIWR(vwr,c); NNRWRM(vwr,NNARA_WR_RADDR_RST);} while(0)

#define WC_TLYP_ACC(c,vwr) do{LIWR(vwr,c); NNRWRM(vwr,NNARA_TLYP_ACC);} while(0)
#define WC_MSF_MODE(c,vwr) do{LIWR(vwr,c); NNRWRM(vwr,NNARA_MSF_MODE);} while(0)
#define WC_F1_DOWN(c,vwr) do{LIWR(vwr,c); NNRWRM(vwr,NNARA_F1_DOWN);} while(0)

#define WC_FR_FP_W(c,vwr) do{LIWR(vwr,c); NNRWRM(vwr,NNARA_FR_FP_W);} while(0)
#define WC_FR_FP_STRIDE(c,vwr) do{                              \
    LIWR(vwr,c); NNRWRM(vwr,NNARA_FR_FP_STRIDE);} while(0)
#define WC_MAC_STRIDE(h,w,vwr) do{                                      \
    LIWR(vwr,(w-1)|((h-1)<<4)); NNRWRM(vwr,NNARA_MAC_STRIDE);} while(0)
#define WC_MAC_FP(lg2h,lg2w,vwr) do{                                    \
    LIWR(vwr,lg2w|(lg2h<<4)); NNRWRM(vwr,NNARA_MAC_FP);} while(0)
#define WC_MAC_BIT(fbit,wbit,vwr) do{                                   \
    LIWR(vwr,(fbit/2-1)|((wbit/2-1)<<4)); NNRWRM(vwr,NNARA_MAC_BIT);} while(0)
#define WC_FP_NUM0(n0,vwr) do{                                      \
    LIWR(vwr,n0-1); NNRWRM(vwr,NNARA_FP_NUM0);} while(0)
#define WC_FP_NUM1(n1,vwr) do{                                      \
    LIWR(vwr,n1-1); NNRWRM(vwr,NNARA_FP_NUM1);} while(0)
//DATA
#define WC_WR_WADDR(c,vwr) do{LIWR(vwr,c); NNRWRD(vwr,NNARA_WR_WADDR);} while(0)
#define WC_FR_WADDR(c,vwr) do{LIWR(vwr,c); NNRWRD(vwr,NNARA_FR_WADDR);} while(0)
#define WC_FR_WADDR_INC0(c,vwr) do{                             \
    LIWR(vwr,c); NNRWRD(vwr,NNARA_FR_WADDR_INC0);} while(0)
#define WC_FR_WADDR_INC1(c,vwr) do{                             \
    LIWR(vwr,c); NNRWRD(vwr,NNARA_FR_WADDR_INC1);} while(0)
#define WC_FR_ALWAYS_INC(c,vwr) do{                             \
    LIWR(vwr,c); NNRWRD(vwr,NNARA_FR_ALWAYS_INC);} while(0)
#define WC_F_BIT(c,vwr) do{LIWR(vwr,c); NNRWRD(vwr,NNARA_F_BIT);} while(0)
//G
#define W0_PCEG0(vwr) do {                                              \
    LIWR(vwr,1<<NNARA_CTRL_PCEG0_BIT); NNRWRG(vwr,NNARA_CTRL_W0);} while(0)
#define W0_PCEG1(vwr) do {                                              \
    LIWR(vwr,1<<NNARA_CTRL_PCEG1_BIT); NNRWRG(vwr,NNARA_CTRL_W0);} while(0)
#define W0_PCEG2(vwr) do {                                              \
    LIWR(vwr,1<<NNARA_CTRL_PCEG2_BIT); NNRWRG(vwr,NNARA_CTRL_W0);} while(0)
#define W1_PCEG0(vwr) do {                                              \
    LIWR(vwr,1<<NNARA_CTRL_PCEG0_BIT); NNRWRG(vwr,NNARA_CTRL_W1);} while(0)
#define W1_PCEG1(vwr) do {                                              \
    LIWR(vwr,1<<NNARA_CTRL_PCEG1_BIT); NNRWRG(vwr,NNARA_CTRL_W1);} while(0)
#define W1_PCEG2(vwr) do {                                              \
    LIWR(vwr,1<<NNARA_CTRL_PCEG2_BIT); NNRWRG(vwr,NNARA_CTRL_W1);} while(0)
#define W1_CLREW(vwr) do {                                              \
    LIWR(vwr,1<<NNARA_CTRL_CLREW_BIT); NNRWRG(vwr,NNARA_CTRL_W1);} while(0)
#define W1_CLRPC(vwr) do {                                              \
    LIWR(vwr,1<<NNARA_CTRL_CLRPC_BIT); NNRWRG(vwr,NNARA_CTRL_W1);} while(0)
#define W0_INTE_INVALID_INSN(vwr) do {                                  \
    LIWR(vwr,1<<NNARA_INVALID_INSN_BIT); NNRWRG(vwr,NNARA_INTE_W0);} while(0)
#define W0_INTE_PMISSBYFR(vwr) do {                                     \
    LIWR(vwr,1<<NNARA_PMISSBYFR_BIT); NNRWRG(vwr,NNARA_INTE_W0);} while(0)
#define W1_INTE_INVALID_INSN(vwr) do {                                  \
    LIWR(vwr,1<<NNARA_INVALID_INSN_BIT); NNRWRG(vwr,NNARA_INTE_W1);} while(0)
#define W1_INTE_PMISSBYFR(vwr) do {                                     \
    LIWR(vwr,1<<NNARA_PMISSBYFR_BIT); NNRWRG(vwr,NNARA_INTE_W1);} while(0)

/****** 1.(2) write a variable to NNA register ******/
//MAC
#define WV_WR_RADDR(v,vwr) do{                                  \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_WR_RADDR);} while(0)
#define WV_WR_RADDR_RST(v,vwr) do{                              \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_WR_RADDR_RST);} while(0)

#define WV_FR_RADDR(v,vwr) do{                                  \
    uint32_t data[1]; data[0]=v;LA(w,VR31,vwr,data,0);} while(0)

#define WV_FP_H(v,vwr) do{                                      \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_FP_H);} while(0)
#define WV_FP_W(v,vwr) do{                                      \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_FP_W);} while(0)
#define WV_MACTL_FP_Y(v,vwr) do{                                        \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_MACTL_FP_Y);} while(0)
#define WV_MACTL_FP_X(v,vwr) do{                                        \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_MACTL_FP_X);} while(0)
#define WV_TLYP_ACC(v,vwr) do{                                  \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_TLYP_ACC);} while(0)
#define WV_MSF_MODE(v,vwr) do{                                  \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_MSF_MODE);} while(0)
#define WV_F1_DOWN(v,vwr) do{                                   \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_F1_DOWN);} while(0)

#define WV_FR_FP_W(v,vwr) do{                                   \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_FR_FP_W);} while(0)
#define WV_FR_FP_STRIDE(v,vwr) do{                              \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_FR_FP_STRIDE);} while(0)
#define WV_MAC_STRIDE(y,x,vwr) do{                                      \
    uint32_t data[1]; data[0]=(x-1)|((y-1)<<4);                         \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_MAC_STRIDE);} while(0)
#define WV_MAC_FP(lg2h,lg2w,vwr) do{                                    \
    uint32_t data[1]; data[0]=lg2w|(lg2h<<4);                           \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_MAC_FP);} while(0)
#define WV_MAC_BIT(fbit,wbit,vwr) do{                                   \
    uint32_t data[1]; data[0]=(fbit/2-1)|((wbit/2-1)<<4);               \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_MAC_BIT);} while(0)
#define WV_FP_NUM0(n0,vwr) do{                                      \
    uint32_t data[1]; data[0]=n0-1;                                 \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_FP_NUM0);} while(0)
#define WV_FP_NUM1(n1,vwr) do{                                      \
    uint32_t data[1]; data[0]=n1-1;                                 \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_FP_NUM1);} while(0)
#define WV_MAC_W_MASK(v,vwr) do{                                        \
    uint32_t data[1]; data[0]=v;                                        \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_MAC_W_MASK);} while(0)
#define WV_MAC_P_MASK(v,vwr) do{                                        \
    uint32_t data[1]; data[0]=v;                                        \
    LA(w,VR31,vwr,data,0); NNRWRM(vwr,NNARA_MAC_P_MASK);} while(0)

//DATA
#define WV_WR_WADDR(v,vwr) do{                                          \
    uint32_t data[1]; data[0]=v;                                        \
    LA(w,VR31,vwr,data,0); NNRWRD(vwr,NNARA_WR_WADDR);} while(0)
#define WV_FR_WADDR(v,vwr) do{                                          \
    uint32_t data[1]; data[0]=v;                                        \
    LA(w,VR31,vwr,data,0); NNRWRD(vwr,NNARA_FR_WADDR);} while(0)
#define WV_FR_WADDR_INC0(v,vwr) do{                                     \
    uint32_t data[1]; data[0]=v;                                        \
    LA(w,VR31,vwr,data,0); NNRWRD(vwr,NNARA_FR_WADDR_INC0);} while(0)
#define WV_FR_WADDR_INC1(v,vwr) do{                                     \
    uint32_t data[1]; data[0]=v;                                        \
    LA(w,VR31,vwr,data,0); NNRWRD(vwr,NNARA_FR_WADDR_INC1);} while(0)
#define WV_FR_ALWAYS_INC(v,vwr) do{                                     \
    uint32_t data[1]; data[0]=v;                                        \
    LA(w,VR31,vwr,data,0); NNRWRD(vwr,NNARA_FR_ALWAYS_INC);} while(0)
#define WV_F_BIT(v,vwr) do{                                     \
    uint32_t data[1]; data[0]=v;                                \
    LA(w,VR31,vwr,data,0); NNRWRD(vwr,NNARA_F_BIT);} while(0)

/****** 1.(2) write a vwr to NNA register ******/
#define WR_MACTL_FP_Y(vwr) do{NNRWRM(vwr,NNARA_MACTL_FP_Y);} while(0)
#define WR_MACTL_FP_X(vwr) do{NNRWRM(vwr,NNARA_MACTL_FP_X);} while(0)

#define WR_FR_WADDR(vwr) do{NNRWRD(vwr,NNARA_FR_WADDR);} while(0)

// 3. write data
// 3.(1) write W
#define NNA_WRW_P0      0
#define NNA_WRW_P1      1
#define NNA_WRW_P2      2
#define NNA_WRW_P3      3
#define NNA_WRW_INCA    1
#define NNA_WRW_NONINCA 0
// 3.(2) write F
//#define NNA_WRF_SWAP  1
//#define NNA_WRF_NONSWAP       0
#define NNA_WRF_INCA0   0
#define NNA_WRF_INCA1   1
#define NNA_WRF_OFFSET0 0
#define NNA_WRF_OFFSET1 1
#define NNA_WRF_OFFSET2 2
#define NNA_WRF_OFFSET3 3
#define NNA_WRF_4P      0
#define NNA_WRF_8P      1
#define NNA_WRF_INVMSB  1
#define NNA_WRF_NOINVMSB        0
#define NNA_WRF_2BIT    0
#define NNA_WRF_HIGHBIT 0

// 4. read data -- read OCB
#define NNA_OCB0        0
#define NNA_OCB1        1
#define NNA_OCB2        2
#define NNA_OCB3        3
#define NNA_LOCH        0
#define NNA_HICH        1

// 5. MAC
#define NNA_MAC_RDW_RSTA        0
#define NNA_MAC_RDW_NONRSTA     1
#define NNA_FP_NUM0             0
#define NNA_FP_NUM1             1

// 6. hint in LD/ST
#define LDST_HINT_NORMAL        0
#define LDST_HINT_BYPASSL1      1

#endif /* __NNA_REGS_H__ */

