#include <linux/delay.h>
#include <media/v4l2-common.h>
#include <linux/v4l2-mediabus.h>
#include <linux/mm.h>

#include "tx-isp-csi.h"
#include "tx-isp-debug.h"

void dump_csi_reg(struct tx_isp_csi_device *csd)
{
	ISP_PRINT(ISP_INFO_LEVEL,"****>>>>> dump csi reg <<<<<******\n");
	ISP_PRINT(ISP_INFO_LEVEL,"**********VERSION =%08x\n", csi_core_read(csd, VERSION));
	ISP_PRINT(ISP_INFO_LEVEL,"**********N_LANES =%08x\n", csi_core_read(csd, N_LANES));
	ISP_PRINT(ISP_INFO_LEVEL,"**********PHY_SHUTDOWNZ = %08x\n", csi_core_read(csd, PHY_SHUTDOWNZ));
	ISP_PRINT(ISP_INFO_LEVEL,"**********DPHY_RSTZ = %08x\n", csi_core_read(csd, DPHY_RSTZ));
	ISP_PRINT(ISP_INFO_LEVEL,"**********CSI2_RESETN =%08x\n", csi_core_read(csd, CSI2_RESETN));
	ISP_PRINT(ISP_INFO_LEVEL,"**********PHY_STATE = %08x\n", csi_core_read(csd, PHY_STATE));
	ISP_PRINT(ISP_INFO_LEVEL,"**********DATA_IDS_1 = %08x\n", csi_core_read(csd, DATA_IDS_1));
	ISP_PRINT(ISP_INFO_LEVEL,"**********DATA_IDS_2 = %08x\n", csi_core_read(csd, DATA_IDS_2));
	ISP_PRINT(ISP_INFO_LEVEL,"**********ERR1 = %08x\n", csi_core_read(csd, ERR1));
	ISP_PRINT(ISP_INFO_LEVEL,"**********ERR2 = %08x\n", csi_core_read(csd, ERR2));
	ISP_PRINT(ISP_INFO_LEVEL,"**********MASK1 =%08x\n", csi_core_read(csd, MASK1));
	ISP_PRINT(ISP_INFO_LEVEL,"**********MASK2 =%08x\n", csi_core_read(csd, MASK2));
	ISP_PRINT(ISP_INFO_LEVEL,"**********PHY_TST_CTRL0 = %08x\n", csi_core_read(csd, PHY_TST_CTRL0));
	ISP_PRINT(ISP_INFO_LEVEL,"**********PHY_TST_CTRL1 = %08x\n", csi_core_read(csd, PHY_TST_CTRL1));
}
static struct tx_isp_csi_device *dump_csd = NULL;
void check_csi_error(void) {

	unsigned int temp1, temp2;
	while(1){
		dump_csi_reg(dump_csd);
		temp1 = csi_core_read(dump_csd,ERR1);
		temp2 = csi_core_read(dump_csd,ERR2);
		if(temp1 != 0)
			ISP_PRINT(ISP_INFO_LEVEL,"error-------- 1:0x%08x\n", temp1);
		if(temp2 != 0)
			ISP_PRINT(ISP_INFO_LEVEL,"error-------- 2:0x%08x\n", temp2);
	}
}

static unsigned char csi_core_write_part(struct tx_isp_csi_device *csd,unsigned int address,
					 unsigned int data, unsigned char shift, unsigned char width)
{
	unsigned int mask = (1 << width) - 1;
	unsigned int temp = csi_core_read(csd,address);
	temp &= ~(mask << shift);
	temp |= (data & mask) << shift;
	csi_core_write(csd,address, temp);

	return 0;
}

unsigned char csi_set_on_lanes(struct tx_isp_csi_device *csd,unsigned char lanes)
{
	ISP_PRINT(ISP_INFO_LEVEL,"%s:----------> lane num: %d\n", __func__, lanes);
	return csi_core_write_part(csd,N_LANES, (lanes - 1), 0, 2);
}

static int csi_phy_release(struct tx_isp_csi_device *csd)
{
	csi_core_write_part(csd,CSI2_RESETN, 0, 0, 1);
	csi_core_write_part(csd,CSI2_RESETN, 1, 0, 1);
	return 0;
}

