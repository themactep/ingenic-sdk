#include <linux/delay.h>
#include "../tx-isp-videobuf.h"
#include "tx-isp-vic.h"

void dump_vic_reg(struct tx_isp_vic_device *vsd)
{
	printk("BT_DVP_CFG : %08x\n", tx_isp_vic_readl(vsd,VIC_DB_CFG));
#if VIC_SUPPORT_MIPI
	printk("INTERFACE_TYPE : %08x\n", tx_isp_vic_readl(vsd,VIC_INTF_TYPE));
	printk("IDI_TYPE_TYPE : %08x\n", tx_isp_vic_readl(vsd,VIC_IDI_TYPE));
#endif
	printk("RESOLUTION : %08x\n", tx_isp_vic_readl(vsd,VIC_RESOLUTION));
	printk("AB_VALUE : %08x\n", tx_isp_vic_readl(vsd,VIC_AB_VALUE));
	printk("GLOBAL_CONFIGURE : %08x\n", tx_isp_vic_readl(vsd,VIC_GLOBAL_CFG));
	printk("CONTROL : %08x\n", tx_isp_vic_readl(vsd,VIC_CONTROL));
	printk("PIXEL : 0x%08x\n", tx_isp_vic_readl(vsd,VIC_PIXEL));
	printk("LINE : 0x%08x\n", tx_isp_vic_readl(vsd,VIC_LINE));
	printk("VIC_STATE : 0x%08x\n", tx_isp_vic_readl(vsd,VIC_STATE));
	printk("OFIFO_COUNT : 0x%08x\n", tx_isp_vic_readl(vsd,VIC_OFIFO_COUNT));
	printk("0x3c :0x%08x\n",tx_isp_vic_readl(vsd,0x3c));
	printk("0x40 :0x%08x\n",tx_isp_vic_readl(vsd,0x40));
	printk("0x44 :0x%08x\n",tx_isp_vic_readl(vsd,0x44));
	printk("0x54 :0x%08x\n",tx_isp_vic_readl(vsd,0x54));
	printk("0x58 :0x%08x\n",tx_isp_vic_readl(vsd,0x58));
	printk("0x58 :0x%08x\n",tx_isp_vic_readl(vsd,0x5c));
	printk("0x60 :0x%08x\n",tx_isp_vic_readl(vsd,0x60));
	printk("0x64 :0x%08x\n",tx_isp_vic_readl(vsd,0x64));
	printk("0x68 :0x%08x\n",tx_isp_vic_readl(vsd,0x68));
	printk("0x6c :0x%08x\n",tx_isp_vic_readl(vsd,0x6c));
	printk("0x70 :0x%08x\n",tx_isp_vic_readl(vsd,0x70));
	printk("0x74 :0x%08x\n",tx_isp_vic_readl(vsd,0x74));
	printk("0x04 :0x%08x\n",tx_isp_vic_readl(vsd,0x04));
	printk("0x08 :0x%08x\n",tx_isp_vic_readl(vsd,0x08));
	printk("VIC_EIGHTH_CB :0x%08x\n",tx_isp_vic_readl(vsd,VIC_EIGHTH_CB));
	printk("CB_MODE0 :0x%08x\n",tx_isp_vic_readl(vsd,CB_MODE0));
}

static struct tx_isp_vic_device *dump_vsd = NULL;
void check_vic_error(void){
	while(1){
		dump_vic_reg(dump_vsd);
	}
}

#if (defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21))
void isp_lfb_ctrl_flb_enable(int on)
{
	volatile unsigned int value = tx_isp_vic_readl(dump_vsd, LFB_CONTROL);
	value &= ~(LFB_CTRL_ENABLE_MASK);
	value |= LFB_CTRL_ENABLE(on);
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, value);
}

void isp_lfb_reset(void)
{
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, 0);
}

void isp_lfb_ctrl_hw_recovery(int on){
	volatile unsigned int value = tx_isp_vic_readl(dump_vsd, LFB_CONTROL);
	value &= ~(LFB_CTRL_RECORVER_MODE_MASK);
	value |= LFB_CTRL_RECORVER_MODE(on);
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, value);
}

static inline void isp_lfb_ctrl_select(int chan)
{
	volatile unsigned int value = tx_isp_vic_readl(dump_vsd, LFB_CONTROL);
	value &= ~(LFB_CTRL_CHAN_SEL_MASK);
	value |= LFB_CTRL_CHAN_SEL(chan);
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, value);
}

void isp_lfb_ctrl_select_fr(void)
{
	isp_lfb_ctrl_select(LFB_CTRL_CHAN_SEL_FR);
}

