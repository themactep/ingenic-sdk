/*
 * Video Class definitions of Tomahawk series SoC.
 *
 * Copyright 2017, <xianghui.shen@ingenic.com>
 *
 * This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/init.h>
#include <linux/delay.h>

#include "tx-isp-video-in.h"
/*
 * Sensor subdevice helper functions
 */

/* Load an i2c sub-device. */
struct tx_isp_subdev *isp_i2c_new_subdev_board(struct i2c_adapter *adapter, struct i2c_board_info *info,
		const unsigned short *probe_addrs)
{
	struct tx_isp_subdev *sd = NULL;
	struct i2c_client *client;

#if 0
	private_request_module(true, I2C_MODULE_PREFIX "%s", info->type);
#else
	private_request_module(I2C_MODULE_PREFIX "%s", info->type);
#endif
	/* Create the i2c client */
	if (info->addr == 0)
		return NULL;
	else
		client = private_i2c_new_device(adapter, info);

	/* Note: by loading the module first we are certain that c->driver
	   will be set if the driver was found. If the module was not loaded
	   first, then the i2c core tries to delay-load the module for us,
	   and then c->driver is still NULL until the module is finally
	   loaded. This delay-load mechanism doesn't work if other drivers
	   want to use the i2c device, so explicitly loading the module
	   is the best alternative. */
	if (client == NULL || client->driver == NULL)
		goto error;

	/* Lock the module so we can safely get the tx_isp_subdev pointer */
	if (!private_try_module_get(client->driver->driver.owner))
		goto error;
	sd = private_i2c_get_clientdata(client);

	/* Decrease the module use count to match the first try_module_get. */
	private_module_put(client->driver->driver.owner);

error:
	/* If we have a client but no subdev, then something went wrong and
	   we must unregister the client. */
	if (client && sd == NULL)
		private_i2c_unregister_device(client);
	return sd;
}


static long subdev_sensor_ops_register_sensor(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_vin_device *vi = !sd ? NULL : sd_to_vin_device(sd);
	struct tx_isp_sensor_register_info *info = arg;
	struct tx_isp_subdev *sensor_sd = NULL;
	struct tx_isp_sensor *sensor = NULL;
	int ret = ISP_SUCCESS;

	if (!vi || !info)
		return -EINVAL;
	if(vi->state == TX_ISP_MODULE_SLAKE){
		ISP_ERROR("the devnode does't have been opened.\n");
		return -EPERM;
	}

	if(info->cbus_type == TX_SENSOR_CONTROL_INTERFACE_I2C){
		struct i2c_adapter *adapter;
		struct i2c_board_info board_info;
		adapter = private_i2c_get_adapter(info->i2c.i2c_adapter_id);
		if (!adapter) {
			ISP_ERROR("Failed to get I2C adapter %d, deferring probe\n",
					info->i2c.i2c_adapter_id);
			return -EINVAL;
		}
		memset(&board_info, 0 , sizeof(board_info));
		memcpy(&board_info.type, &info->i2c.type, I2C_NAME_SIZE);
		board_info.addr = info->i2c.addr;
		sensor_sd = isp_i2c_new_subdev_board(adapter, &board_info, NULL);
		if (IS_ERR_OR_NULL(sensor_sd)) {
			private_i2c_put_adapter(adapter);
			ISP_ERROR("Failed to acquire subdev %s, deferring probe\n",
					info->i2c.type);
			return -EINVAL;
		}
	}else if (info->cbus_type == TX_SENSOR_CONTROL_INTERFACE_SPI){
	}else{
		ISP_WRANING("%s[%d] the type of sensor SBUS hasn't been defined.\n",__func__,__LINE__);
		return -EINVAL;
	}
	/* add private data of sensor */
	sensor = tx_isp_get_subdev_hostdata(sensor_sd);
	sensor->info = *info;

	ret = tx_isp_subdev_call(sensor_sd, core, g_chip_ident, &sensor_sd->chip);
	if(ret != ISP_SUCCESS){
		if(info->cbus_type == TX_SENSOR_CONTROL_INTERFACE_I2C){
			struct i2c_client *client = tx_isp_get_subdevdata(sensor_sd);
			struct i2c_adapter *adapter = client->adapter;
			if (adapter)
				private_i2c_put_adapter(adapter);
				private_i2c_unregister_device(client);
		}else{
		}
		tx_isp_subdev_deinit(sensor_sd);
		return -EINVAL;
	}

	private_mutex_lock(&vi->mlock);
	list_add_tail(&sensor->list, &vi->sensors);
	private_mutex_unlock(&vi->mlock);

	/* set the parent of sensor subdev */
	sensor_sd->module.parent = sd->module.parent;

	ISP_INFO("Registered sensor subdevice %s\n", sensor_sd->module.name);
	return ISP_SUCCESS;
}

