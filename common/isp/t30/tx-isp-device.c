/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/io.h>
/*#include <linux/list.h>*/
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <mach/platform.h>

#include <tx-isp-common.h>
#include "tx-isp-interrupt.h"
#include "tx-isp-debug.h"
#include "videoin/tx-isp-vic.h"
#include "videoin/tx-isp-csi.h"
#include "videoin/tx-isp-video-in.h"
#include "apical-isp/tx-isp-core.h"

extern struct platform_device tx_isp_platform_device;

#define TX_ISP_DRIVER_VERSION "H20190725b"

static struct tx_isp_device* globe_ispdev = NULL;

static int private_cpm_reset(unsigned int addr, unsigned int bit)
{
	int timeout = 500;
	unsigned int value = 0;
	value = *(volatile unsigned int*)(addr);
	value |= 1<<(bit-1);
	*(volatile unsigned int*)(addr) = value;
	while(timeout && ((*(volatile unsigned int*)(addr)) & (1<<(bit-2))) == 0){
		private_msleep(2);
		timeout--;
	}
	if(timeout == 0)
		return -1;

	value = *(volatile unsigned int*)(addr);
	value &= ~(1<<(bit-1));
	value |= 1<<bit;
	*(volatile unsigned int*)(addr) = value;

	value = *(volatile unsigned int*)(addr);
	value &= ~(1<<bit);
	*(volatile unsigned int*)(addr) = value;
	return 0;
}

int private_reset_tx_isp_module(enum tx_isp_subdev_id id)
{
	unsigned int addr = 0;
	unsigned int bit = 0;
	switch(id){
		case TX_ISP_CORE_SUBDEV_ID:
			addr = 0xb00000c4;
			bit = 22;
			break;
		case TX_ISP_LDC_SUBDEV_ID:
			addr = 0xb00000ac;
			bit = 8;
			break;
		case TX_ISP_NCU_SUBDEV_ID:
			addr = 0xb00000ac;
			bit = 2;
			break;
		case TX_ISP_MSCALER_SUBDEV_ID:
			addr = 0xb00000ac;
			bit = 5;
			break;
		default:
			break;
	}
	if(addr){
		return private_cpm_reset(addr, bit);
	}
	return 0;
}


int tx_isp_reg_set(struct tx_isp_subdev *sd, unsigned int reg, int start, int end, int val)
{
	int ret = 0;
	int i = 0, mask = 0;
	unsigned int oldv = 0;
	unsigned int new = 0;
	for (i = 0;  i < (end - start + 1); i++) {
		mask += (1 << (start + i));
	}

	oldv = tx_isp_sd_readl(sd, reg);
	new = oldv & (~mask);
	new |= val << start;
	tx_isp_sd_writel(sd, reg, new);

	return ret;
}