void isp_lfb_ctrl_select_ds1(void)
{
	isp_lfb_ctrl_select(LFB_CTRL_CHAN_SEL_DS1);
}

void isp_lfb_ctrl_output_ncu(void){
	volatile unsigned int value = tx_isp_vic_readl(dump_vsd, LFB_CONTROL);
	value &= ~(LFB_CTRL_OUT_SEL_MASK);
	value |= LFB_CTRL_OUT_SEL(LBF_CTRL_OUT_SEL_NCU);
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, value);
}

void isp_lfb_ctrl_output_mscaler(void){
	volatile unsigned int value = tx_isp_vic_readl(dump_vsd, LFB_CONTROL);
	value &= ~(LFB_CTRL_OUT_SEL_MASK);
	value |= LFB_CTRL_OUT_SEL(LBF_CTRL_OUT_SEL_MS);
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, value);
}

void isp_lfb_ctrl_vsync_wait(int on){
	volatile unsigned int value = tx_isp_vic_readl(dump_vsd, LFB_CONTROL);
	value &= ~(LFB_CTRL_VSYNC_COND_MASK);
	value |= LFB_CTRL_VSYNC_COND(on);
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, value);
}

void isp_lfb_ctrl_ncu_to_mscaler(void){
	volatile unsigned int value = tx_isp_vic_readl(dump_vsd, LFB_CONTROL);
	value &= ~(LFB_CTRL_DISABLE_MSC_MASK);
	value |= LFB_CTRL_DISABLE_MSC(LBF_CTRL_NCU_TO_MSC);
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, value);
}

void isp_lfb_ctrl_ncu_to_ddr(void){
	volatile unsigned int value = tx_isp_vic_readl(dump_vsd, LFB_CONTROL);
	value &= ~(LFB_CTRL_DISABLE_MSC_MASK);
	value |= LFB_CTRL_DISABLE_MSC(LBF_CTRL_NCU_TO_DDR);
	tx_isp_vic_writel(dump_vsd, LFB_CONTROL, value);
}

void isp_lfb_restart(void){
	tx_isp_vic_writel(dump_vsd, LFB_RESTART, 0);
	tx_isp_vic_writel(dump_vsd, LFB_RESTART, 1);
}

void isp_lfb_config_default_dma(unsigned int base, unsigned int uv_offset, int lineoffset, int bank_id)
{
	/* set buffer counter */
	volatile unsigned int value = LFB_Y_CFG_BUF_NUM(1) | LFB_Y_CFG_STRIDE(lineoffset);
	tx_isp_vic_writel(dump_vsd, LFB_Y_CFG, value);
	tx_isp_vic_writel(dump_vsd, LFB_Y_BUF0+bank_id*4, base);

	if(uv_offset){
		value = LFB_UV_CFG_BUF_NUM(1) | LFB_UV_CFG_STRIDE(lineoffset);
		tx_isp_vic_writel(dump_vsd, LFB_UV_CFG, value);
		tx_isp_vic_writel(dump_vsd, LFB_UV_BUF0+bank_id*4, base+uv_offset);
	}
}

void isp_lfb_config_resolution(unsigned int width, unsigned int height)
{
	/* set buffer counter */
	volatile unsigned int value = LFB_Y_CFG_WIDTH(width) | LFB_Y_CFG_HEIGHT(height);
	tx_isp_vic_writel(dump_vsd, LFB_Y_RES, value);

	value = LFB_UV_CFG_WIDTH(width>>1) | LFB_UV_CFG_HEIGHT(height>>1);
	tx_isp_vic_writel(dump_vsd, LFB_UV_RES, value);
}

volatile unsigned int isp_lfb_read_error_reg(void)
{
	return tx_isp_vic_readl(dump_vsd, LFB_ERR_REG);
}
#else

void isp_lfb_reset(void){}
void isp_lfb_ctrl_flb_enable(int on){}
void isp_lfb_ctrl_hw_recovery(int on){}
void isp_lfb_ctrl_select_fr(void){}
void isp_lfb_ctrl_select_ds1(void){}
void isp_lfb_ctrl_output_ncu(void){}
void isp_lfb_ctrl_output_mscaler(void){}
void isp_lfb_ctrl_vsync_wait(int on){}
void isp_lfb_ctrl_ncu_to_mscaler(void){}
void isp_lfb_ctrl_ncu_to_ddr(void){}
void isp_lfb_restart(void){}
void isp_lfb_config_default_dma(unsigned int base, unsigned int uv_offset, int lineoffset, int bank_id){}
void isp_lfb_config_resolution(unsigned int width, unsigned int height){}
volatile unsigned int isp_lfb_read_error_reg(void){return 0}
#endif