static long subdev_sensor_ops_release_sensor(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_vin_device *vi = !sd ? NULL : sd_to_vin_device(sd);
	struct tx_isp_sensor_register_info *info = arg;
	struct tx_isp_sensor *sensor = NULL;
	struct tx_isp_subdev *sensor_sd = NULL;

	if (!vi || !info)
		return -EINVAL;
	if(vi->state == TX_ISP_MODULE_SLAKE){
		ISP_ERROR("the devnode does't have been opened.\n");
		return -EPERM;
	}

	private_mutex_lock(&vi->mlock);
	list_for_each_entry(sensor, &vi->sensors, list) {
		if(!strcmp(sensor->info.name, info->name)){
			sensor_sd = &sensor->sd;
			break;
		}
	}
	/* when can't find the matching sensor, do nothing!*/
	if(!sensor_sd){
		private_mutex_unlock(&vi->mlock);
		return ISP_SUCCESS;
	}
	/* when the sensor is active, please stop it firstly.*/
	if(sensor == vi->active){
		private_mutex_unlock(&vi->mlock);
		ISP_WRANING("the sensor is active, please stop it firstly.\n");
		return -EINVAL;
	}

	list_del(&sensor->list);
	private_mutex_unlock(&vi->mlock);

	if(sensor->info.cbus_type == TX_SENSOR_CONTROL_INTERFACE_I2C){
		struct i2c_client *client = tx_isp_get_subdevdata(sensor_sd);
		struct i2c_adapter *adapter = client->adapter;
		if (adapter)
			private_i2c_put_adapter(adapter);
		private_i2c_unregister_device(client);

	}else if (sensor->info.cbus_type == TX_SENSOR_CONTROL_INTERFACE_SPI){
	}else{
		ISP_WRANING("%s[%d] the type of sensor SBUS hasn't been defined.\n",__func__,__LINE__);
		return -EINVAL;
	}
	return ISP_SUCCESS;
}

static int subdev_sensor_ops_release_all_sensor(struct tx_isp_subdev *sd)
{
	struct tx_isp_vin_device *vi = !sd ? NULL : sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = NULL;

	if (!vi)
		return -EINVAL;
	if(vi->state == TX_ISP_MODULE_SLAKE){
		ISP_ERROR("the devnode does't have been opened.\n");
		return -EPERM;
	}

	while(!list_empty(&vi->sensors)){
		sensor = list_first_entry(&vi->sensors, struct tx_isp_sensor, list);
		list_del(&sensor->list);
		if(sensor->info.cbus_type == TX_SENSOR_CONTROL_INTERFACE_I2C){
			struct i2c_client *client = tx_isp_get_subdevdata(&sensor->sd);
			struct i2c_adapter *adapter = client->adapter;
			if (adapter)
				private_i2c_put_adapter(adapter);
			private_i2c_unregister_device(client);

		}else if (sensor->info.cbus_type == TX_SENSOR_CONTROL_INTERFACE_SPI){
		}else{
			ISP_ERROR("%s[%d] the type of sensor SBUS hasn't been defined.\n",__func__,__LINE__);
			return -EINVAL;
		}
	}
	return 0;
}