static struct tx_isp_link_config link1[] = {
	{
		.src = {TX_ISP_CORE_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.dst = {TX_ISP_NCU_NAME, TX_ISP_PADTYPE_INPUT, 0},
		.flag = TX_ISP_PADLINK_LFB,
	},
	{
		.src = {TX_ISP_NCU_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.dst = {TX_ISP_MSCALER_NAME, TX_ISP_PADTYPE_INPUT, 0},
		.flag = TX_ISP_PADLINK_LFB,
	},
	{
		.src = {TX_ISP_MSCALER_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.dst = {TX_ISP_FS_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.flag = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_FS,
	},
	{
		.src = {TX_ISP_MSCALER_NAME, TX_ISP_PADTYPE_OUTPUT, 1},
		.dst = {TX_ISP_FS_NAME, TX_ISP_PADTYPE_OUTPUT, 1},
		.flag = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_FS,
	},
	{
		.src = {TX_ISP_MSCALER_NAME, TX_ISP_PADTYPE_OUTPUT, 2},
		.dst = {TX_ISP_FS_NAME, TX_ISP_PADTYPE_OUTPUT, 2},
		.flag = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_FS,
	},
};

static struct tx_isp_link_config link2[] = {
	{
		.src = {TX_ISP_CORE_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.dst = {TX_ISP_FS_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.flag = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_FS,
	},
	{
		.src = {TX_ISP_CORE_NAME, TX_ISP_PADTYPE_OUTPUT, 1},
		.dst = {TX_ISP_FS_NAME, TX_ISP_PADTYPE_OUTPUT, 1},
		.flag = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_FS,
	},
};

static struct tx_isp_link_config link3[] = {
	{
		.src = {TX_ISP_CORE_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.dst = {TX_ISP_LDC_NAME, TX_ISP_PADTYPE_INPUT, 0},
		.flag = TX_ISP_PADLINK_DDR,
	},
	{
		.src = {TX_ISP_LDC_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.dst = {TX_ISP_NCU_NAME, TX_ISP_PADTYPE_INPUT, 0},
		.flag = TX_ISP_PADLINK_DDR,
	},
	{
		.src = {TX_ISP_NCU_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.dst = {TX_ISP_MSCALER_NAME, TX_ISP_PADTYPE_INPUT, 0},
		.flag = TX_ISP_PADLINK_LFB,
	},
	{
		.src = {TX_ISP_MSCALER_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.dst = {TX_ISP_FS_NAME, TX_ISP_PADTYPE_OUTPUT, 0},
		.flag = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_FS,
	},
	{
		.src = {TX_ISP_MSCALER_NAME, TX_ISP_PADTYPE_OUTPUT, 1},
		.dst = {TX_ISP_FS_NAME, TX_ISP_PADTYPE_OUTPUT, 1},
		.flag = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_FS,
	},
	{
		.src = {TX_ISP_MSCALER_NAME, TX_ISP_PADTYPE_OUTPUT, 2},
		.dst = {TX_ISP_FS_NAME, TX_ISP_PADTYPE_OUTPUT, 2},
		.flag = TX_ISP_PADLINK_DDR | TX_ISP_PADLINK_FS,
	},
};

static struct tx_isp_link_configs configs[] = {
	{link1, ARRAY_SIZE(link1)},
	{link2, ARRAY_SIZE(link2)},
	{link3, ARRAY_SIZE(link3)},
};

static int configs_num = ARRAY_SIZE(configs);

static struct tx_isp_subdev_pad* find_subdev_link_pad(struct tx_isp_module *module, struct link_pad_description *desc)
{
	int index = 0;
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev_pad* pad = NULL;
	struct tx_isp_subdev *subdev = NULL;

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			submod = module->submods[index];
			if(strcmp(submod->name, desc->name) == 0)
				break;
			else
				submod = NULL;
		}
	}
	/*printk("## %s %d name = %s index = %d type = 0x%08x ##\n", __func__, __LINE__, desc->name, desc->index, desc->type);*/
	if(submod){
		subdev = module_to_subdev(submod);
		if(desc->type == TX_ISP_PADTYPE_INPUT && desc->index < subdev->num_inpads)
			pad = &(subdev->inpads[desc->index]);
		else if(desc->type == TX_ISP_PADTYPE_OUTPUT && desc->index < subdev->num_outpads)
			pad = &(subdev->outpads[desc->index]);
		else
			ISP_ERROR("Can't find the matched pad!\n");
	}
	return pad;
}


static int subdev_video_destroy_link(struct tx_isp_subdev_link *link)
{
	struct tx_isp_subdev_pad *source = NULL;	/* Source pad */
	struct tx_isp_subdev_pad *sink = NULL;		/* Sink pad  */
	struct tx_isp_subdev_link *backlink = NULL;
	if(link->flag == TX_ISP_LINKFLAG_DYNAMIC)
		return 0;
#if 0
	if(link->source->state == TX_ISP_PADSTATE_STREAM || link->sink->state == TX_ISP_PADSTATE_STREAM){
		ISP_ERROR("Please stop active links firstly! %d\n", __LINE__);
		return -EPERM;
	}
#endif
	source = link->source;
	sink = link->sink;
	backlink = link->reverse;

	link->source = NULL;
	link->sink = NULL;
	link->reverse = NULL;
	link->flag = TX_ISP_LINKFLAG_DYNAMIC;
	source->state = TX_ISP_PADSTATE_FREE;

	if(backlink){
		backlink->source = NULL;
		backlink->sink = NULL;
		backlink->reverse = NULL;
		backlink->flag = TX_ISP_LINKFLAG_DYNAMIC;
	}
	if(sink)
		sink->state = TX_ISP_PADSTATE_FREE;
	return 0;
}

static int subdev_video_setup_link(struct tx_isp_subdev_pad *local, struct tx_isp_subdev_pad *remote, unsigned int flag)
{
	int ret = 0;
	/*struct tx_isp_subdev_link *link = NULL;*/
	/*struct tx_isp_subdev_link *backlink = NULL;*/

	/* check links_type of pads  */
	if(!((local->links_type & remote->links_type) & flag)){
		ISP_ERROR("The link type is mismatch!\n");
		return -EPERM;
	}

	/* check state of pads */
	if(local->state == TX_ISP_PADSTATE_STREAM || remote->state == TX_ISP_PADSTATE_STREAM){
		ISP_ERROR("Please stop active links firstly! %d\n", __LINE__);
		return -EPERM;
	}

	if(local->state == TX_ISP_PADSTATE_LINKED){
		if(local->link.sink != remote){
			ret = subdev_video_destroy_link(&local->link);
			if(ret)
				return ret;
		}
	}

	if(remote->state == TX_ISP_PADSTATE_LINKED){
		if(remote->link.sink != local){
			ret = subdev_video_destroy_link(&remote->link);
			if(ret)
				return ret;
		}
	}

	local->link.source = local;
	local->link.sink = remote;
	local->link.reverse = &remote->link;
	local->link.flag = flag | TX_ISP_LINKFLAG_ENABLED;
	local->state = TX_ISP_PADSTATE_LINKED;

	remote->link.source = remote;
	remote->link.sink = local;
	remote->link.reverse = &local->link;
	remote->link.flag = flag | TX_ISP_LINKFLAG_ENABLED;
	remote->state = TX_ISP_PADSTATE_LINKED;

	return 0;
}

extern unsigned int ispscalerwh;
extern unsigned int ispcropwh;
extern unsigned int ispw;
extern unsigned int isph;

static int tx_isp_video_link_setup(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct tx_isp_subdev_pad *src = NULL;
	struct tx_isp_subdev_pad *dst = NULL;
	struct tx_isp_link_config *config = NULL;
	int index = 0;
	int length = 0;
	int link;
	unsigned int resolution = 0;
	if (copy_from_user(&link, (void __user *)arg, sizeof(link))) {
		ISP_ERROR("[%s][%d] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	if(link < 0 || link >= configs_num){
		ISP_ERROR("link(%d) is invalid!\n", link);
		return -EINVAL;
	}

	/*printk("## %s %d link = %d ##\n", __func__, __LINE__, link);*/
	if(ispdev->active_link == link)
		return 0;

	config = configs[link].config;
	length = configs[link].length;

	if(link == 0 || link == 2)
		config[0].src.index = ispscalerwh ? 1 : 0;

	for(index = 0; index < length; index++){
		src = find_subdev_link_pad(module, &(config[index].src));
		dst = find_subdev_link_pad(module, &(config[index].dst));
		if(src && dst){
			ret = subdev_video_setup_link(src, dst, config[index].flag);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}
	ispdev->active_link = link;

	/* return the resolution of isp output to api */
	resolution = (ispw << 16) | isph;
	if (copy_to_user((void __user *)arg, &resolution, sizeof(resolution))) {
		ISP_ERROR("[%s][%d] copy to user error\n", __func__, __LINE__);
		return -EFAULT;
	}
	return 0;
exit:
	return ret;
}

static int tx_isp_video_link_destroy(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct tx_isp_subdev_pad *src = NULL;
	struct tx_isp_subdev_pad *dst = NULL;
	struct tx_isp_link_config *config = NULL;
	int index = 0;
	int length = 0;

	if(ispdev->active_link < 0)
		return 0;

	/*printk("## %s %d link = %d ##\n", __func__, __LINE__, ispdev->active_link);*/
	config = configs[ispdev->active_link].config;
	length = configs[ispdev->active_link].length;
	for(index = 0; index < length; index++){
		src = find_subdev_link_pad(module, &(config[index].src));
		dst = find_subdev_link_pad(module, &(config[index].dst));
		if(src && dst){
			ret = subdev_video_destroy_link(&src->link);
			ret = subdev_video_destroy_link(&dst->link);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}
	ispdev->active_link = -1;
	return 0;
exit:
	return ret;
}


static int tx_isp_sensor_enum_input(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;
	struct v4l2_input input;
	if (copy_from_user(&input, (void __user *)arg, sizeof(input))) {
		ISP_ERROR("[%s][%d] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, sensor, ioctl, TX_ISP_EVENT_SENSOR_ENUM_INPUT, &input);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	if (copy_to_user((void __user *)arg, &input, sizeof(input))) {
		ISP_ERROR("[%s][%d] copy to user error\n", __func__, __LINE__);
		return -EFAULT;
	}
	return 0;
exit:
	return ret;
}

static int tx_isp_sensor_get_input(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;
	int input;

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, sensor, ioctl, TX_ISP_EVENT_SENSOR_GET_INPUT, &input);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	if (copy_to_user((void __user *)arg, &input, sizeof(input))) {
		ISP_ERROR("[%s][%d] copy to user error\n", __func__, __LINE__);
		return -EFAULT;
	}
	return 0;
exit:
	return ret;
}

static int tx_isp_sensor_set_input(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;
	int input;
	if (copy_from_user(&input, (void __user *)arg, sizeof(input))) {
		ISP_ERROR("[%s][%d] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, sensor, ioctl, TX_ISP_EVENT_SENSOR_SET_INPUT, &input);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	/* return the resolution of sensor output to api */
	if (copy_to_user((void __user *)arg, &input, sizeof(input))) {
		ISP_ERROR("[%s][%d] copy to user error\n", __func__, __LINE__);
		return -EFAULT;
	}
	return 0;
exit:
	return ret;
}

static int tx_isp_sensor_register_sensor(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;
	struct tx_isp_sensor_register_info info;
	if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
		ISP_ERROR("[%s][%d] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, sensor, ioctl, TX_ISP_EVENT_SENSOR_REGISTER, &info);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	return 0;
exit:
	return ret;
}

static int tx_isp_sensor_release_sensor(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;
	struct tx_isp_sensor_register_info info;
	if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
		ISP_ERROR("[%s][%d] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, sensor, ioctl, TX_ISP_EVENT_SENSOR_RELEASE, &info);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	return 0;
exit:
	return ret;
}

static int tx_isp_video_s_stream(struct tx_isp_module *module, unsigned long enable)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, video, s_stream, enable);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	return 0;
exit:
	for(; index > 0; index--){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			tx_isp_subdev_call(subdev, video, s_stream, !enable);
		}
	}
	return ret;
}

static int tx_isp_video_link_stream(struct tx_isp_module *module, unsigned long enable)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;

	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, video, link_stream, enable);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	return 0;
exit:
	for(; index > 0; index--){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			tx_isp_subdev_call(subdev, video, link_stream, !enable);
		}
	}
	return ret;
}

static int tx_isp_sensor_s_register(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	struct tx_isp_dbg_register dbg;
	struct v4l2_dbg_register reg;
	int index = 0;

	if (copy_from_user(&reg, (void __user *)arg, sizeof(reg))) {
		ISP_ERROR("[%s][%d] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	dbg.name = reg.match.name;
	dbg.reg = reg.reg;
	dbg.val = reg.val;
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, sensor, ioctl, TX_ISP_EVENT_SENSOR_S_REGISTER, &dbg);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	return 0;
exit:
	return ret;
}

static int tx_isp_sensor_g_register(struct tx_isp_module *module, unsigned long arg)
{
	int ret = 0;
	struct tx_isp_subdev *subdev = NULL;
	struct tx_isp_dbg_register dbg;
	struct v4l2_dbg_register reg;
	int index = 0;

	if (copy_from_user(&reg, (void __user *)arg, sizeof(reg))) {
		ISP_ERROR("[%s][%d] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	dbg.name = reg.match.name;
	dbg.reg = reg.reg;
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		if(module->submods[index]){
			subdev = module_to_subdev(module->submods[index]);
			ret = tx_isp_subdev_call(subdev, sensor, ioctl, TX_ISP_EVENT_SENSOR_G_REGISTER, &dbg);
			if(ret && ret != -ENOIOCTLCMD)
				goto exit;
		}
	}

	reg.val = dbg.val;
	reg.size = dbg.size;
	if (copy_to_user((void __user *)arg, &reg, sizeof(reg))) {
		ISP_ERROR("[%s][%d] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}
	return 0;
exit:
	return ret;
}


static long tx_isp_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	struct tx_isp_module *module = miscdev_to_module(dev);
	int ret = 0;
	switch(cmd){
		case VIDIOC_ENUMINPUT:
			ret = tx_isp_sensor_enum_input(module, arg);
			break;
		case VIDIOC_G_INPUT:
			ret = tx_isp_sensor_get_input(module, arg);
			break;
		case VIDIOC_S_INPUT:
			ret = tx_isp_sensor_set_input(module, arg);
			break;
		case VIDIOC_REGISTER_SENSOR:
			ret = tx_isp_sensor_register_sensor(module, arg);
			break;
		case VIDIOC_RELEASE_SENSOR:
			ret = tx_isp_sensor_release_sensor(module, arg);
			break;
		case VIDIOC_STREAMON:
			ret = tx_isp_video_s_stream(module, 1);
			break;
		case VIDIOC_STREAMOFF:
			ret = tx_isp_video_s_stream(module, 0);
			break;
		case VIDIOC_CREATE_SUBDEV_LINKS:
			ret = tx_isp_video_link_setup(module, arg);
			break;
		case VIDIOC_DESTROY_SUBDEV_LINKS:
			ret = tx_isp_video_link_destroy(module, arg);
			break;
		case VIDIOC_LINKS_STREAMON:
			ret = tx_isp_video_link_stream(module, 1);
			break;
		case VIDIOC_LINKS_STREAMOFF:
			ret = tx_isp_video_link_stream(module, 0);
			break;
		case VIDIOC_DBG_S_REGISTER:
			ret = tx_isp_sensor_s_register(module, arg);
			break;
		case VIDIOC_DBG_G_REGISTER:
			ret = tx_isp_sensor_g_register(module, arg);
			break;
		default:
			break;
	}
	return ret;
}

static int tx_isp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_module *module = miscdev_to_module(dev);
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;
	int ret = 0;

	if(ispdev->refcnt){
		ispdev->refcnt++;
		return 0;
	}

	/*printk("## %s %d ##\n",__func__,__LINE__);	*/
	ispdev->active_link = -1;
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		submod = module->submods[index];
		if(submod){
			subdev = module_to_subdev(submod);
			ret = tx_isp_subdev_call(subdev, internal, activate_module);
			if(ret && ret != -ENOIOCTLCMD)
				break;
	/*printk("## %s %d name = %s ret = %d ops = %p ##\n",__func__,__LINE__, submod->name, ret, subdev->ops);	*/
		}
	}
	if(ret == -ENOIOCTLCMD)
		return 0;

	return ret;
}

static int tx_isp_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_module *module = miscdev_to_module(dev);
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *subdev = NULL;
	int ret = 0;
	int index = 0;
	if(ispdev->refcnt){
		ispdev->refcnt--;
		return 0;
	}

	for(index = TX_ISP_ENTITY_ENUM_MAX_DEPTH - 1; index >= 0; index--){
		submod = module->submods[index];
		if(submod){
			subdev = module_to_subdev(submod);
			ret = tx_isp_subdev_call(subdev, internal, slake_module);
			if(ret && ret != -ENOIOCTLCMD)
				break;
		}
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;

	if(!ret && ispdev->active_link >= 0)
		tx_isp_video_link_destroy(module, 0);

	return ret;
}

/**********************************   new  ********************************************/

int tx_isp_send_event_to_remote(struct tx_isp_subdev_pad *pad, unsigned int cmd, void *data)
{
	int ret = 0;
	if(pad && pad->link.sink && pad->link.sink->event)
		ret = pad->link.sink->event(pad->link.sink, cmd, data);
	else
		ret = -ENOIOCTLCMD;
	return ret;
}

static int tx_isp_notify(struct tx_isp_module *this, unsigned int notification, void *data)
{
	int ret = 0;
	struct tx_isp_module *module = &globe_ispdev->module;
	struct tx_isp_module *submod = NULL;
	struct tx_isp_subdev *subdev = NULL;
	int index = 0;

	/* */
	for(index = 0; index < TX_ISP_ENTITY_ENUM_MAX_DEPTH; index++){
		submod = module->submods[index];
		if(submod){
			subdev = module_to_subdev(submod);
			if(NOTIFICATION_TYPE_OPS(notification) == NOTIFICATION_TYPE_CORE_OPS)
				ret = tx_isp_subdev_call(subdev, core, ioctl, notification, data);
			else if(NOTIFICATION_TYPE_OPS(notification) == NOTIFICATION_TYPE_SENSOR_OPS)
				ret = tx_isp_subdev_call(subdev, sensor, ioctl, notification, data);
			else
				ret = 0;
			if(ret && ret != -ENOIOCTLCMD)
				break;
		}

	}
	if(ret == -ENOIOCTLCMD)
		return 0;
	return ret;
}

extern struct platform_driver tx_isp_vin_driver;
extern struct platform_driver tx_isp_csi_driver;
extern struct platform_driver tx_isp_vic_driver;
extern struct platform_driver tx_isp_core_driver;
extern struct platform_driver tx_isp_ldc_driver;
extern struct platform_driver tx_isp_ncu_driver;
extern struct platform_driver tx_isp_mscaler_driver;
extern struct platform_driver tx_isp_fs_driver;

static struct platform_driver *isp_drivers[] = {
	&tx_isp_vin_driver,
	&tx_isp_csi_driver,
	&tx_isp_vic_driver,
	&tx_isp_core_driver,
	&tx_isp_ldc_driver,
	&tx_isp_ncu_driver,
	&tx_isp_mscaler_driver,
	&tx_isp_fs_driver,
};

static unsigned int drivers_num = ARRAY_SIZE(isp_drivers);

static void tx_isp_unregister_platforms(struct tx_isp_platform *pform)
{
	int index = 0;

	for(index = 0; index < TX_ISP_PLATFORM_MAX_NUM; index++){
#if 0
		if(pform[index].drv){
			private_platform_driver_unregister(pform[index].drv);
		}
		if(pform[index].dev){
			private_platform_device_unregister(pform[index].dev);
		}
#else
		if(pform[index].drv){
			pform[index].drv->remove(pform[index].dev);
		}
		if(pform[index].dev){
			private_platform_device_unregister(pform[index].dev);
		}
#endif
	}
	return;
}


static int tx_isp_match_and_register_platforms(struct tx_isp_device_descriptor *desc, struct tx_isp_platform *pform)
{
	struct platform_device *dev;
	struct platform_driver *drv;
	int dev_index, drv_index;
	int ret = 0;
	if(desc == NULL)
		return -EINVAL;
	/*printk("%s %d\n", __func__, __LINE__);*/
	if(desc->entity_num > TX_ISP_PLATFORM_MAX_NUM)
	{
		ISP_ERROR("The number of platform device is too much!\n");
		return -EINVAL;
	}
	for(dev_index = 0; dev_index < desc->entity_num; dev_index++){
		dev = desc->entities[dev_index];
		pform[dev_index].dev = dev;
		for(drv_index = 0; drv_index < drivers_num; drv_index++){
			drv = isp_drivers[drv_index];
			if(strcmp(dev->name, drv->driver.name) == 0){
				ret = private_platform_device_register(dev);
				if(ret){
					ISP_ERROR("Failed to register isp device(%d)\n", dev_index);
					pform[dev_index].dev = NULL;
					goto error;
				}
#if 0
				printk("%s %d\n", __func__, __LINE__);
				ret = private_platform_driver_register(drv);
				if(ret){
					ISP_ERROR("Failed to register isp driver(%d)\n", drv_index);
					goto error;
				}
#else
				/*printk("%s %d name = %s\n", __func__, __LINE__,dev->name);*/
				if(drv->probe)
					drv->probe(dev);
#endif
				pform[dev_index].drv = drv;
				break;
			}
		}
	}

	return 0;
error:
	tx_isp_unregister_platforms(pform);
	return ret;
}

int tx_isp_module_init(struct platform_device *pdev, struct tx_isp_module *module)
{
	int ret = 0;
	struct tx_isp_descriptor *desc = NULL;
	if(!module){
		ISP_ERROR("%s the parameters are invalid!\n", __func__);
		return -EINVAL;
	}
	desc = pdev ? pdev->dev.platform_data : NULL;
	if(desc)
		module->desc = *desc;
	module->parent = NULL;
	memset(module->submods, 0, sizeof(module->submods));
	module->name = pdev->name;
	module->ops = NULL;
	module->debug_ops = NULL;
	module->dev = &(pdev->dev);
	module->notify = tx_isp_notify;

	return ret;
}

void tx_isp_module_deinit(struct tx_isp_module *module)
{
	if(module)
		memset(module, 0, sizeof(*module));
}

static int isp_subdev_init_clks(struct tx_isp_subdev *sd, struct tx_isp_device_clk *info)
{
	struct clk **clks = NULL;
	unsigned int num = sd->clk_num;
	int ret = 0;
	int i;

	if(!sd->clk_num){
		sd->clks = NULL;
		return 0;
	}

	clks = (struct clk **)kzalloc(sizeof(clks) * num, GFP_KERNEL);
	if(!clks){
		ISP_ERROR("%s[%d] Failed to allocate core's clks\n",__func__,__LINE__);
		ret = -ENOMEM;
		goto exit;
	}
	for (i = 0; i < num; i++) {
		clks[i] = clk_get(sd->module.dev, info[i].name);
		if (IS_ERR(clks[i])) {
			ISP_ERROR("Failed to get %s clock %ld\n", info[i].name, PTR_ERR(clks[i]));
			ret = PTR_ERR(clks[i]);
			goto failed_to_get_clk;
		}
		if(info[i].rate != DUMMY_CLOCK_RATE) {
			ret = clk_set_rate(clks[i], info[i].rate);
			if(ret){
				ISP_ERROR("Failed to set %s clock rate(%ld)\n", info[i].name, info[i].rate);
				goto failed_to_get_clk;
			}
		}
	}
	sd->clks = clks;
	return 0;
failed_to_get_clk:
	while(--i >= 0){
		clk_put(clks[i]);
	}
	kfree(clks);
exit:
	return ret;
}

static int isp_subdev_release_clks(struct tx_isp_subdev *sd)
{
	struct clk **clks = sd->clks;
	int i = 0;

	if(clks == NULL)
		return 0;
	for (i = 0; i < sd->clk_num; i++)
		clk_put(clks[i]);

	kfree(clks);
	sd->clks = NULL;
	return 0;
}

static int tx_isp_subdev_device_init(struct tx_isp_subdev *sd, struct tx_isp_subdev_descriptor *desc)
{
	int ret = 0;
	int index = 0;
	int count = 0;
	struct tx_isp_subdev_pad *outpads = NULL;
	struct tx_isp_subdev_pad *inpads = NULL;

	/* init clks */
	sd->clk_num = desc->clks_num;
	ret = isp_subdev_init_clks(sd, desc->clks);
	if(ret){
		ISP_ERROR("Failed to init %s's clks!\n", sd->module.name);
		return ret;
	}
	/* init pads */
	for(index = 0; index < desc->pads_num; index++){
		if(desc->pads[index].type == TX_ISP_PADTYPE_OUTPUT)
			sd->num_outpads++;
		else
			sd->num_inpads++;
	}

	/*printk("## %s %d pads = %d inpads = %d, outpads = %d ##\n",__func__,__LINE__, desc->pads_num, sd->num_inpads, sd->num_outpads);*/
	if(sd->num_outpads){
		outpads = (struct tx_isp_subdev_pad *)kzalloc(sizeof(*outpads) * sd->num_outpads, GFP_KERNEL);
		if(!outpads){
			ISP_ERROR("Failed to malloc %s's outpads\n",sd->module.name);
			ret = -ENOMEM;
			goto failed_outpads;
		}

		count = 0;
		for(index = 0; index < desc->pads_num; index++){
			if(desc->pads[index].type == TX_ISP_PADTYPE_OUTPUT){
				outpads[count].sd = sd;
				outpads[count].index = count;
				outpads[count].type = desc->pads[index].type;
				outpads[count].links_type = desc->pads[index].links_type;
				outpads[count].state = TX_ISP_PADSTATE_FREE;
				outpads[count].link.flag = TX_ISP_LINKFLAG_DYNAMIC;
				count++;
			}
		}
	}
	if(sd->num_inpads){
		inpads = (struct tx_isp_subdev_pad *)kzalloc(sizeof(*inpads) * sd->num_inpads, GFP_KERNEL);
		if(!outpads){
			ISP_ERROR("Failed to malloc %s's inpads\n",sd->module.name);
			ret = -ENOMEM;
			goto failed_inpads;
		}

		count = 0;
		for(index = 0; index < desc->pads_num; index++){
			if(desc->pads[index].type == TX_ISP_PADTYPE_INPUT){
				inpads[count].sd = sd;
				inpads[count].index = count;
				inpads[count].type = desc->pads[index].type;
				inpads[count].links_type = desc->pads[index].links_type;
				inpads[count].state = TX_ISP_PADSTATE_FREE;
				inpads[count].link.flag = TX_ISP_LINKFLAG_DYNAMIC;
				count++;
			}
		}
	}
	sd->outpads = outpads;
	sd->inpads = inpads;

	return 0;
failed_inpads:
	kfree(outpads);
failed_outpads:
	isp_subdev_release_clks(sd);
	return ret;
}

static inline int tx_isp_subdev_widget_init(struct tx_isp_subdev *sd, struct tx_isp_widget_descriptor *desc)
{
	/* init clks */
	sd->clk_num = desc->clks_num;
	return isp_subdev_init_clks(sd, desc->clks);
}

int tx_isp_subdev_init(struct platform_device *pdev, struct tx_isp_subdev *sd, struct tx_isp_subdev_ops *ops)
{
	int ret = 0;
	struct resource *res = NULL;
	int index = 0;
	struct tx_isp_device_descriptor *desc = NULL;
	if(!pdev || !sd){
		ISP_ERROR("%s the parameters are invalid!\n", __func__);
		return -EINVAL;
	}
	/* subdev ops */
	sd->ops = ops;

	/*printk("## %s %d ##\n",__func__,__LINE__);*/
	ret = tx_isp_module_init(pdev, &sd->module);
	if(ret){
		ISP_ERROR("Failed to init isp module(%s)\n", pdev->name);
		ret = -ENOMEM;
		goto exit;
	}

	desc = pdev->dev.platform_data;
	if(desc == NULL)
		goto done;

	/* init interrupt */
	ret = tx_isp_request_irq(pdev, &sd->irqdev);
	if(ret){
		ISP_ERROR("Failed to request irq!\n");
		goto failed_irq;
	}

	/**/
	for(index = 0; index < pdev->num_resources; index++){
		res = private_platform_get_resource(pdev, IORESOURCE_MEM, index);
		if(res && !strcmp(res->name, TX_ISP_DEV_NAME))
			break;
		else
			res = NULL;
	}
	if(res){
		res = private_request_mem_region(res->start,
				res->end - res->start + 1, dev_name(&pdev->dev));
		if (!res) {
			ISP_ERROR("%s[%d] Not enough memory for resources\n", __func__,__LINE__);
			ret = -EBUSY;
			goto failed_req;
		}

		sd->base = private_ioremap(res->start, res->end - res->start + 1);
		if (!sd->base) {
			ISP_ERROR("%s[%d] Unable to ioremap registers\n", __func__,__LINE__);
			ret = -ENXIO;
			goto err_ioremap;
		}
	}
	sd->res = res;

	/* init subdev */
	if(desc->type == TX_ISP_TYPE_SUBDEV){
		ret = tx_isp_subdev_device_init(sd, (void*)desc);
	}else if(desc->type == TX_ISP_TYPE_WIDGET){
		ret = tx_isp_subdev_widget_init(sd, (void*)desc);
	}else{
		ISP_INFO("It's header!\n");
	}
	if(ret){
		ISP_ERROR("[%d] Failed to init subdev!\n", __LINE__);
		goto failed_init;
	}
done:
	return 0;
failed_init:
	private_iounmap(sd->base);
err_ioremap:
	private_release_mem_region(res->start, res->end - res->start + 1);
failed_req:
	tx_isp_free_irq(&sd->irqdev);
failed_irq:
	tx_isp_module_deinit(&sd->module);
exit:
	return ret;
}

EXPORT_SYMBOL(tx_isp_subdev_init);

void tx_isp_subdev_deinit(struct tx_isp_subdev *sd)
{
	struct tx_isp_module *module = &sd->module;
	if(module->ops)
		private_misc_deregister(&module->miscdev);

	isp_subdev_release_clks(sd);
	if(sd->outpads)
		kfree(sd->outpads);
	if(sd->inpads)
		kfree(sd->inpads);
	if(sd->base){
		private_iounmap(sd->base);
	}
	if(sd->res){
		private_release_mem_region(sd->res->start, sd->res->end - sd->res->start + 1);
		sd->res = NULL;
	}
	if(sd->irqdev.irq){
		tx_isp_free_irq(&sd->irqdev);
	}
	tx_isp_module_deinit(&sd->module);
	sd->ops = NULL;
}

EXPORT_SYMBOL(tx_isp_subdev_deinit);

int tx_isp_create_graph_and_nodes(struct tx_isp_device* ispdev)
{
	int ret = 0;
	int index = 0;
	struct tx_isp_platform *pdevs = ispdev->pdevs;
	struct tx_isp_module *module = NULL;
	struct tx_isp_module *submodule = NULL;
	struct tx_isp_module *header = &ispdev->module;

	/*printk("%s %d\n", __func__, __LINE__);*/
	/* First, find subdevs */
	for(index = 0; index < ispdev->pdev_num; index++){
		module = private_platform_get_drvdata(pdevs[index].dev);
		/*printk("%s %d module = %p\n", __func__, __LINE__,module);*/
		if(IS_ERR_OR_NULL(module)){
			printk("Can't find the module(%d) driver!\n", index);
			continue;
		}
		if(module->desc.type == TX_ISP_TYPE_SUBDEV){
			header->submods[TX_ISP_GET_ID(module->desc.unitid)] = module;
		}
	}
	/*printk("%s %d\n", __func__, __LINE__);*/
	/* Second, find widgets */
	for(index = 0; index < ispdev->pdev_num; index++){
		module = private_platform_get_drvdata(pdevs[index].dev);
		if(module->desc.type == TX_ISP_TYPE_WIDGET){
			/*printk("%s %d  subdev %d, widget %d\n", __func__, __LINE__, TX_ISP_GET_ID(module->desc.parentid), TX_ISP_GET_ID(module->desc.unitid));*/
			submodule = header->submods[TX_ISP_GET_ID(module->desc.parentid)];
			if(submodule == NULL){
				ISP_ERROR("the module(%d.%d) doesn't have parent!\n",module->desc.parentid, module->desc.unitid);
				ret = -EINVAL;
				break;
			}
			submodule->submods[TX_ISP_GET_ID(module->desc.unitid)] = module;
		}
	}

	/*printk("%s %d\n", __func__, __LINE__);*/
	/* Thrid, create misc device and debug node */
	for(index = 0; index < ispdev->pdev_num; index++){
		module = private_platform_get_drvdata(pdevs[index].dev);
		if(module->ops){
			module->miscdev.minor = MISC_DYNAMIC_MINOR;
			module->miscdev.name = module->name;
			module->miscdev.fops = module->ops;
			ret = private_misc_register(&module->miscdev);
			if (ret < 0) {
				ret = -ENOENT;
				ISP_ERROR("Failed to register tx-isp miscdev(%d.%d)!\n", module->desc.parentid, module->desc.unitid);
				goto failed_misc_register;
			}
		}
		if(module->debug_ops){
			private_proc_create_data(module->name, S_IRUGO, ispdev->proc, module->debug_ops, (void *)module);
		}
	}
	return 0;
failed_misc_register:
	while(--index >= 0){
		module = private_platform_get_drvdata(pdevs[index].dev);
		if(module->ops){
			private_misc_deregister(&module->miscdev);
		}
	}
	return ret;
}

static struct file_operations tx_isp_fops = {
	.open = tx_isp_open,
	.release = tx_isp_release,
	.unlocked_ioctl = tx_isp_unlocked_ioctl,
};

#if 0
static const struct file_operations tx_isp_debug_fops ={
	.read = seq_read,
	.open = motor_info_open,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static int tx_isp_probe(struct platform_device *pdev)
{
	struct tx_isp_device* ispdev = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	struct tx_isp_module *module = NULL;
	int ret = ISP_SUCCESS;

	/*ISP_INFO("@@@@@@@@@@@@@@@@@@@@ tx-isp-probe @@@@@@@@@@@@@@@@@@@@@@@@@@\n");*/
	ispdev = (struct tx_isp_device*)kzalloc(sizeof(*ispdev), GFP_KERNEL);
	if (!ispdev) {
		ISP_ERROR("Failed to allocate camera device\n");
		ret = -ENOMEM;
		goto exit;
	}

	/* register subdev */
	desc = pdev->dev.platform_data;
	ret = tx_isp_match_and_register_platforms(desc, ispdev->pdevs);
	if(ret){
		goto failed_to_match;
	}
	ispdev->pdev_num = desc->entity_num;

	/* init self */
	private_spin_lock_init(&ispdev->slock);
	ret = tx_isp_module_init(pdev, &ispdev->module);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}

	tx_isp_set_module_nodeops(&ispdev->module, &tx_isp_fops);
	/*tx_isp_set_module_debugops(&ispdev->module, &tx_isp_debug_fops);*/
	module = &ispdev->module;

	module->miscdev.minor = MISC_DYNAMIC_MINOR;
	module->miscdev.name = "tx-isp";
	module->miscdev.fops = &tx_isp_fops;
	ret = private_misc_register(&module->miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		ISP_ERROR("Failed to register tx-isp miscdev!\n");
		goto failed_misc_register;
	}

	ispdev->proc = jz_proc_mkdir("isp");
	if (!ispdev->proc) {
		ispdev->proc = NULL;
		ISP_ERROR("Failed to create debug directory of tx-isp!\n");
		goto failed_to_proc;
	}
	private_platform_set_drvdata(pdev, ispdev);
	globe_ispdev = ispdev;

	/* create the topology graph of isp modules */
	ret = tx_isp_create_graph_and_nodes(ispdev);
	if(ret){
		goto failed_to_nodes;
	}

	isp_mem_init();
	/*isp_debug_init();*/
	ispdev->version = TX_ISP_DRIVER_VERSION;
	printk("@@@@ tx-isp-probe ok(version %s) @@@@@\n", ispdev->version);
	return 0;

failed_to_nodes:
	proc_remove(ispdev->proc);
failed_to_proc:
	private_misc_deregister(&module->miscdev);
failed_misc_register:
	tx_isp_module_deinit(&ispdev->module);
failed_to_ispmodule:
	tx_isp_unregister_platforms(ispdev->pdevs);
failed_to_match:
	kfree(ispdev);
exit:
	return ret;

}

static int __exit tx_isp_remove(struct platform_device *pdev)
{
	struct tx_isp_device* ispdev = private_platform_get_drvdata(pdev);
	struct tx_isp_module *module = &ispdev->module;

	/*printk("%s %d\n", __func__, __LINE__);*/

	private_misc_deregister(&module->miscdev);
	proc_remove(ispdev->proc);
	tx_isp_unregister_platforms(ispdev->pdevs);
	platform_set_drvdata(pdev, NULL);
	/*isp_debug_deinit();*/

	kfree(ispdev);

	return 0;
}

#ifdef CONFIG_PM
static int tx_isp_suspend(struct device *dev)
{
//	tx_isp_device_t* ispdev = dev_get_drvdata(dev);

//	isp_dev_call(ispdev->isp, suspend, NULL);

	return 0;
}

static int tx_isp_resume(struct device *dev)
{
//	tx_isp_device_t* ispdev = dev_get_drvdata(dev);

//	isp_dev_call(ispdev->isp, resume, NULL);

	return 0;
}

static struct dev_pm_ops tx_isp_pm_ops = {
	.suspend = tx_isp_suspend,
	.resume = tx_isp_resume,
};
#endif

static struct platform_driver tx_isp_driver = {
	.probe = tx_isp_probe,
	.remove = __exit_p(tx_isp_remove),
	.driver = {
		.name = "tx-isp",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &tx_isp_pm_ops,
#endif
	},
};

static int __init tx_isp_init(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		printk("Failed to insmod isp driver!\n");
		return ret;
	}
	ret = private_platform_device_register(&tx_isp_platform_device);
	if(ret){
		printk("Failed to insmod isp driver!!!\n");
		return ret;
	}

	ret = private_platform_driver_register(&tx_isp_driver);
	if(ret){
		private_platform_device_unregister(&tx_isp_platform_device);
	}
	return ret;
}

static void __exit tx_isp_exit(void)
{
	private_platform_driver_unregister(&tx_isp_driver);
	private_platform_device_unregister(&tx_isp_platform_device);
}

module_init(tx_isp_init);
module_exit(tx_isp_exit);

MODULE_AUTHOR("Ingenic xhshen");
MODULE_DESCRIPTION("tx isp driver");
MODULE_LICENSE("GPL");
/*MODULE_LICENSE("Proprietary");*/