/* interrupt operations */
void tx_vic_enable_irq(int enable)
{
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(dump_vsd) ? NULL : &(dump_vsd->sd);
	struct tx_isp_irq_device *irq_dev = NULL;
	volatile unsigned int reg = 0;
	unsigned long flags = 0;

	if(IS_ERR_OR_NULL(sd))
		return;

	irq_dev = &sd->irqdev;
	private_spin_lock_irqsave(&dump_vsd->slock, flags);
	/*private_mutex_lock(&dump_vsd->mlock);*/
	reg = tx_isp_sd_readl(sd, TX_ISP_TOP_IRQ_ENABLE);
	reg |= enable;
	tx_isp_sd_writel(sd, TX_ISP_TOP_IRQ_ENABLE, reg);
	/*printk("@@@@ irq enable = 0x%08x 0x%08x\n", reg, enable);*/
	if(dump_vsd->irq_state == 0){
		dump_vsd->irq_state = 1;
		if(irq_dev->enable_irq)
			irq_dev->enable_irq(irq_dev);
	}
	/*private_mutex_unlock(&dump_vsd->mlock);*/
	private_spin_unlock_irqrestore(&dump_vsd->slock, flags);
}

void tx_vic_disable_irq(int enable)
{
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(dump_vsd) ? NULL : &(dump_vsd->sd);
	struct tx_isp_irq_device *irq_dev = NULL;
	volatile unsigned int reg = 0;
	unsigned long flags = 0;

	if(IS_ERR_OR_NULL(sd))
		return;
	irq_dev = &sd->irqdev;
	private_spin_lock_irqsave(&dump_vsd->slock, flags);
	/*private_mutex_lock(&dump_vsd->mlock);*/
	reg = tx_isp_sd_readl(sd, TX_ISP_TOP_IRQ_ENABLE);
	reg &= ~enable;
	tx_isp_sd_writel(sd, TX_ISP_TOP_IRQ_ENABLE, reg);
	if(reg == 0 && dump_vsd->irq_state){
		dump_vsd->irq_state = 0;
		if(irq_dev->disable_irq)
			irq_dev->disable_irq(irq_dev);
	}
	/*private_mutex_unlock(&dump_vsd->mlock);*/
	private_spin_unlock_irqrestore(&dump_vsd->slock, flags);
}

