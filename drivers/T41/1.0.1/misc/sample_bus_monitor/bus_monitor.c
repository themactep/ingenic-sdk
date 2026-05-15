/*
 * Copyright (c) 2015 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Input file for Ingenic bus_monitor driver
 *
 * This  program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/suspend.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/time.h>
#include <soc/base.h>
#include <dt-bindings/interrupt-controller/t41-irq.h>
#include "bus_monitor.h"

static unsigned int monitor_chn = 0;
static unsigned int irq_num = 0;

//#define DEBUG
#ifdef	DEBUG
static int debug_monitor = 1;

#define MONITOR_DEBUG(format, ...) { if (debug_monitor) printk(format, ## __VA_ARGS__);}
#else
#define MONITOR_DEBUG(format, ...) do{ } while(0)
#endif

#define MONITOR_BUF_SIZE (1024 * 1024 * 2)

struct monitor_reg_struct jz_monitor_regs_name[] = {

};

int dump_chx_all_reg_value(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    struct monitor_param *ip = monitor_param;
    MONITOR_DEBUG("[%s,%d] chn =  %d\n",__func__,__LINE__,monitor_chn);

    switch(ip->monitor_mode){
        case MONITOR_CHK_ID:
            printk("----->MONITOR_AWID_ZONE0_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWID_ZONE0_NUM(monitor_chn)));
            printk("----->MONITOR_AWID_ZONE1_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWID_ZONE1_NUM(monitor_chn)));
            printk("----->MONITOR_AWID_ZONE2_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWID_ZONE2_NUM(monitor_chn)));
            printk("----->MONITOR_ARID_ZONE0_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARID_ZONE0_NUM(monitor_chn)));
            printk("----->MONITOR_ARID_ZONE1_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARID_ZONE1_NUM(monitor_chn)));
            printk("----->MONITOR_ARID_ZONE2_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARID_ZONE2_NUM(monitor_chn)));
            printk("----->MONITOR_WID_ZONE0_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WID_ZONE0_NUM(monitor_chn)));
            printk("----->MONITOR_WID_ZONE1_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WID_ZONE1_NUM(monitor_chn)));
            printk("----->MONITOR_WID_ZONE2_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WID_ZONE2_NUM(monitor_chn)));
            printk("----->MONITOR_RID_ZONE0_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_RID_ZONE0_NUM(monitor_chn)));
            printk("----->MONITOR_RID_ZONE1_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_RID_ZONE1_NUM(monitor_chn)));
            printk("----->MONITOR_RID_ZONE2_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_RID_ZONE2_NUM(monitor_chn)));
            printk("----->MONITOR_BID_ZONE0_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_BID_ZONE0_NUM(monitor_chn)));
            printk("----->MONITOR_BID_ZONE1_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_BID_ZONE1_NUM(monitor_chn)));
            printk("----->MONITOR_BID_ZONE2_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_BID_ZONE2_NUM(monitor_chn)));
            reg_write(monitor, MONITOR_CHKID_FUN_CLR(monitor_chn), 0x11111);
            break;
        case MONITOR_CHK_ADDR:
        case MONITOR_ID_ADDR:
            printk("----->MONITOR_AWAADDR_ZONE0_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWAADDR_ZONE0_NUM(monitor_chn)));
            printk("----->MONITOR_AWAADDR_ZONE1_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWAADDR_ZONE1_NUM(monitor_chn)));
            printk("----->MONITOR_AWAADDR_ZONE2_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWAADDR_ZONE2_NUM(monitor_chn)));
            printk("----->MONITOR_ARAADDR_ZONE0_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARAADDR_ZONE0_NUM(monitor_chn)));
            printk("----->MONITOR_ARAADDR_ZONE1_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARAADDR_ZONE1_NUM(monitor_chn)));
            printk("----->MONITOR_ARAADDR_ZONE2_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARAADDR_ZONE2_NUM(monitor_chn)));
            reg_write(monitor, MONITOR_CHKADDR_FUN_CLR(monitor_chn), 0x11);
            break;
        case MONITOR_VALS:
            if(monitor_chn  != 5 && monitor_chn != 7){
                printk("----->MONITOR_AWVAL_CLK_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWVAL_CLK_NUM(monitor_chn)));
                printk("----->MONITOR_AWVAL_VLD_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWVAL_VLD_NUM(monitor_chn)));
                printk("----->MONITOR_AWVAL_RDY_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWVAL_RDY_NUM(monitor_chn)));
                printk("----->MONITOR_AWVAL_HIT_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWVAL_HIT_NUM(monitor_chn)));
                printk("----->MONITOR_ARVAL_CLK_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARVAL_CLK_NUM(monitor_chn)));
                printk("----->MONITOR_ARVAL_VLD_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARVAL_VLD_NUM(monitor_chn)));
                printk("----->MONITOR_ARVAL_RDY_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARVAL_RDY_NUM(monitor_chn)));
                printk("----->MONITOR_ARVAL_HIT_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARVAL_HIT_NUM(monitor_chn)));
                printk("----->MONITOR_WVAL_CLK_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_CLK_NUM(monitor_chn)));
                printk("----->MONITOR_WVAL_VLD_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_VLD_NUM(monitor_chn)));
                printk("----->MONITOR_WVAL_RDY_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_RDY_NUM(monitor_chn)));
                printk("----->MONITOR_WVAL_HIT_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_HIT_NUM(monitor_chn)));
                printk("----->MONITOR_RVAL_CLK_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_RVAL_CLK_NUM(monitor_chn)));
                printk("----->MONITOR_RVAL_VLD_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_RVAL_VLD_NUM(monitor_chn)));
                printk("----->MONITOR_RVAL_RDY_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_RVAL_RDY_NUM(monitor_chn)));
                printk("----->MONITOR_RVAL_HIT_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_RVAL_HIT_NUM(monitor_chn)));
                printk("----->MONITOR_BVAL_CLK_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_BVAL_CLK_NUM(monitor_chn)));
                printk("----->MONITOR_BVAL_VLD_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_BVAL_VLD_NUM(monitor_chn)));
                printk("----->MONITOR_BVAL_RDY_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_BVAL_RDY_NUM(monitor_chn)));
                printk("----->MONITOR_BVAL_HIT_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_BVAL_HIT_NUM(monitor_chn)));
                reg_write(monitor, MONITOR_CHKVALS_FUN_CLR(monitor_chn), 0x1041041);
            }else{
                printk("----->MONITOR_AWVAL_CLK_NUM(%d)	 = 0x%x \n", 0, reg_read(monitor,MONITOR_AWVAL_CLK_NUM(0)));
                printk("----->MONITOR_AWVAL_CLK_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWVAL_CLK_NUM(monitor_chn)));
                printk("----->MONITOR_AWVAL_VLD_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWVAL_VLD_NUM(monitor_chn)));
                printk("----->MONITOR_ARVAL_CLK_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARVAL_CLK_NUM(monitor_chn)));
                printk("----->MONITOR_ARVAL_VLD_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARVAL_VLD_NUM(monitor_chn)));
                printk("----->MONITOR_WVAL_CLK_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_CLK_NUM(monitor_chn)));
                printk("----->MONITOR_WVAL_VLD_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_VLD_NUM(monitor_chn)));
                printk("----->MONITOR_WVAL_RDY_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_RDY_NUM(monitor_chn)));
                printk("----->MONITOR_WVAL_HIT_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_HIT_NUM(monitor_chn)));
                reg_write(monitor, MONITOR_CHKVALS_FUN_CLR(monitor_chn), 0x11);
            }
            break;
        case MONITOR_VALM:
            printk("----->MONITOR_AWVAL_MAX_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_AWVAL_MAX_NUM(monitor_chn)));
            printk("----->MONITOR_ARVAL_MAX_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_ARVAL_MAX_NUM(monitor_chn)));
            printk("----->MONITOR_WVAL_MAX_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_WVAL_MAX_NUM(monitor_chn)));
            printk("----->MONITOR_RVAL_MAX_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_RVAL_MAX_NUM(monitor_chn)));
            printk("----->MONITOR_BVAL_MAX_NUM(%d)	 = 0x%x \n",monitor_chn, reg_read(monitor,MONITOR_BVAL_MAX_NUM(monitor_chn)));
            //printk("----->(%d)	 = 0x%x",monitor_chn, reg_read(monitor,(monitor_chn)));
            reg_write(monitor, MONITOR_CHKVALM_FUN_CLR(monitor_chn), 0x1041041);
            break;
        default:
            MONITOR_DEBUG("not support this mode !!!");
            break;
    }
    printk("----->irq_num num = %d\n",irq_num);
    irq_num = 0;

    return 0;
}


static void reg_bit_set(struct jz_monitor *monitor, int offset, unsigned int bit)
{
	unsigned int reg = 0;
	reg = reg_read(monitor, offset);
	reg |= bit;
	reg_write(monitor, offset, reg);
}

static void reg_bit_clr(struct jz_monitor *monitor, int offset, unsigned int bit)
{
	unsigned int reg = 0;
	reg= reg_read(monitor, offset);
	reg &= ~(bit);
	reg_write(monitor, offset, reg);
}


static int _monitor_dump_regs(struct jz_monitor *monitor)
{

	return 0;
}

static int monitor_reg_set(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    return 0;
}

static int set_id_switch_fun(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    struct monitor_param *ip = monitor_param;
    MONITOR_DEBUG("[%s,%d] chn =  %d\n",__func__,__LINE__,monitor_chn);

    switch(ip->monitor_id){
        case MONITOR_LDC:
            reg_write(monitor, MONITOR_AWID_VALUE(monitor_chn), ((CHN_LDC(CHN_NUM_LDC)) + ((CHN_LDC(CHN_NUM_LDC + 1)) << 8)));
            reg_write(monitor, MONITOR_ARID_VALUE(monitor_chn), ((CHN_LDC(CHN_NUM_LDC)) + ((CHN_LDC(CHN_NUM_LDC + 1)) << 8)));
            reg_write(monitor, MONITOR_WID_VALUE(monitor_chn), ((CHN_LDC(CHN_NUM_LDC)) + ((CHN_LDC(CHN_NUM_LDC + 1)) << 8)));
            reg_write(monitor, MONITOR_RID_VALUE(monitor_chn), ((CHN_LDC(CHN_NUM_LDC)) + ((CHN_LDC(CHN_NUM_LDC + 1)) << 8)));
            reg_write(monitor, MONITOR_BID_VALUE(monitor_chn), ((CHN_LDC(CHN_NUM_LDC)) + ((CHN_LDC(CHN_NUM_LDC + 1)) << 8)));
            break;
        case MONITOR_LCDC:
            reg_write(monitor, MONITOR_AWID_VALUE(monitor_chn), ((CHN_LCDC(CHN_NUM_LCDC)) + ((CHN_LCDC(CHN_NUM_LCDC + 1)) << 8)));
            reg_write(monitor, MONITOR_ARID_VALUE(monitor_chn), ((CHN_LCDC(CHN_NUM_LCDC)) + ((CHN_LCDC(CHN_NUM_LCDC + 1)) << 8)));
            reg_write(monitor, MONITOR_WID_VALUE(monitor_chn), ((CHN_LCDC(CHN_NUM_LCDC)) + ((CHN_LCDC(CHN_NUM_LCDC + 1)) << 8)));
            reg_write(monitor, MONITOR_RID_VALUE(monitor_chn), ((CHN_LCDC(CHN_NUM_LCDC)) + ((CHN_LCDC(CHN_NUM_LCDC + 1)) << 8)));
            reg_write(monitor, MONITOR_BID_VALUE(monitor_chn), ((CHN_LCDC(CHN_NUM_LCDC)) + ((CHN_LCDC(CHN_NUM_LCDC + 1)) << 8)));
            break;
        case MONITOR_AIP:
            reg_write(monitor, MONITOR_AWID_VALUE(monitor_chn), ((CHN_AIP(CHN_NUM_AIP)) + ((CHN_AIP(CHN_NUM_AIP + 1)) << 8)));
            reg_write(monitor, MONITOR_ARID_VALUE(monitor_chn), ((CHN_AIP(CHN_NUM_AIP)) + ((CHN_AIP(CHN_NUM_AIP + 1)) << 8)));
            reg_write(monitor, MONITOR_WID_VALUE(monitor_chn), ((CHN_AIP(CHN_NUM_AIP)) + ((CHN_AIP(CHN_NUM_AIP + 1)) << 8)));
            reg_write(monitor, MONITOR_RID_VALUE(monitor_chn), ((CHN_AIP(CHN_NUM_AIP)) + ((CHN_AIP(CHN_NUM_AIP + 1)) << 8)));
            reg_write(monitor, MONITOR_BID_VALUE(monitor_chn), ((CHN_AIP(CHN_NUM_AIP)) + ((CHN_AIP(CHN_NUM_AIP + 1)) << 8)));
            break;
        case MONITOR_ISP:
            reg_write(monitor, MONITOR_AWID_VALUE(monitor_chn), ((CHN_ISP(CHN_NUM_ISP)) + ((CHN_ISP(CHN_NUM_ISP + 1)) << 8)));
            reg_write(monitor, MONITOR_ARID_VALUE(monitor_chn), ((CHN_ISP(CHN_NUM_ISP)) + ((CHN_ISP(CHN_NUM_ISP + 1)) << 8)));
            reg_write(monitor, MONITOR_WID_VALUE(monitor_chn), ((CHN_ISP(CHN_NUM_ISP)) + ((CHN_ISP(CHN_NUM_ISP + 1)) << 8)));
            reg_write(monitor, MONITOR_RID_VALUE(monitor_chn), ((CHN_ISP(CHN_NUM_ISP)) + ((CHN_ISP(CHN_NUM_ISP + 1)) << 8)));
            reg_write(monitor, MONITOR_BID_VALUE(monitor_chn), ((CHN_ISP(CHN_NUM_ISP)) + ((CHN_ISP(CHN_NUM_ISP + 1)) << 8)));
            break;
        case MONITOR_I2D:
            reg_write(monitor, MONITOR_AWID_VALUE(monitor_chn), ((CHN_I2D(CHN_NUM_I2D)) + ((CHN_I2D(CHN_NUM_I2D + 1)) << 8)));
            reg_write(monitor, MONITOR_ARID_VALUE(monitor_chn), ((CHN_I2D(CHN_NUM_I2D)) + ((CHN_I2D(CHN_NUM_I2D + 1)) << 8)));
            reg_write(monitor, MONITOR_WID_VALUE(monitor_chn), ((CHN_I2D(CHN_NUM_I2D)) + ((CHN_I2D(CHN_NUM_I2D + 1)) << 8)));
            reg_write(monitor, MONITOR_RID_VALUE(monitor_chn), ((CHN_I2D(CHN_NUM_I2D)) + ((CHN_I2D(CHN_NUM_I2D + 1)) << 8)));
            reg_write(monitor, MONITOR_BID_VALUE(monitor_chn), ((CHN_I2D(CHN_NUM_I2D)) + ((CHN_I2D(CHN_NUM_I2D + 1)) << 8)));
            break;
        case MONITOR_EL:
            reg_write(monitor, MONITOR_AWID_VALUE(monitor_chn), ((CHN_EL(CHN_NUM_EL)) + ((CHN_EL(CHN_NUM_EL + 1)) << 8)));
            reg_write(monitor, MONITOR_ARID_VALUE(monitor_chn), ((CHN_EL(CHN_NUM_EL)) + ((CHN_EL(CHN_NUM_EL + 1)) << 8)));
            reg_write(monitor, MONITOR_WID_VALUE(monitor_chn), ((CHN_EL(CHN_NUM_EL)) + ((CHN_EL(CHN_NUM_EL + 1)) << 8)));
            reg_write(monitor, MONITOR_RID_VALUE(monitor_chn), ((CHN_EL(CHN_NUM_EL)) + ((CHN_EL(CHN_NUM_EL + 1)) << 8)));
            reg_write(monitor, MONITOR_BID_VALUE(monitor_chn), ((CHN_EL(CHN_NUM_EL)) + ((CHN_EL(CHN_NUM_EL + 1)) << 8)));
            break;
        case MONITOR_CPU:
            reg_write(monitor, MONITOR_AWID_VALUE(monitor_chn), ((CHN_CPU(CHN_NUM_CPU)) + ((CHN_CPU(CHN_NUM_CPU + 1)) << 8)));
            reg_write(monitor, MONITOR_ARID_VALUE(monitor_chn), ((CHN_CPU(CHN_NUM_CPU)) + ((CHN_CPU(CHN_NUM_CPU + 1)) << 8)));
            reg_write(monitor, MONITOR_WID_VALUE(monitor_chn), ((CHN_CPU(CHN_NUM_CPU)) + ((CHN_CPU(CHN_NUM_CPU + 1)) << 8)));
            reg_write(monitor, MONITOR_RID_VALUE(monitor_chn), ((CHN_CPU(CHN_NUM_CPU)) + ((CHN_CPU(CHN_NUM_CPU + 1)) << 8)));
            reg_write(monitor, MONITOR_BID_VALUE(monitor_chn), ((CHN_CPU(CHN_NUM_CPU)) + ((CHN_CPU(CHN_NUM_CPU + 1)) << 8)));
            break;                                              
        default:
            printk("NOT SUPPORT THIS ID !!!\n");
            break;
    }

    return 0;
}

static int set_valm_fun(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    struct monitor_param *ip = monitor_param;
    MONITOR_DEBUG("[%s,%d] chn =  %d\n",__func__,__LINE__,monitor_chn);

    set_id_switch_fun(monitor, ip);
    reg_write(monitor, MONITOR_CHKVALM_FUN_EN(monitor_chn), 0x11111);
    reg_write(monitor, MONITOR_CHKVALM_INT_EN(monitor_chn), 0x11111);
    return 0;
}


static int set_vals_fun(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    struct monitor_param *ip = monitor_param;
    MONITOR_DEBUG("[%s,%d] chn =  %d\n",__func__,__LINE__,monitor_chn);

    if(monitor_chn != 5 && monitor_chn != 7){
        set_id_switch_fun(monitor, ip);
        switch(ip->monitor_fun_mode){
            case 0:
                reg_write(monitor, MONITOR_CHKVALS_FUN_MODE(monitor_chn), 0x1041041); //user contor
                reg_write(monitor, MONITOR_CHKVALS_INT_EN(monitor_chn), 0x1041041);  // 1
                break;
            case 1:
                reg_write(monitor, MONITOR_CHKVALS_FUN_MODE(monitor_chn), 0x1041041); //user contor
                reg_write(monitor, MONITOR_CHKVALS_INT_EN(monitor_chn), 0x30c30c3); // 1/8
                break;
            case 2:
                reg_write(monitor, MONITOR_CHKVALS_FUN_MODE(monitor_chn), 0x0); //max time ->0xffffffff
                reg_write(monitor, MONITOR_CHKVALS_INT_EN(monitor_chn), 0x30c30c3); // 1/8
                reg_write(monitor, MONITOR_CHKVALS_FUN_EN(monitor_chn), 0x1041041);
                break;
            case 3:
                reg_write(monitor, MONITOR_CHKVALS_FUN_MODE(monitor_chn), 0x2082082); //
                reg_write(monitor, MONITOR_CHKVALS_INT_EN(monitor_chn), 0x30c30c3); // 1/8
                reg_write(monitor, MONITOR_CHKVALS_FUN_EN(monitor_chn), 0x1041041);
                break;
            case 4:
                reg_write(monitor, MONITOR_CHKVALS_FUN_MODE(monitor_chn), 0x1e79e79e); //mxa time 0x8888000
                reg_write(monitor, MONITOR_CHKVALS_INT_EN(monitor_chn), 0x30c30c3); // 1/8
                reg_write(monitor, MONITOR_CHKVALS_FUN_EN(monitor_chn), 0x1041041);
                break;
            default:
                printk("\not support fun mode !!!!");
        }
    }else{
        switch(ip->monitor_fun_mode){
            case 0:
                reg_write(monitor, MONITOR_CHKVALS_FUN_MODE(0), 0x1041041); //get time
                reg_write(monitor, MONITOR_CHKVALS_FUN_MODE(monitor_chn), 0x990000); //user contor
                reg_write(monitor, MONITOR_CHKVALS_INT_EN(monitor_chn), 0x11);  // 1
                break;
            case 1:
                reg_write(monitor, MONITOR_CHKVALS_FUN_MODE(monitor_chn), 0x990000); //user contor this is pdma setting
                reg_write(monitor, MONITOR_CHKVALS_INT_EN(monitor_chn), 0x11);  // bit0是写中断en，bit4是读中断en
                reg_write(monitor, MONITOR_CHKVALS_FUN_EN(monitor_chn), 0x11);  // bit0是写中断en，bit4是读中断en
                break;
            default:
                printk("\not support fun mode !!!!");
        }
    }
    return 0;
}

static int set_chk_id_addr_fun(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    struct monitor_param *ip = monitor_param;
    MONITOR_DEBUG("[%s,%d] chn =  %d\n",__func__,__LINE__,monitor_chn);

    reg_write(monitor, MONITOR_CHKADDR_FUN_MODE(monitor_chn), 0x0);

    set_id_switch_fun(monitor,ip);
    reg_write(monitor, MONITOR_AWADDR_VALUE0(monitor_chn), ip->monitor_start);
    reg_write(monitor, MONITOR_AWADDR_VALUE1(monitor_chn), ip->monitor_end);
    reg_write(monitor, MONITOR_ARADDR_VALUE0(monitor_chn), ip->monitor_start);
    reg_write(monitor, MONITOR_ARADDR_VALUE1(monitor_chn), ip->monitor_end);

    //reg_write(monitor, MONITOR_CHKADDR_INT_EN(monitor_chn), 0x7777);
    reg_write(monitor, MONITOR_CHKADDR_FUN_EN(monitor_chn), 0x1111);


    return 0;
}

static int set_chk_addr_fun(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    
    struct monitor_param *ip = monitor_param;
    MONITOR_DEBUG("[%s,%d] chn =  %d\n",__func__,__LINE__,monitor_chn);

    reg_write(monitor, MONITOR_CHKADDR_FUN_MODE(monitor_chn), 0x0);

    reg_write(monitor, MONITOR_AWADDR_VALUE0(monitor_chn), ip->monitor_start);
    reg_write(monitor, MONITOR_AWADDR_VALUE1(monitor_chn), ip->monitor_end);
    reg_write(monitor, MONITOR_ARADDR_VALUE0(monitor_chn), ip->monitor_start);
    reg_write(monitor, MONITOR_ARADDR_VALUE1(monitor_chn), ip->monitor_end);

    //reg_write(monitor, MONITOR_CHKADDR_INT_EN(monitor_chn), 0x7777);
    reg_write(monitor, MONITOR_CHKADDR_FUN_EN(monitor_chn), 0x1111);

    return 0;
}

static int set_chk_id_fun(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
    struct monitor_param *ip = monitor_param;
    MONITOR_DEBUG("[%s,%d] chn =  %d\n",__func__,__LINE__,monitor_chn);

    reg_write(monitor, MONITOR_CHKID_FUN_MODE(monitor_chn), 0x0);

    set_id_switch_fun(monitor,ip);
    reg_write(monitor, MONITOR_CHKID_INT_EN(monitor_chn), 0x77777);
    reg_write(monitor, MONITOR_CHKID_FUN_EN(monitor_chn), 0x11111);
    return 0;
}

static int monitor_start(struct jz_monitor *monitor, struct monitor_param *monitor_param)
{
	int ret = 0;
	struct monitor_param *ip = monitor_param;

	if ((monitor == NULL) || (monitor_param == NULL)) {
		dev_err(monitor->dev, "monitor: monitor is NULL or monitor_param is NULL\n");
		return -1;
	}
	MONITOR_DEBUG("monitor: enter monitor_start %d\n", current->pid);
    monitor_chn = ip->monitor_chn;
    if(monitor_chn  > 7){
        printk("chn set err : %d !!!");
        return -1;
    }
    memcpy(&monitor->iparam, ip, sizeof(struct monitor_param));

#ifndef CONFIG_FPGA_TEST
	clk_enable(monitor->clk);
#ifdef CONFIG_SOC_T40
	clk_enable(monitor->ahb0_gate);
#endif
#endif
   
    switch(ip->monitor_mode){
        case MONITOR_CHK_ID:
           set_chk_id_fun(monitor, ip); 
            break;
        case MONITOR_CHK_ADDR:
           set_chk_addr_fun(monitor, ip); 
            break;
        case MONITOR_ID_ADDR:
           set_chk_id_addr_fun(monitor, ip); 
            break;
        case MONITOR_VALS:
           set_vals_fun(monitor, ip); 
            break;
        case MONITOR_VALM:
           set_valm_fun(monitor, ip); 
            break;
        default:
            MONITOR_DEBUG("not support this mode !!!");
            break;
    }

    MONITOR_DEBUG("monitor: exit monitor_start %d\n", current->pid);

#ifndef CONFIG_FPGA_TEST
#ifdef CONFIG_SOC_T40
	clk_disable(monitor->ahb0_gate);
#endif
#endif
    return 0;

err_monitor_wait_for_done:
#ifndef CONFIG_FPGA_TEST
#ifdef CONFIG_SOC_T40
	clk_disable(monitor->ahb0_gate);
#endif
#endif
	return ret;

}

static long monitor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct monitor_param iparam;
	struct miscdevice *dev = filp->private_data;
	struct jz_monitor *monitor = container_of(dev, struct jz_monitor, misc_dev);

	MONITOR_DEBUG("monitor: %s pid: %d, tgid: %d file: %p, cmd: 0x%08x\n",
			__func__, current->pid, current->tgid, filp, cmd);

	if (_IOC_TYPE(cmd) != JZMONITOR_IOC_MAGIC) {
		dev_err(monitor->dev, "invalid cmd!\n");
		return -EFAULT;
	}

	mutex_lock(&monitor->mutex);

	switch (cmd) {
		case IOCTL_MONITOR_START:
			if (copy_from_user(&iparam, (void *)arg, sizeof(struct monitor_param))) {
				dev_err(monitor->dev, "copy_from_user error!!!\n");
				ret = -EFAULT;
				break;
			}
			ret = monitor_start(monitor, &iparam);
			if (ret) {
				printk("monitor: error monitor start ret = %d\n", ret);
			}
			break;
		case IOCTL_MONITOR_DUMP_REG:
            /*
			ret = wait_for_completion_interruptible_timeout(&monitor->done_buf, msecs_to_jiffies(2000));
			if (ret < 0) {
				printk("monitor: done_buf wait_for_completion_interruptible_timeout err %d\n", ret);
			} else if (ret == 0 ) {
				printk("monitor: done_buf wait_for_completion_interruptible_timeout timeout %d\n", ret);
				ret = -1;
				dump_chx_all_reg_value(monitor);
			} else {
				ret = 0;
			}
            */
			if (copy_from_user(&iparam, (void *)arg, sizeof(struct monitor_param))) {
				dev_err(monitor->dev, "copy_from_user error!!!\n");
				ret = -EFAULT;
				break;
			}
			dump_chx_all_reg_value(monitor, &iparam);
			break;
		case IOCTL_MONITOR_BUF_UNLOCK:
			complete(&monitor->done_buf);
			break;
	    default:
			dev_err(monitor->dev, "invalid command: 0x%08x\n", cmd);
			ret = -EINVAL;
	}

	mutex_unlock(&monitor->mutex);
	return ret;
}