/*
* 每次枚举都需要对vi->sensors链表中的sensor的index进行编号。
*/
long subdev_sensor_ops_enum_input(struct tx_isp_subdev *sd, void *arg)
{
	struct tx_isp_vin_device *vi = !sd ? NULL : sd_to_vin_device(sd);
	struct v4l2_input *input = (struct v4l2_input *)arg;
	struct tx_isp_sensor *sensor = NULL;
	int i = 0;

	if (!vi || !input){
		return -EINVAL;
	}

	private_mutex_lock(&vi->mlock);
	list_for_each_entry(sensor, &vi->sensors, list) {
		sensor->index = i++;
		if(sensor->index == input->index){
			input->type = sensor->type;
			strncpy(input->name, sensor->info.name, sizeof(sensor->info.name));
			break;
		}
	}
	private_mutex_unlock(&vi->mlock);
	if(sensor->index != input->index)
		return -EINVAL;

	return 0;
}
/*
* 返回vi->active sensor 中的index编号
*/
static long subdev_sensor_ops_get_input(struct tx_isp_subdev *sd, int *index)
{
	struct tx_isp_vin_device *vi = !sd ? NULL : sd_to_vin_device(sd);
	if (!vi || !index)
		return -EINVAL;
	*index = IS_ERR_OR_NULL(vi->active) ? -1 : vi->active->index;
	return 0;
}
/*
*
*/
static long subdev_sensor_ops_set_input(struct tx_isp_subdev *sd, int *index)
{
	struct tx_isp_vin_device *vi = !sd ? NULL : sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = NULL;
	int enable = 0;
	int ret = ISP_SUCCESS;
	if (!vi || !index)
		return -EINVAL;

	/*firstly, Determine whether the point to the same sensor */
	if(vi->active && *index == vi->active->index)
		return ISP_SUCCESS;

	sensor = vi->active;

	/* secondly, streamoff, deinit and unprepare previous sensor. */
	if(sensor){
		if(vi->state == TX_ISP_MODULE_RUNNING){
			ISP_WRANING("Please, streamoff sensor firstly!\n");
			return -EPERM;
		}

		enable = 0;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SUBDEV_INIT, &enable);
		if(ret != ISP_SUCCESS){
			ISP_WRANING("Failed to deinit the pipeline of %s.\n", sensor->attr.name);
			goto err_exit;
		}

		vi->active = NULL;

		sd = &sensor->sd;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, NULL);
		if(ret != ISP_SUCCESS)
			goto err_exit;
	}

	/* third, s_input NULL .*/
	if(-1 == *index){
		return ISP_SUCCESS;
	}
	/* fourth, find a corresponding sensor.*/
	private_mutex_lock(&vi->mlock);
	list_for_each_entry(sensor, &vi->sensors, list) {
		if(sensor->index == *index){
			break;
		}
	}
	private_mutex_unlock(&vi->mlock);

	/* if the index isn't find in the list of sensors that has been registered.*/
	if(sensor->index != *index){
		ISP_ERROR("Failed to the set input sensor(%d) that .\n", *index);
		return -EINVAL;
	}

	/*lastly, prepare, init and streamon active sensor */
	vi->active = sensor;

	sd = &sensor->sd;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if(ret != ISP_SUCCESS)
		goto err_exit;
	enable = 1;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SUBDEV_INIT, &enable);
	if(ret != ISP_SUCCESS){
		ISP_WRANING("Failed to init the pipeline of %s.\n", sensor->attr.name);
		goto err_init;
	}

	*index = ((sensor->video.vi_max_width & 0xffff) << 16) | (sensor->video.vi_max_height & 0xffff);

	return ISP_SUCCESS;
err_init:
err_exit:
	return ret;
}