static int tx_isp_vic_start(struct tx_isp_vic_device *vsd)
{
	struct tx_isp_video_in *vin = &vsd->vin;
	int ret = ISP_SUCCESS;
	unsigned int value = 0;
	unsigned int input_cfg = 0;
	unsigned int input_res = 0;

	/*printk("## %s %d vin = %p##\n", __func__,__LINE__,vin);*/
	if(vin->attr->dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI){
#if VIC_SUPPORT_MIPI
		tx_isp_vic_writel(vsd,VIC_INTF_TYPE, INTF_TYPE_MIPI);//MIPI
		switch(vin->mbus.code){
			case V4L2_MBUS_FMT_SBGGR8_1X8:
			case V4L2_MBUS_FMT_SGBRG8_1X8:
			case V4L2_MBUS_FMT_SGRBG8_1X8:
			case V4L2_MBUS_FMT_SRGGB8_1X8:
				tx_isp_vic_writel(vsd,VIC_IDI_TYPE,MIPI_RAW8);//RAW8
				break;
			case 	V4L2_MBUS_FMT_SBGGR10_1X10:
			case	V4L2_MBUS_FMT_SGBRG10_1X10:
			case	V4L2_MBUS_FMT_SGRBG10_1X10:
			case	V4L2_MBUS_FMT_SRGGB10_1X10:
				tx_isp_vic_writel(vsd,VIC_IDI_TYPE,MIPI_RAW10);//RAW10
				break;
			case	V4L2_MBUS_FMT_SBGGR12_1X12:
			case	V4L2_MBUS_FMT_SGBRG12_1X12:
			case	V4L2_MBUS_FMT_SGRBG12_1X12:
			case	V4L2_MBUS_FMT_SRGGB12_1X12:
				tx_isp_vic_writel(vsd,VIC_IDI_TYPE,MIPI_RAW12);//RAW12
				break;
			case V4L2_MBUS_FMT_RGB555_2X8_PADHI_LE:
				tx_isp_vic_writel(vsd,VIC_IDI_TYPE,MIPI_RGB555);//RGB555
				break;
			case V4L2_MBUS_FMT_RGB565_2X8_LE:
				tx_isp_vic_writel(vsd,VIC_IDI_TYPE,MIPI_RGB565);//RGB565
				break;
			case 	V4L2_MBUS_FMT_UYVY8_1_5X8 :
			case	V4L2_MBUS_FMT_VYUY8_1_5X8 :
			case	V4L2_MBUS_FMT_YUYV8_1_5X8 :
			case	V4L2_MBUS_FMT_YVYU8_1_5X8 :
			case	V4L2_MBUS_FMT_YUYV8_1X16 :
				tx_isp_vic_writel(vsd,VIC_IDI_TYPE,MIPI_YUV422);//YUV422
				break;

		}
		input_res = (vin->mbus.width<< 16) | (vin->mbus.height);
		tx_isp_vic_writel(vsd,VIC_RESOLUTION, input_res);
#if defined(CONFIG_SOC_T10) || defined(CONFIG_SOC_T20)
		tx_isp_vic_writel(vsd,VIC_GLOBAL_CFG, ISP_PRESET_MODE1);//ISP_PRESET_MODE1);
#endif
#if (defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21))
		tx_isp_vic_writel(vsd,VIC_FRM_ECC, FRAME_ECC_EN);//ISP_PRESET_MODE1);
#endif
		tx_isp_vic_writel(vsd,VIC_CONTROL, REG_ENABLE);
		while(tx_isp_vic_readl(vsd,VIC_CONTROL));
		tx_isp_vic_writel(vsd, VIC_CONTROL, VIC_SRART);
		//dump_vic_reg(dump_vsd);
#endif
	}else if(vin->attr->dbus_type == TX_SENSOR_DATA_INTERFACE_DVP){
#if VIC_SUPPORT_MIPI
		tx_isp_vic_writel(vsd,VIC_INTF_TYPE, INTF_TYPE_DVP);//DVP
#endif
		switch(vin->mbus.code){
			case V4L2_MBUS_FMT_SBGGR8_1X8:
			case V4L2_MBUS_FMT_SGBRG8_1X8:
			case V4L2_MBUS_FMT_SGRBG8_1X8:
			case V4L2_MBUS_FMT_SRGGB8_1X8:
				if (vin->attr->dvp.gpio == DVP_PA_LOW_8BIT) {
					input_cfg = DVP_RAW8; //RAW8 low_align
				} else if (vin->attr->dvp.gpio == DVP_PA_HIGH_8BIT) {
					input_cfg = DVP_RAW8|DVP_RAW_ALIG;//RAW8 high_align
				}else{
					printk("%s[%d] VIC failed to config DVP mode!(8bits-sensor)\n",__func__,__LINE__);
					ret = -1;
				}
				break;
			case V4L2_MBUS_FMT_SBGGR10_1X10:
			case V4L2_MBUS_FMT_SGBRG10_1X10:
			case V4L2_MBUS_FMT_SGRBG10_1X10:
			case V4L2_MBUS_FMT_SRGGB10_1X10:
				if(vin->attr->dvp.mode == SENSOR_DVP_SONY_MODE){
				    if (vin->attr->dvp.gpio == DVP_PA_LOW_10BIT) {
						input_cfg = DVP_SONY_MODE|DVP_RAW10;
					} else if (vin->attr->dvp.gpio == DVP_PA_HIGH_10BIT) {
						input_cfg = DVP_SONY_MODE|DVP_RAW10|DVP_RAW_ALIG;
					}else{
						printk("%s[%d] VIC failed to config DVP SONY mode!(10bits-sensor)\n",__func__,__LINE__);
						ret = -1;
                                        }
				}else{
					if (vin->attr->dvp.gpio == DVP_PA_LOW_10BIT) {
						input_cfg = DVP_RAW10; //RAW10 low_align
					} else if (vin->attr->dvp.gpio == DVP_PA_HIGH_10BIT) {
						input_cfg = DVP_RAW10|DVP_RAW_ALIG;//RAW10 high_align
					}else{
						printk("%s[%d] VIC failed to config DVP mode!(10bits-sensor)\n",__func__,__LINE__);
						ret = -1;
					}
				}
				break;
			case V4L2_MBUS_FMT_SBGGR12_1X12:
			case V4L2_MBUS_FMT_SGBRG12_1X12:
			case V4L2_MBUS_FMT_SGRBG12_1X12:
			case V4L2_MBUS_FMT_SRGGB12_1X12:
				if(vin->attr->dvp.mode == SENSOR_DVP_SONY_MODE){
					input_cfg = DVP_RAW12|DVP_SONY_MODE;
				}else{
					input_cfg = DVP_RAW12;
				}
				break;
			case V4L2_MBUS_FMT_RGB565_2X8_LE:
				input_cfg = DVP_RGB565_16BIT;
				break;
			case V4L2_MBUS_FMT_BGR565_2X8_LE:
				input_cfg = DVP_BRG565_16BIT;
				break;
			case V4L2_MBUS_FMT_UYVY8_1_5X8 :
			case V4L2_MBUS_FMT_VYUY8_1_5X8 :
			case V4L2_MBUS_FMT_YUYV8_1_5X8 :
			case V4L2_MBUS_FMT_YVYU8_1_5X8 :
			case V4L2_MBUS_FMT_YUYV8_1X16 :
				input_cfg = DVP_YUV422_8BIT;
				break;

		}

		if(ret){
			goto exit;
		}
		if(vin->attr->dvp.polar.hsync_polar == DVP_POLARITY_LOW){
			input_cfg |= HSYN_POLAR;
		}

		if(vin->attr->dvp.polar.vsync_polar == DVP_POLARITY_LOW){
			input_cfg |= VSYN_POLAR;
		}
		tx_isp_vic_writel(vsd,VIC_DB_CFG, input_cfg);
		if(vin->attr->dvp.blanking.hblanking)
			tx_isp_vic_writel(vsd, VIC_INPUT_HSYNC_BLANKING, vin->mbus.width + (vin->attr->dvp.blanking.hblanking << 16));
		if(vin->attr->dvp.blanking.vblanking)
			tx_isp_vic_writel(vsd, VIC_INPUT_VSYNC_BLANKING, vin->attr->dvp.blanking.vblanking);

#if (defined(CONFIG_SOC_T30) || defined(CONFIG_SOC_T21))
		tx_isp_vic_writel(vsd, VIC_INPUT_HSYNC_BLANKING, vin->mbus.width + (vin->attr->dvp.blanking.hblanking << 16));
		tx_isp_vic_writel(vsd, 0x150, 0x2);
#endif

		value = (vin->mbus.width<< 16) | (vin->mbus.height);
		tx_isp_vic_writel(vsd,VIC_RESOLUTION, value);
		/*printk(" hblanking = %d vblanking = %d \n", vin->attr->dvp.blanking.hblanking, vin->attr->dvp.blanking.vblanking);*/
		/*printk("************RESOLUTION : %08x***************\n", tx_isp_vic_readl(vsd,VIC_RESOLUTION));*/
		tx_isp_vic_writel(vsd,VIC_GLOBAL_CFG, ISP_PRESET_MODE1);
		tx_isp_vic_writel(vsd,VIC_CONTROL, REG_ENABLE);
		tx_isp_vic_writel(vsd, VIC_CONTROL, VIC_SRART);
		/*dump_vic_reg(dump_vsd);*/
	}else{
		tx_isp_vic_writel(vsd,VIC_DB_CFG, DVP_RAW12);//RAW12
#if VIC_SUPPORT_MIPI
		tx_isp_vic_writel(vsd,VIC_INTF_TYPE, INTF_TYPE_DVP);//DVP
#endif
		value = (vin->mbus.width<< 16) | (vin->mbus.height);
		tx_isp_vic_writel(vsd,VIC_RESOLUTION, value);
		tx_isp_vic_writel(vsd,VIC_GLOBAL_CFG, ISP_PRESET_MODE1);
		tx_isp_vic_writel(vsd,CB_MODE0, 0x80008000);
		tx_isp_vic_writel(vsd, VIC_CONTROL, VIC_SRART);
	};
exit:
	return ret;
}