static int monitor_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct miscdevice *dev = filp->private_data;
	struct jz_monitor *monitor = container_of(dev, struct jz_monitor, misc_dev);

	MONITOR_DEBUG("monitor: %s pid: %d, tgid: %d filp: %p\n",
			__func__, current->pid, current->tgid, filp);
	mutex_lock(&monitor->mutex);

	mutex_unlock(&monitor->mutex);
	return ret;
}

static int monitor_release(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct miscdevice *dev = filp->private_data;
	struct jz_monitor *monitor = container_of(dev, struct jz_monitor, misc_dev);

	MONITOR_DEBUG("monitor: %s  pid: %d, tgid: %d filp: %p\n",
			__func__, current->pid, current->tgid, filp);
	mutex_lock(&monitor->mutex);

	mutex_unlock(&monitor->mutex);
	return ret;
}

static struct file_operations monitor_ops = {
	.owner = THIS_MODULE,
	.open = monitor_open,
	.release = monitor_release,
	.unlocked_ioctl = monitor_ioctl,
};


static irqreturn_t monitor_irq_handler(int irq, void *data)
{
	struct jz_monitor *monitor;
	unsigned int status;

	monitor = (struct jz_monitor *)data;

	complete(&monitor->done_buf);

    if(!irq_num)
	    MONITOR_DEBUG("monitor: %s, fun = %d\n", __func__,monitor->iparam.monitor_mode);

    irq_num++;
    switch(monitor->iparam.monitor_mode){
        case MONITOR_CHK_ID:
            status  = reg_read(monitor, MONITOR_CHKID_INT_STATE(monitor_chn));
            reg_write(monitor, MONITOR_CHKID_INT_CLR(monitor_chn), status);
            break;
        case MONITOR_CHK_ADDR:
        case MONITOR_ID_ADDR:
            status  = reg_read(monitor, MONITOR_CHKADDR_INT_STATE(monitor_chn));
            reg_write(monitor, MONITOR_CHKADDR_INT_CLR(monitor_chn), status);
            break;
        case MONITOR_VALS:
            status  = reg_read(monitor, MONITOR_CHKVALS_INT_STATE(monitor_chn));
            reg_write(monitor, MONITOR_CHKVALS_INT_CLR(monitor_chn), status);
            break;
        case MONITOR_VALM:
            status  = reg_read(monitor, MONITOR_CHKVALM_INT_STATE(monitor_chn));
            reg_write(monitor, MONITOR_CHKVALM_INT_CLR(monitor_chn), status);
            break;
        default:
            MONITOR_DEBUG("not support this mode !!!");
            break;
    }
    return IRQ_HANDLED;
}