static int tx_isp_vin_init(struct tx_isp_subdev *sd, int on)
{
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = vi->active;
	int ret = 0;
	/*printk("## %s %d on=%d ##\n", __func__, __LINE__, on);*/
	if(sensor){
		ret = tx_isp_subdev_call(&sensor->sd, core, init, on);
	}else{
		ISP_WRANING("[%d] Don't have active sensor!\n",__LINE__);
		ret = -EPERM;
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;
	if(on){
		vi->state = TX_ISP_MODULE_INIT;
	}else{
		vi->state = TX_ISP_MODULE_DEINIT;
	}
	return ret;
}

static int tx_isp_vin_reset(struct tx_isp_subdev *sd, int on)
{
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = vi->active;
	int ret = 0;
	if(sensor){
		ret = tx_isp_subdev_call(&sensor->sd, core, reset, on);
	}else{
		ISP_WRANING("[%d] Don't have active sensor!\n",__LINE__);
		ret = -EPERM;
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;
	return ret;
}

static int vic_core_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = vi->active;

	/* This module operation */
	switch(cmd){
		case TX_ISP_EVENT_SUBDEV_INIT:
			if(sensor)
				ret = tx_isp_subdev_call(&sensor->sd, core, init, *(int*)arg);
			break;
		default:
			break;
	}

	ret = ret == -ENOIOCTLCMD ? 0 : ret;

	return ret;
}

static int tx_isp_vin_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	int ret = 0;
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = vi->active;
	if(sensor){
		ret = tx_isp_subdev_call(&sensor->sd, core, g_register, reg);
	}else{
		ISP_WRANING("[%d] Don't have active sensor!\n",__LINE__);
		ret = -EPERM;
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;
	return ret;
}

static int tx_isp_vin_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = vi->active;
	int ret = 0;
	if(sensor){
		ret = tx_isp_subdev_call(&sensor->sd, core, s_register, reg);
	}else{
		ISP_WRANING("[%d] Don't have active sensor!\n",__LINE__);
		ret = -EPERM;
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;
	return ret;
}

static int vin_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = vi->active;

	/* streamon */
	if(enable && vi->state == TX_ISP_MODULE_RUNNING){
		return ISP_SUCCESS;
	}

	/* streamoff */
	if(!enable && vi->state != TX_ISP_MODULE_RUNNING){
		return ISP_SUCCESS;
	}

	/*printk("## %s %d sensor = %p enable = %d ##\n", __func__,__LINE__,sensor, enable);*/
	if(sensor){
		ret = tx_isp_subdev_call(&sensor->sd, video, s_stream, enable);
	}
	if(!ret){
		if(enable)
			vi->state = TX_ISP_MODULE_RUNNING;
		else
			vi->state = TX_ISP_MODULE_INIT;
	}
	return ret;
}

static int tx_isp_vin_activate_subdev(struct tx_isp_subdev *sd)
{
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	private_mutex_lock(&vi->mlock);
	if(vi->state == TX_ISP_MODULE_SLAKE){
		vi->state = TX_ISP_MODULE_ACTIVATE;
	}
	private_mutex_unlock(&vi->mlock);
	vi->refcnt++;
	return 0;
}

static int tx_isp_vin_slake_subdev(struct tx_isp_subdev *sd)
{
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	int index;

	if(vi->refcnt > 0)
		vi->refcnt--;

	if(!vi->refcnt){
		if (vi->state == TX_ISP_MODULE_RUNNING){
			vin_s_stream(sd, 0);
		}
		if(vi->state == TX_ISP_MODULE_INIT){
			tx_isp_vin_init(sd, 0);
		}
		if(vi->active){
			index = -1;
			subdev_sensor_ops_set_input(sd, &index);
		}
		if(!list_empty(&vi->sensors)){
			subdev_sensor_ops_release_all_sensor(sd);
		}
		private_mutex_lock(&vi->mlock);
		if(vi->state == TX_ISP_MODULE_ACTIVATE)
			vi->state = TX_ISP_MODULE_SLAKE;
		private_mutex_unlock(&vi->mlock);
	}
	return 0;
}

static struct tx_isp_subdev_internal_ops vin_subdev_internal_ops ={
	.activate_module = tx_isp_vin_activate_subdev,
	.slake_module = tx_isp_vin_slake_subdev,
};

static struct tx_isp_subdev_core_ops vin_subdev_core_ops ={
	.init = tx_isp_vin_init,
	.reset = tx_isp_vin_reset,
	.ioctl = vic_core_ops_ioctl,
	/*.g_register = tx_isp_vin_g_register,*/
	/*.s_register = tx_isp_vin_s_register,*/
};

static struct tx_isp_subdev_video_ops vin_subdev_video_ops = {
	.s_stream = vin_s_stream,
};