static long csi_phy_start(struct tx_isp_csi_device *csd)
{
	csd->state = TX_ISP_MODULE_RUNNING;
	return 0;
}

static int csi_phy_stop(struct tx_isp_csi_device *csd)
{
	csd->state = TX_ISP_MODULE_INIT;
	return 0;
}

static int csi_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = ISP_SUCCESS;
	struct tx_isp_csi_device *csd = IS_ERR_OR_NULL(sd) ? NULL : sd_to_csi_device(sd);

	if(IS_ERR_OR_NULL(csd))
		return 0;

	switch(cmd){
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if(csd->vin.attr->dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = csi_phy_stop(csd);
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if(csd->vin.attr->dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = csi_phy_start(csd);
		}
		break;
	default:
		break;
	}
	return ret;
}

static int csi_clks_ops(struct tx_isp_subdev *sd, int on)
{
	struct clk **clks = IS_ERR_OR_NULL(sd) ? NULL : sd->clks;
	int i = 0;

	if(IS_ERR_OR_NULL(clks))
		return 0;

	if(on){
		for(i = 0; i < sd->clk_num; i++)
			private_clk_enable(clks[i]);
	}else{
		for(i = sd->clk_num - 1; i >=0; i--)
			private_clk_disable(clks[i]);
	}
	return 0;
}

static int csi_core_ops_init(struct tx_isp_subdev *sd, int on)
{
	struct tx_isp_csi_device *csd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);

	if(IS_ERR_OR_NULL(csd))
		return -EINVAL;

	if(on){
		if(csd->vin.attr->dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI){
			csi_set_on_lanes(csd, csd->vin.attr->mipi.lans);
			csi_core_write_part(csd,PHY_SHUTDOWNZ, 0, 0, 1);
			mdelay(10);
			csi_core_write_part(csd,PHY_SHUTDOWNZ, 1, 0, 1);
			csi_core_write_part(csd,DPHY_RSTZ, 0, 0, 1);
			msleep(10);
			csi_core_write_part(csd,DPHY_RSTZ, 1, 0, 1);


			csi_core_write_part(csd,PHY_TST_CTRL0, 0, 0, 1);
			csi_core_write(csd,PHY_TST_CTRL1, 0);
			csi_core_write_part(csd,PHY_TST_CTRL1, 0, 16, 1);
			csi_core_write_part(csd,PHY_TST_CTRL0, 0, 1, 1);

			//phy init as fllows1.8
			csi_phy_write(csd, PHY_CRTL0, 0x7d);
			csi_phy_write(csd, CK_LANE_CONFIG, 0x3f);
			csi_phy_write(csd, PHY_CRTL1, 0x3f);
			csi_phy_write(csd, PHY_LVDS_MODE, 0x1e);
			csi_phy_write(csd, PHY_CRTL1, 0x1f);
			msleep(10);

			if(csd->vin.attr->mipi.mode == SENSOR_MIPI_SONY_MODE){
				csi_phy_write(csd, CK_LANE_SETTLE_TIME, 0x86);
				csi_phy_write(csd, PHY_DT0_LANE_SETTLE_TIME, 0x86);
				csi_phy_write(csd, PHY_DT1_LANE_SETTLE_TIME, 0x86);
				msleep(10);
			}
			//phy init finished
			csi_core_write_part(csd,CSI2_RESETN, 0, 0, 1);
			mdelay(10);
			csi_core_write_part(csd,CSI2_RESETN, 1, 0, 1);
			mdelay(10);
		}else if(csd->vin.attr->dbus_type == TX_SENSOR_DATA_INTERFACE_DVP){
			csi_core_write(csd, DPHY_RSTZ, 0x0);
			csi_core_write(csd, DPHY_RSTZ, 0x1);

			csi_phy_write(csd, PHY_CRTL0, 0x7d);
			csi_phy_write(csd, PHY_CRTL1, 0x3e);
			csi_phy_write(csd, PHY_LVDS_MODE, 0x1e);
			csi_phy_write(csd, PHY_MODEL_SWITCH, 0x1);
#if 0
			*(volatile unsigned int*)(0xb004000c) = 0x0;
			*(volatile unsigned int*)(0xb004000c) = 0x1;
			*(volatile unsigned int*)(0xb0022000) = 0x7d;
			*(volatile unsigned int*)(0xb0022080) = 0x3e;//0x3f
			*(volatile unsigned int*)(0xb0022300) = 0x1e;//disable
			*(volatile unsigned int*)(0xb00222cc) = 0x1;
#endif
		}else{
			ISP_WRANING("The sensor dbus_type is %d\n", csd->vin.attr->dbus_type);
		}
		csd->state = TX_ISP_MODULE_INIT;
	}else{
		/*reset phy*/
		csi_core_write_part(csd,PHY_SHUTDOWNZ, 0, 0, 1);
		csi_core_write_part(csd,DPHY_RSTZ, 0, 0, 1);
		csi_core_write_part(csd,CSI2_RESETN, 0, 0, 1);
		csd->state = TX_ISP_MODULE_DEINIT;
	}
	return 0;
}