static int monitor_probe(struct platform_device *pdev)
{
    int ret = 0;
    unsigned int reg_addr = 0;
    struct jz_monitor *monitor;

	MONITOR_DEBUG("%s\n", __func__);
	monitor = (struct jz_monitor *)kzalloc(sizeof(struct jz_monitor), GFP_KERNEL);
	if (!monitor) {
		dev_err(&pdev->dev, "alloc jz_monitor failed!\n");
		return -ENOMEM;
	}

	sprintf(monitor->name, "monitor");

	monitor->misc_dev.minor = MISC_DYNAMIC_MINOR;
	monitor->misc_dev.name = monitor->name;
	monitor->misc_dev.fops = &monitor_ops;
	monitor->dev = &pdev->dev;

	mutex_init(&monitor->mutex);
	init_completion(&monitor->done_monitor);
	init_completion(&monitor->done_buf);
	//complete(&monitor->done_buf);

	monitor->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!monitor->res) {
		dev_err(&pdev->dev, "failed to get dev resources: %d\n", ret);
		ret = -EINVAL;
		goto err_get_res;
	}

    MONITOR_DEBUG("### monitor->res->start = 0x%08x  res->end = 0x%08x\n", monitor->res->start, monitor->res->end);
	monitor->res = request_mem_region(monitor->res->start,
			monitor->res->end - monitor->res->start + 1,
			pdev->name);
	if (!monitor->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto err_get_res;
	}
	monitor->iomem = ioremap(monitor->res->start, resource_size(monitor->res));
	if (!monitor->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region: %d\n",ret);
		ret = -EINVAL;
		goto err_ioremap;
	}

	monitor->irq = platform_get_irq(pdev, 0);
    MONITOR_DEBUG("## monitor->irq = %d\n", monitor->irq);
	if (devm_request_irq(&pdev->dev, monitor->irq, monitor_irq_handler, IRQF_SHARED, monitor->name, monitor)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto err_req_irq;
	}