static int subdev_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	struct tx_isp_vin_device *vi = sd_to_vin_device(sd);
	struct tx_isp_sensor *sensor = vi->active;
	int ret = 0;

	switch(cmd){
		case TX_ISP_EVENT_SENSOR_ENUM_INPUT:
			ret = subdev_sensor_ops_enum_input(sd, arg);
			break;
		case TX_ISP_EVENT_SENSOR_GET_INPUT:
			ret = subdev_sensor_ops_get_input(sd, arg);
			break;
		case TX_ISP_EVENT_SENSOR_SET_INPUT:
			ret = subdev_sensor_ops_set_input(sd, arg);
			break;
		case TX_ISP_EVENT_SENSOR_REGISTER:
			ret = subdev_sensor_ops_register_sensor(sd, arg);
			break;
		case TX_ISP_EVENT_SENSOR_RELEASE:
			ret = subdev_sensor_ops_release_sensor(sd, arg);
			break;
		case TX_ISP_EVENT_SENSOR_S_REGISTER:
			ret = tx_isp_vin_s_register(sd, arg);
			break;
		case TX_ISP_EVENT_SENSOR_G_REGISTER:
			ret = tx_isp_vin_g_register(sd, arg);
			break;
		default:
			if(sensor){
				ret = tx_isp_subdev_call(&sensor->sd, sensor, ioctl, cmd, arg);
			}else{
				ISP_WRANING("[%d] Don't have active sensor!\n",__LINE__);
				ret = -EPERM;
			}
		break;
	}
	ret = ret == -ENOIOCTLCMD ? 0 : ret;
	return ret;
}

static struct tx_isp_subdev_sensor_ops	vin_subdev_sensor_ops = {
	.release_all_sensor = subdev_sensor_ops_release_all_sensor,
	.ioctl	= subdev_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops vin_subdev_ops = {
	.core = &vin_subdev_core_ops,
	.video = &vin_subdev_video_ops,
	.sensor = &vin_subdev_sensor_ops,
	.internal = &vin_subdev_internal_ops,
};


/* debug sensor registers */
#define VIDEOIN_CMD_BUF_SIZE 128
static uint8_t video_input_cmd_buf[VIDEOIN_CMD_BUF_SIZE];
static int video_input_cmd_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_vin_device *vi = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdev_hostdata(sd);

	if(vi->state < TX_ISP_MODULE_RUNNING){
		len += seq_printf(m ,"sensor doesn't work, please enable sensor\n");
		return len;
	}

	len += seq_printf(m ,"%s\n", video_input_cmd_buf);
	return len;
}

static ssize_t video_input_cmd_set(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos)
{
	int ret = 0;
	int len = 0;
	struct seq_file *m = file->private_data;
	struct tx_isp_module *module = (void *)(m->private);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_vin_device *vi = IS_ERR_OR_NULL(sd) ? NULL : tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor *sensor = IS_ERR_OR_NULL(vi)? NULL : vi->active;
	struct tx_isp_dbg_register dbg;
	char *buf = NULL;

	if(IS_ERR_OR_NULL(sensor)){
		len += seq_printf(m ,"don't have active sensor, please set sensor firstly!\n");
		return len;
	}

	if(count > VIDEOIN_CMD_BUF_SIZE){
		buf = kzalloc((count+1), GFP_KERNEL);
		if(!buf)
			return -ENOMEM;
	}else
		buf = video_input_cmd_buf;

	if(copy_from_user(buf, buffer, count)){
		if(count > VIDEOIN_CMD_BUF_SIZE)
			kfree(buf);
		return -EFAULT;
	}
	//printk("##### %s\n", buf);
	if (!strncmp(buf, "r sen_reg", sizeof("r sen_reg")-1)) {
		unsigned reg = 0;
		unsigned val = 0;
		reg = simple_strtoull(buf+sizeof("r sen_reg"), NULL, 0);

		dbg.name = sensor->sd.chip.name;
		dbg.reg = reg;
		ret = tx_isp_subdev_call(&sensor->sd, core, g_register, &dbg);
		if (ret)
			ISP_ERROR("##### err %s.%d\n", __func__, __LINE__);
		val = (unsigned int)dbg.val;
		seq_printf(m ,"isp: sensor reg read 0x%x(0x%x)\n", reg, val);
		sprintf(video_input_cmd_buf, "0x%x\n", val);
	} else if (!strncmp(buf, "w sen_reg", sizeof("w sen_reg")-1)) {
		unsigned reg = 0;
		unsigned val = 0;
		char *p = 0;
		reg = simple_strtoull(buf+sizeof("w sen_reg"), &p, 0);
		val = simple_strtoull(p+1, NULL, 0);
		seq_printf(m ,"isp: sensor reg write 0x%x(0x%x)\n", reg, val);
		dbg.name = sensor->sd.chip.name;
		dbg.reg = reg;
		dbg.val = val;
		ret = tx_isp_subdev_call(&sensor->sd, core, s_register, &dbg);
		if (!ret)
			sprintf(video_input_cmd_buf, "%s\n", "ok");
		else
			sprintf(video_input_cmd_buf, "%s\n", "failed");
	} else {
		sprintf(video_input_cmd_buf, "null");
	}
	if(count > VIDEOIN_CMD_BUF_SIZE)
		kfree(buf);
	return count;
}