static int csi_sensor_ops_sync_sensor_attr(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_csi_device *csd = IS_ERR_OR_NULL(sd) ? NULL : sd_to_csi_device(sd);
	if(IS_ERR_OR_NULL(csd)){
		ISP_ERROR("The parameter is invalid!\n");
		return -EINVAL;
	}
	/*printk("## %s %d arg=%p ##\n", __func__,__LINE__, arg);*/
	if(arg)
		memcpy(&csd->vin, (void *)arg, sizeof(struct tx_isp_video_in));
	else
		memset(&csd->vin, 0, sizeof(struct tx_isp_video_in));
	return 0;
}

static int csi_video_s_stream(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_csi_device *csd = IS_ERR_OR_NULL(sd) ? NULL : sd_to_csi_device(sd);
	int ret = ISP_SUCCESS;

	if(IS_ERR_OR_NULL(csd)){
		ISP_ERROR("The parameter is invalid!\n");
		return -EINVAL;
	}

	/*printk("## %s %d attr=%p ##\n", __func__,__LINE__, csd->vin.attr);*/
	if(csd->vin.attr->dbus_type == TX_SENSOR_DATA_INTERFACE_MIPI){
		if (enable) {
			ret = csi_phy_start(csd);//sd--lanes ???????
			/*printk("csi------------start\n");*/
		}
		else {
			ret = csi_phy_stop(csd);
			/*printk("csi--------------stop\n");*/
		}
	}else{
		/*printk("the sensor is not mipi bus\n");*/
	}
	return ret;
}

static int tx_isp_csi_activate_subdev(struct tx_isp_subdev *sd)
{
	struct tx_isp_csi_device *csd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);

	if(IS_ERR_OR_NULL(csd))
		return -EINVAL;

	private_mutex_lock(&csd->mlock);
	if(csd->state == TX_ISP_MODULE_SLAKE){
		csd->state = TX_ISP_MODULE_ACTIVATE;
		csi_clks_ops(sd, 1);
	}
	private_mutex_unlock(&csd->mlock);
	return 0;
}

static int tx_isp_csi_slake_subdev(struct tx_isp_subdev *sd)
{
	struct tx_isp_csi_device *csd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);

	if(IS_ERR_OR_NULL(csd))
		return -EINVAL;

	if (csd->state == TX_ISP_MODULE_RUNNING){
		csi_video_s_stream(sd, 0);
	}

	if(csd->state == TX_ISP_MODULE_INIT){
		csi_core_ops_init(sd, 0);
	}

	private_mutex_lock(&csd->mlock);
	if(csd->state == TX_ISP_MODULE_ACTIVATE){
		csd->state = TX_ISP_MODULE_SLAKE;
		csi_clks_ops(sd, 0);
	}
	private_mutex_unlock(&csd->mlock);
	return 0;
}

static struct tx_isp_subdev_internal_ops csi_subdev_internal_ops ={
	.activate_module = tx_isp_csi_activate_subdev,
	.slake_module = tx_isp_csi_slake_subdev,
};

static struct tx_isp_subdev_core_ops csi_core_ops = {
	.init = csi_core_ops_init,
};

static struct tx_isp_subdev_video_ops csi_video_ops = {
	.s_stream = csi_video_s_stream,
};