static int vic_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = ISP_SUCCESS;
	struct tx_isp_vic_device *vsd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);

	if(IS_ERR_OR_NULL(vsd))
		return 0;

	switch(cmd){
		case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
			tx_isp_vic_writel(vsd, VIC_CONTROL, VIC_RESET);
			break;
		case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
			ret = tx_isp_vic_start(vsd);
			break;
		default:
			break;
	}
	return ret;
}

static int vic_sensor_ops_sync_sensor_attr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_vic_device *vd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	if(IS_ERR_OR_NULL(vd)){
		ISP_ERROR("The parameter is invalid!\n");
		return -EINVAL;
	}
	if(arg)
		memcpy(&vd->vin, (void *)arg, sizeof(struct tx_isp_video_in));
	else
		memset(&vd->vin, 0, sizeof(struct tx_isp_video_in));
	return 0;
}



static irqreturn_t isp_vic_interrupt_service_routine(struct tx_isp_subdev *sd, u32 status, bool *handled)
{
	struct tx_isp_vic_device *vd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	unsigned int tmp = 0;
	volatile unsigned int state, pending, mask;

	if(IS_ERR_OR_NULL(vd))
		return IRQ_HANDLED;

	mask = tx_isp_sd_readl(sd, TX_ISP_TOP_IRQ_MASK);
	state = tx_isp_sd_readl(sd, TX_ISP_TOP_IRQ_STA);
	pending = state & (~mask);
	tx_isp_sd_writel(sd, TX_ISP_TOP_IRQ_CLR_1, pending);
#ifdef CONFIG_SOC_T10
	if((0x3 << 19) & pending){
		tmp = tx_isp_vic_readl(vd, VIC_CONTROL);
		tmp |= VIC_RESET;
		tx_isp_vic_writel(vd, VIC_CONTROL, tmp);
		printk("## VIC ERROR status = 0x%08x\n", pending);
		tx_isp_vic_writel(vd, VIC_CONTROL, VIC_SRART);
	}
#else
	/*printk("pending=0x%08x, mask = 0x%08x state = 0x%08x\n",pending,mask,state);*/
	if((0x3 << 20) & pending){
		tmp = tx_isp_vic_readl(vd, VIC_CONTROL);
		tmp |= VIC_RESET;
		tx_isp_vic_writel(vd, VIC_CONTROL, tmp);
		printk("## VIC ERROR status = 0x%08x\n", pending);
	}
#endif

	if ((0x1<<24) & pending){
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb330024c,*(volatile unsigned int*)(0xb330024c));
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb3300260,*(volatile unsigned int*)(0xb3300260));
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb3300264,*(volatile unsigned int*)(0xb3300264));
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb3300268,*(volatile unsigned int*)(0xb3300268));
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb330026c,*(volatile unsigned int*)(0xb330026c));
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb3300270,*(volatile unsigned int*)(0xb3300270));
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb3300274,*(volatile unsigned int*)(0xb3300274));
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb3300278,*(volatile unsigned int*)(0xb3300278));
		ISP_INFO("## [0x%08x] = 0x%08x ##\n",0xb330027c,*(volatile unsigned int*)(0xb330027c));
	}

	if((0x1<<26) & pending){
		tx_isp_vic_writel(vd, VIC_DMA_CONFIG, 0);
		private_complete(&vd->snap_comp);
	}
	/*vic frd interrupt */
	if (0x10000 & pending) {
		vd->vic_frd_c++;
		/*printk("## vic %d ##\n", vd->vic_frd_c);*/
	}

	return IRQ_HANDLED;
}