#if 0
	monitor->clk = clk_get(monitor->dev, "gate_monitor");
	if (IS_ERR(monitor->clk)) {
		dev_err(&pdev->dev, "monitor clk get failed!\n");
		goto err_get_clk;
	}

	monitor->ahb0_gate = clk_get(monitor->dev, "gate_ahb0");
	if (IS_ERR(monitor->clk)) {
		dev_err(&pdev->dev, "monitor clk get failed!\n");
		goto err_get_clk;
	}
#endif
	monitor->clk = devm_clk_get(monitor->dev, "gate_monitor");
	if (IS_ERR(monitor->clk)) {
		dev_err(&pdev->dev, "monitor clk get failed!\n");
		goto err_get_clk;
	}

	monitor->ahb0_gate = devm_clk_get(monitor->dev, "gate_ahb0");
	if (IS_ERR(monitor->clk)) {
		dev_err(&pdev->dev, "monitor clk get failed!\n");
		goto err_get_clk;
	}


    if ((clk_prepare_enable(monitor->clk)) < 0) {
		dev_err(&pdev->dev, "Enable monitor gate clk failed\n");
		goto err_get_clk;
	}
	if ((clk_prepare_enable(monitor->ahb0_gate)) < 0) {
		dev_err(&pdev->dev, "Enable monitor ahb0 clk failed\n");
		goto err_get_clk;
	}

	dev_set_drvdata(&pdev->dev, monitor);


    ret = misc_register(&monitor->misc_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "register misc device failed!\n");
		goto err_set_drvdata;
	}

    printk("JZ MONITOR probe ok version is %s!!!!\n",MONITOR_VERSION);
	return 0;