static struct tx_isp_subdev_sensor_ops csi_sensor_ops = {
	.sync_sensor_attr = csi_sensor_ops_sync_sensor_attr,
	.ioctl = csi_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops csi_subdev_ops = {
	.core = &csi_core_ops,
	.video = &csi_video_ops,
	.sensor = &csi_sensor_ops,
	.internal = &csi_subdev_internal_ops,
};

static int isp_csi_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_csi_device *csd = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdevdata(sd);
	volatile unsigned int err1,err2;
	if(IS_ERR_OR_NULL(csd)){
		ISP_ERROR("The parameter is invalid!\n");
		return 0;
	}
	err1 = csi_core_read(csd, ERR1);
	err2 = csi_core_read(csd, ERR2);
	if (err1 != 0)
		len += seq_printf(m ,"0x0020 is  0x%x\n", err1);
	if (err2 != 0)
		len += seq_printf(m ,"0x0024 is  0x%x\n", err2);
	if ((err1 != 0) || (err2 != 0))
		len += seq_printf(m ,"0x0014 is  0x%x\n", csi_core_read(csd, PHY_STATE));
	return len;
}

static int dump_isp_csi_open(struct inode *inode, struct file *file)
{
	return private_single_open_size(file, isp_csi_show, PDE_DATA(inode), 1024);
}

static struct file_operations isp_csi_fops ={
	.read = private_seq_read,
	.open = dump_isp_csi_open,
	.llseek = private_seq_lseek,
	.release = private_single_release,
};

static int tx_isp_csi_probe(struct platform_device *pdev)
{
	struct tx_isp_csi_device *csd = NULL;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	struct resource *res = NULL;
	int ret = ISP_SUCCESS;

	csd = (struct tx_isp_csi_device *)kzalloc(sizeof(*csd), GFP_KERNEL);
	if(!csd){
		ISP_ERROR("Failed to allocate csi device\n");
		ret = -ENOMEM;
		goto exit;
	}

	desc = pdev->dev.platform_data;
	sd = &csd->sd;
	ret = tx_isp_subdev_init(pdev, sd, &csi_subdev_ops);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}

	res = private_request_mem_region(MIPI_PHY_IOBASE,
					 0x1000, "mipi-phy");
	if (!res) {
		ISP_ERROR("%s[%d] Not enough memory for resources\n", __func__,__LINE__);
		ret = -EBUSY;
		goto failed_req;
	}

	csd->phy_base = private_ioremap(res->start, res->end - res->start + 1);
	if (!sd->base) {
		ISP_ERROR("%s[%d] Unable to ioremap registers\n", __func__,__LINE__);
		ret = -ENXIO;
		goto failed_ioremap;
	}
	csd->phy_res = res;
	tx_isp_set_subdev_debugops(sd, &isp_csi_fops);
	private_mutex_init(&csd->mlock);
	private_platform_set_drvdata(pdev, &sd->module);
	tx_isp_set_subdevdata(sd, csd);
	csd->state = TX_ISP_MODULE_SLAKE;

	dump_csd = csd;
	return ISP_SUCCESS;
failed_ioremap:
	private_release_mem_region(res->start, res->end - res->start + 1);
failed_req:
	tx_isp_subdev_deinit(sd);
failed_to_ispmodule:
	kfree(csd);
exit:
	return ret;
}

static int __exit tx_isp_csi_remove(struct platform_device *pdev)
{
	struct tx_isp_module *module = private_platform_get_drvdata(pdev);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_csi_device *csd = IS_ERR_OR_NULL(sd) ? NULL : sd_to_csi_device(sd);
	struct resource *res = csd->phy_res;

	csi_phy_release(csd);
	private_platform_set_drvdata(pdev, NULL);

	private_iounmap(csd->phy_base);
	private_release_mem_region(res->start, res->end - res->start + 1);
	tx_isp_subdev_deinit(sd);
	kfree(csd);
	return 0;
}

struct platform_driver tx_isp_csi_driver = {
	.probe = tx_isp_csi_probe,
	.remove = __exit_p(tx_isp_csi_remove),
	.driver = {
		.name = TX_ISP_CSI_NAME,
		.owner = THIS_MODULE,
	},
};