static void vic_interrupts_enable(struct tx_isp_subdev *sd)
{
	/* vic frd interrupt */
	int value = 0x10000 | (0x3 << 20) | (0x1<<24) | (0x1<<26);
	tx_vic_enable_irq(value);
}

static void vic_interrupts_disable(struct tx_isp_subdev *sd)
{
	/* vic frd interrupt */
	int value = 0x10000 | (0x3 << 20) | (0x1<<24) | (0x1<<26);
	tx_vic_disable_irq(value);
}

static void vic_clks_ops(struct tx_isp_subdev *sd, int on)
{
	struct clk **clks = sd->clks;
	int i = 0;

	if(on){
		for(i = 0; i < sd->clk_num; i++)
			private_clk_enable(clks[i]);
	}else{
		for(i = sd->clk_num - 1; i >=0; i--)
			private_clk_disable(clks[i]);
	}
	return;
}

static int vic_core_ops_init(struct tx_isp_subdev *sd, int on)
{
	struct tx_isp_vic_device *vd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("The parameter is invalid!\n");
		return -EINVAL;
	}

	/*printk("## %s %d on=%d##\n", __func__, __LINE__,on);*/
	if(on){
		if(vd->state == TX_ISP_MODULE_INIT)
			return 0;
		/* enable vic interrups */
		vic_interrupts_enable(sd);
		vd->state = TX_ISP_MODULE_INIT;
	}else{
		if(vd->state == TX_ISP_MODULE_DEINIT)
			return 0;
		vic_interrupts_disable(sd);
		vd->state = TX_ISP_MODULE_DEINIT;
	}

	return ISP_SUCCESS;
}

static int vic_core_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = ISP_SUCCESS;

	/* This module operation */
	switch(cmd){
		case TX_ISP_EVENT_SYNC_SENSOR_ATTR:
			ret = tx_isp_subdev_call(sd, sensor, sync_sensor_attr, arg);
			break;
		case TX_ISP_EVENT_SUBDEV_INIT:
			ret = tx_isp_subdev_call(sd, core, init, *(int*)arg);
			break;
		default:
			break;
	}

	ret = ret == -ENOIOCTLCMD ? 0 : ret;

	return ret;
}