err_set_drvdata:
#ifndef CONFIG_FPGA_TEST
	clk_put(monitor->clk);
err_get_clk:
#endif
	free_irq(monitor->irq, monitor);
err_req_irq:
	iounmap(monitor->iomem);
err_ioremap:
err_get_res:
	kfree(monitor);

	return ret;
}

static int monitor_remove(struct platform_device *pdev)
{
	struct jz_monitor *monitor;
	struct resource *res;
	MONITOR_DEBUG("%s\n", __func__);

	monitor = dev_get_drvdata(&pdev->dev);
	res = monitor->res;
	free_irq(monitor->irq, monitor);
	iounmap(monitor->iomem);
	release_mem_region(res->start, res->end - res->start + 1);

	misc_deregister(&monitor->misc_dev);

    if (monitor) {
		kfree(monitor);
	}

	return 0;
}

static struct platform_driver jz_monitor_driver = {
	.probe	= monitor_probe,
	.remove = monitor_remove,
	.driver = {
		.name = "jz-monitor",
	},
};

static struct resource jz_monitor_resources[] = {
    [0] = {
	.start  = MONITOR_IOBASE,
	.end    = MONITOR_IOBASE + 0x8000-1,
	.flags  = IORESOURCE_MEM,
    },
    [1] = {
        .start  = IRQ_MON + 8,
        .end    = IRQ_MON + 8,
        .flags  = IORESOURCE_IRQ,
    },
};

struct platform_device jz_monitor_device = {
    .name   = "jz-monitor",
    .id = 0,
    .resource   = jz_monitor_resources,
    .num_resources  = ARRAY_SIZE(jz_monitor_resources),
};

static int __init monitordev_init(void)
{
    int ret = 0;

    MONITOR_DEBUG("%s\n", __func__);

    ret = platform_device_register(&jz_monitor_device);
    if(ret){
	    printk("Failed to insmod des device!!!\n");
	    return ret;
    }
	platform_driver_register(&jz_monitor_driver);
	return 0;
}

static void __exit monitordev_exit(void)
{
	MONITOR_DEBUG("%s\n", __func__);
	platform_device_unregister(&jz_monitor_device);
	platform_driver_unregister(&jz_monitor_driver);
}

module_init(monitordev_init);
module_exit(monitordev_exit);

MODULE_DESCRIPTION("JZ BUS_MONITOR driver");
MODULE_AUTHOR("jiansheng.zhang <jiansheng.zhang@ingenic.cn>");
MODULE_LICENSE("GPL");