static int video_input_cmd_open(struct inode *inode, struct file *file)
{
	return private_single_open_size(file, video_input_cmd_show, PDE_DATA(inode), 512);
}

static struct file_operations video_input_cmd_fops ={
	.read = private_seq_read,
	.open = video_input_cmd_open,
	.llseek = private_seq_lseek,
	.release = private_single_release,
	.write = video_input_cmd_set,
};

static int tx_isp_vin_probe(struct platform_device *pdev)
{
	struct tx_isp_vin_device *vin = NULL;
	struct tx_isp_subdev *sd = NULL;
	struct tx_isp_device_descriptor *desc = NULL;
	int ret = 0;

	/*printk("@@@@@@@@@@@@@@@@@@@@ tx-isp-video-in probe @@@@@@@@@@@@@@@@@@@@@@@@@@\n");*/
	vin = (struct tx_isp_vin_device *)kzalloc(sizeof(*vin), GFP_KERNEL);
	if(!vin){
		ISP_ERROR("Failed to allocate sensor subdev\n");
		ret = -ENOMEM;
		goto exit;
	}

	private_mutex_init(&vin->mlock);
	INIT_LIST_HEAD(&vin->sensors);
	vin->refcnt = 0;
	vin->active = NULL;

	desc = pdev->dev.platform_data;
	sd = &vin->sd;
	ret = tx_isp_subdev_init(pdev, sd, &vin_subdev_ops);
	if(ret){
		ISP_ERROR("Failed to init isp module(%d.%d)\n", desc->parentid, desc->unitid);
		ret = -ENOMEM;
		goto failed_to_ispmodule;
	}
	tx_isp_set_subdev_hostdata(sd, vin);
	private_platform_set_drvdata(pdev, &sd->module);
	tx_isp_set_subdev_debugops(sd, &video_input_cmd_fops);

	/* set state of the subdev */
	vin->state = TX_ISP_MODULE_SLAKE;
	return ISP_SUCCESS;

failed_to_ispmodule:
	kfree(vin);
exit:
	return ret;
}

static int __exit tx_isp_vin_remove(struct platform_device *pdev)
{
	struct tx_isp_module *module = private_platform_get_drvdata(pdev);
	struct tx_isp_subdev *sd = IS_ERR_OR_NULL(module) ? NULL : module_to_subdev(module);
	struct tx_isp_vin_device *vi = IS_ERR_OR_NULL(sd) ? NULL : sd_to_vin_device(sd);
	private_platform_set_drvdata(pdev, NULL);
	tx_isp_subdev_deinit(sd);
	kfree(vi);
	return 0;
}

struct platform_driver tx_isp_vin_driver = {
	.probe = tx_isp_vin_probe,
	.remove = __exit_p(tx_isp_vin_remove),
	.driver = {
		.name = TX_ISP_VIN_NAME,
		.owner = THIS_MODULE,
	},
};