static int vic_core_s_stream(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_vic_device *vd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	int ret = 0;

	if(IS_ERR_OR_NULL(vd))
		return -EINVAL;

	/* streamon */
	if(enable && vd->state == TX_ISP_MODULE_RUNNING){
		return ISP_SUCCESS;
	}

	/* streamoff */
	if(!enable && vd->state != TX_ISP_MODULE_RUNNING){
		return ISP_SUCCESS;
	}

	if (enable) {
		ret = tx_isp_vic_start(vd);
		vd->state = TX_ISP_MODULE_RUNNING;
		/*printk("vic------------start 0x%08x\n", tx_isp_sd_readl(sd, TX_ISP_TOP_IRQ_ENABLE));*/
	}else {
		/*tx_isp_vic_writel(vd, VIC_CONTROL, GLB_SAFE_RST);*/
		/*tx_isp_vic_writel(vd, VIC_CONTROL, VIC_RESET);*/
		vd->state = TX_ISP_MODULE_INIT;
	//	dump_vic_reg(vsd);
		/*printk("vic------------stop 0x%08x\n", tx_isp_sd_readl(sd, TX_ISP_TOP_IRQ_ENABLE));*/
	}
	return ret;
}

static struct tx_isp_subdev_core_ops vic_core_ops = {
	.init = vic_core_ops_init,
	.ioctl = vic_core_ops_ioctl,
	.interrupt_service_routine = isp_vic_interrupt_service_routine,
};

static struct tx_isp_subdev_video_ops vic_video_ops = {
	.s_stream = vic_core_s_stream,
};

static struct tx_isp_subdev_sensor_ops vic_sensor_ops = {
	.sync_sensor_attr = vic_sensor_ops_sync_sensor_attr,
	.ioctl = vic_sensor_ops_ioctl,
};

static int tx_isp_vic_activate_subdev(struct tx_isp_subdev *sd)
{
	struct tx_isp_vic_device *vd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);

	if(IS_ERR_OR_NULL(vd))
		return -EINVAL;

	private_mutex_lock(&vd->mlock);
	if(vd->state == TX_ISP_MODULE_SLAKE){
		vd->state = TX_ISP_MODULE_ACTIVATE;
		vic_clks_ops(sd, 1);
	}
	private_mutex_unlock(&vd->mlock);
	return 0;
}

static int tx_isp_vic_slake_subdev(struct tx_isp_subdev *sd)
{
	struct tx_isp_vic_device *vd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);

	if(IS_ERR_OR_NULL(vd))
		return -EINVAL;

	if (vd->state == TX_ISP_MODULE_RUNNING){
		vic_core_s_stream(sd, 0);
	}
	if(vd->state == TX_ISP_MODULE_INIT){
		vic_core_ops_init(sd, 0);
	}
	private_mutex_lock(&vd->mlock);
	if(vd->state == TX_ISP_MODULE_ACTIVATE){
		vd->state = TX_ISP_MODULE_SLAKE;
		vic_clks_ops(sd, 0);
	}
	private_mutex_unlock(&vd->mlock);
	return 0;
}

static struct tx_isp_subdev_internal_ops vic_subdev_internal_ops ={
	.activate_module = tx_isp_vic_activate_subdev,
	.slake_module = tx_isp_vic_slake_subdev,
};

static struct tx_isp_subdev_ops vic_subdev_ops = {
	.core = &vic_core_ops,
	.video = &vic_video_ops,
	.sensor = &vic_sensor_ops,
	.internal = &vic_subdev_internal_ops,
};


#define VIC_CMD_BUF_SIZE 32
static uint8_t vic_cmd_buf[VIC_CMD_BUF_SIZE];
static ssize_t isp_vic_cmd_set(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos)
{
	int ret = 0;
	int len = 0;
	struct seq_file *m = file->private_data;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_vic_device *vd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	struct tx_isp_video_in *vin = NULL;
	unsigned int imagesize = 0;
	unsigned int lineoffset = 0;
	int loop = 100;
	char *buf = NULL;
	struct file *fd = NULL;
	/*struct inode *inode = NULL;*/
	mm_segment_t old_fs;
	loff_t *pos;

	if(IS_ERR_OR_NULL(vd)){
		len += seq_printf(m ,"Can't ops the node!\n");
		return len;
	}

	if(count > VIC_CMD_BUF_SIZE){
		buf = kzalloc((count+1), GFP_KERNEL);
		if(!buf)
			return -ENOMEM;
	}else
		buf = vic_cmd_buf;

	if(copy_from_user(buf, buffer, count)){
		if(count > VIC_CMD_BUF_SIZE)
			kfree(buf);
		return -EFAULT;
	}
	if (!strncmp(buf, "snapraw", sizeof("snapraw")-1)) {
		vin = &vd->vin;
		lineoffset = vin->mbus.width * 2;
		imagesize = lineoffset * vin->mbus.height;

		if(vin->mbus.width > VIC_DMA_OUTPUT_MAX_WIDTH){
			len += seq_printf(m ,"Can't output the width(%d)!\n", vin->mbus.width);
			return len;
		}
		if(vd->snap_paddr){
			len += seq_printf(m ,"The node is busy!\n");
			return len;
		}
		vd->snap_paddr = isp_malloc_buffer(imagesize);
		if(vd->snap_paddr){
			vd->snap_vaddr = paddr2vaddr(vd->snap_paddr);
			tx_isp_vic_writel(vd, VIC_DMA_RESET, 0x01);
			tx_isp_vic_writel(vd, VIC_DMA_RESOLUTION, vin->mbus.width << 16 | vin->mbus.height);
			tx_isp_vic_writel(vd, VIC_DMA_Y_STRID, lineoffset);
			tx_isp_vic_writel(vd, VIC_DMA_Y_BUF0, vd->snap_paddr);
			tx_isp_vic_writel(vd, VIC_DMA_CONFIG, 0x1<<31);
			while(loop){
				ret = private_wait_for_completion_interruptible(&vd->snap_comp);
				if (ret >= 0)
					break;
				loop--;
			}
			if(!loop){
				len += seq_printf(m ,"snapraw timeout!\n");
				goto exit;
			}
			/* save raw */
			fd = filp_open("/tmp/snap.raw", O_CREAT | O_WRONLY | O_TRUNC, 00766);
			if (file < 0) {
				printk("Failed to open /tmp/snap.raw\n");
				goto exit;
			}

			/* write file */
			/*inode = file->f_dentry->d_inode;*/
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			pos = &(fd->f_pos);
			vfs_write(fd, vd->snap_vaddr, imagesize, pos);
			filp_close(fd, NULL);
			set_fs(old_fs);
		}
	}
exit:
	if(vd->snap_paddr){
		isp_free_buffer(vd->snap_paddr);
		vd->snap_paddr = 0;
	}

	if(count > VIC_CMD_BUF_SIZE)
		kfree(buf);
	return count;
}

/* vic frd info */
static int isp_vic_frd_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_vic_device *vd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);

	if(IS_ERR_OR_NULL(vd)){
		ISP_ERROR("The parameter is invalid!\n");
		return 0;
	}
	len += seq_printf(m ," %d\n", vd->vic_frd_c);
	return len;
}
static int dump_isp_vic_frd_open(struct inode *inode, struct file *file)
{
	return private_single_open_size(file, isp_vic_frd_show, PDE_DATA(inode), 1024);
}
static struct file_operations isp_vic_frd_fops ={
	.read = private_seq_read,
	.open = dump_isp_vic_frd_open,
	.llseek = private_seq_lseek,
	.release = private_single_release,
	.write = isp_vic_cmd_set,
};

static int tx_isp_vic_probe(struct platform_device *pdev)
{
	struct tx_isp_vic_device *vsd = NULL;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	int ret = ISP_SUCCESS;

	vsd = (struct tx_isp_vic_device *)kzalloc(sizeof(*vsd), GFP_KERNEL);
	if(!vsd){
		ISP_ERROR("Failed to allocate vic device\n");
		ret = -1;
		goto exit;
	}

	desc = pdev->dev.platform_data;
	sd = &vsd->sd;
	ret = tx_isp_subdev_init(pdev, sd, &vic_subdev_ops);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}
	private_platform_set_drvdata(pdev, &sd->module);
	/* creat the node of printing isp info */
	tx_isp_set_subdev_debugops(sd, &isp_vic_frd_fops);
	private_spin_lock_init(&vsd->slock);
	private_mutex_init(&vsd->mlock);

	private_mutex_init(&vsd->snap_mlock);
	private_init_completion(&vsd->snap_comp);
	tx_isp_set_subdevdata(sd, vsd);
	vsd->state = TX_ISP_MODULE_SLAKE;
	dump_vsd = vsd;
	return ISP_SUCCESS;

failed_to_ispmodule:
	kfree(vsd);
exit:
	return ret;
}

static int __exit tx_isp_vic_remove(struct platform_device *pdev)
{
	struct tx_isp_module *module = private_platform_get_drvdata(pdev);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_vic_device *vsd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	private_platform_set_drvdata(pdev, NULL);
	tx_isp_subdev_deinit(sd);
	kfree(vsd);
	return 0;
}

struct platform_driver tx_isp_vic_driver = {
	.probe = tx_isp_vic_probe,
	.remove = __exit_p(tx_isp_vic_remove),
	.driver = {
		.name = TX_ISP_VIC_NAME,
		.owner = THIS_MODULE,
	},
};
