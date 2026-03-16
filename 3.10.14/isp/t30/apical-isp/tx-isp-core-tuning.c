#include <linux/videodev2.h>
#include <linux/delay.h>
#include "apical_command_api.h"
#include <apical-isp/apical_isp_config.h>
#include <apical-isp/apical_math.h>
#include "tx-isp-core-tuning.h"

/** the kernel command line whether the mem of WDR and Temper exist. **/
extern unsigned long ispmem_base;
extern unsigned long ispmem_size;
extern system_tab stab;

static inline int wb_value_v4l2_to_apical(int val)
{
	int ret = 0;
	switch(val){
	case V4L2_WHITE_BALANCE_MANUAL:
		ret = AWB_MANUAL;
		break;
	case V4L2_WHITE_BALANCE_AUTO:
		ret = AWB_AUTO;
		break;
	case V4L2_WHITE_BALANCE_INCANDESCENT:
		ret = AWB_INCANDESCENT;
		break;
	case V4L2_WHITE_BALANCE_FLUORESCENT:
		ret = AWB_FLOURESCENT;
		break;
	case V4L2_WHITE_BALANCE_FLUORESCENT_H:
		ret = AWB_WARM_FLOURESCENT;
		break;
	case V4L2_WHITE_BALANCE_HORIZON:
		ret = AWB_TWILIGHT;
		break;
	case V4L2_WHITE_BALANCE_DAYLIGHT:
		ret = AWB_DAY_LIGHT;
		break;
	case V4L2_WHITE_BALANCE_CLOUDY:
		ret = AWB_CLOUDY;
		break;
	case V4L2_WHITE_BALANCE_SHADE:
		ret = AWB_SHADE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int apical_isp_wb_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	struct isp_core_awb_attr *attr = &tuning->ctrls.awb_attr;
	struct isp_core_mwb_attr *mattr = &tuning->ctrls.mwb_attr;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;
	switch(control->id){
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
		api.id = AWB_MODE_ID;
		api.value = wb_value_v4l2_to_apical(control->value);
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		tuning->ctrls.wb_mode = control->value;
		break;
	case IMAGE_TUNING_CID_AWB_ATTR:
		copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
		/* sets the lowest color temperature that the AWB algorithm can select */
		api.id = AWB_RANGE_LOW_ID;
		api.value = attr->low_color_temp / 100;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		/* sets the highest color temperature that the AWB algorithm can select */
		api.id = AWB_RANGE_HIGH_ID;
		api.value = attr->high_color_temp / 100;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		/*
		 * select which zones are used to gather AWB statistics.
		 * the region of interest is defined as rectangle with top-left coordinates(startx, starty)
		 * and bottom-right coordinates(endx, endy).
		 */
		api.id = AWB_ROI_ID;
		api.value = attr->zone_sel.val;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
#if 0
		/* config the weight of every zone  */
		for(i = 0; i < WEIGHT_ZONE_NUM; i++)
#endif
			break;
	case IMAGE_TUNING_CID_MWB_ATTR:
		copy_from_user(mattr, (const void __user *)control->value, sizeof(*mattr));
		if(tuning->ctrls.wb_mode == V4L2_WHITE_BALANCE_MANUAL){
			api.id = AWB_RGAIN_ID;
			api.value = mattr->red_gain;
			status = apical_command(api.type, api.id, api.value, api.dir, &reason);
			api.id = AWB_BGAIN_ID;
			api.value = mattr->blue_gain;
			status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

#if 0
static inline int ae_value_v4l2_to_apical(int val)
{
	int ret = 0;
	switch(val){
	case V4L2_EXPOSURE_AUTO:
		ret = AE_AUTO;
		break;
	case V4L2_EXPOSURE_MANUAL:
		ret = AE_FULL_MANUAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
static int apical_isp_ae_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	struct isp_core_ae_attr *attr = &tuning->ctrls.ae_attr;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;

	switch(control->id){
	case IMAGE_TUNING_CID_AE_ATTR:
		copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
		api.id = AE_ROI_ID;
		api.value = attr->zone_sel.val;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
#if 0
		/* config the weight of every zone  */
		for(i = 0; i < WEIGHT_ZONE_NUM; i++)
#endif
			break;
	default:
		ret = -EINVAL;
		break;
	}
	return 0;
}
#endif

static int apical_isp_ae_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	struct isp_core_ae_attr *attr = &tuning->ctrls.ae_attr;
	unsigned char status = 0;
	int reason = 0;

	api.type = TALGORITHMS;
	api.dir = COMMAND_GET;

	/* get exposure gain */
	api.id = AE_GAIN_ID;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	attr->gain = api.value;
	/* get exposure compensation */
	api.id = AE_COMPENSATION_ID;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	attr->comp = api.value;
	/* get exposure ROI */
	api.id = AE_ROI_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	attr->zone_sel.val = api.value;
	copy_to_user((void __user *)control->value, (const void *)attr, sizeof(*attr));
#if 0
	/* config the weight of every zone  */
	for(i = 0; i < WEIGHT_ZONE_NUM; i++)
#endif
		return 0;
}

/* the format of return value is 8.8 */
static inline int apical_isp_g_totalgain(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	/* input format 27.5, output format 24.8 */
	unsigned int total_gain;
	total_gain =  stab.global_sensor_analog_gain  + stab.global_sensor_digital_gain + stab.global_isp_digital_gain;

	total_gain = math_exp2(total_gain, 5, 8);

	control->value = total_gain;
	return ISP_SUCCESS;
}

static inline int apical_isp_vflip_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	int state = 0;
	int ret = ISP_SUCCESS;

	tuning->ctrls.vflip = control->value;
	if(core->vflip_state != tuning->ctrls.vflip){
		core->vflip_state = tuning->ctrls.vflip;
		core->vflip_change = 1;
	}
	state = core->vflip_state;
	ret = tx_isp_subdev_call(sd, sensor, ioctl, TX_ISP_EVENT_SENSOR_VFLIP, &state);
	return ret;
}

static inline int apical_isp_hflip_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	int ret = ISP_SUCCESS;

	tuning->ctrls.hflip = control->value;
	if(core->hflip_state != tuning->ctrls.hflip){
		core->hflip_state = tuning->ctrls.hflip;
		core->hflip_change = 1;
	}
	return ret;
}

static inline int apical_isp_hvflip_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	int state = 0;
	int ret = ISP_SUCCESS;
	uint32_t value = control->value;
	tuning->ctrls.vflip = value&0xffff;
	if(core->vflip_state != tuning->ctrls.vflip){
		core->vflip_state = tuning->ctrls.vflip;
		core->vflip_change = 1;
	}
	state = core->vflip_state;
	ret = tx_isp_subdev_call(sd, sensor, ioctl, TX_ISP_EVENT_SENSOR_VFLIP, &state);
	tuning->ctrls.hflip = (value>>16)&0xffff;
	if(core->hflip_state != tuning->ctrls.hflip){
		core->hflip_state = tuning->ctrls.hflip;
		core->hflip_change = 1;
	}
	return ret;
}


static int apical_isp_sinter_dns_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_sinter_attr *attr = &(tuning->ctrls.sinter_attr);
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	copy_from_user(attr, (const void __user*)control->value, sizeof(*attr));

	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;
	api.id = SINTER_MODE_ID;
	api.value = attr->type == ISPCORE_MODULE_AUTO ? AUTO : MANUAL;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);

	if(attr->type == ISPCORE_MODULE_MANUAL){
		api.id = SINTER_STRENGTH_ID;
		api.value = attr->manual_strength;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	}
	if(status != ISP_SUCCESS)
		ret = -ENOENT;
	return ret;
}

static int apical_isp_temper_dns_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	struct isp_core_input_control *contrl = &core->contrl;
	unsigned int temper = 0;
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	tuning->ctrls.temper = control->value;
	temper = tuning->ctrls.temper;

	api.type = TSYSTEM;
	api.dir = COMMAND_SET;
	api.id = SYSTEM_MANUAL_TEMPER;
	switch(temper){
	case ISPCORE_TEMPER_MODE_DISABLE:
		api.value = OFF;
		break;
	case ISPCORE_TEMPER_MODE_AUTO:
	case ISPCORE_TEMPER_MODE_MANUAL:
#if 0
		if(tuning->temper_paddr == 0){
			return -EPERM;
		}
#endif
		apical_isp_temper_temper2_mode_write(1);
		apical_isp_temper_frame_buffer_active_width_write(contrl->inwidth);
		apical_isp_temper_frame_buffer_active_height_write(contrl->inheight);
		apical_isp_temper_frame_buffer_line_offset_write(contrl->inwidth * 4);
		api.value = temper == ISPCORE_TEMPER_MODE_AUTO ? 0 : 1;
		break;
	default:
		ret = -ENOENT;
		break;
	}
	if(ret != ISP_SUCCESS)
		return ret;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if(status != ISP_SUCCESS){
		printk("##[%s %d] failed to temper set command(status = %d)\n", __func__, __LINE__,status);
		ret = -ENOENT;
	}
	/* enable the module mean that disable bypass function */
	apical_isp_temper_frame_buffer_axi_port_enable_write(1);
	apical_isp_top_bypass_temper_write(temper?0:1);
	apical_isp_temper_enable_write(temper?1:0);
	//	printk("##[%s %d] width = %d height = %d \n", __func__, __LINE__,APICAL_READ_32(0xa10),APICAL_READ_32(0xa14));
	//	printk("##[%s %d] bank0 = 0x%08x bank1 = 0x%08x \n", __func__, __LINE__,APICAL_READ_32(0xa18),APICAL_READ_32(0xa1c));
	//	printk("##[%s %d] axi port = 0x%08x temper enable = 0x%08x \n", __func__, __LINE__,APICAL_READ_32(0xa28),APICAL_READ_32(0x2c0));
	//	printk("##[%s %d] 0xa00 = 0x%08x 0xa24 = 0x%08x \n", __func__, __LINE__,APICAL_READ_32(0xa00),APICAL_READ_32(0xa24));

	return ISP_SUCCESS;
}

static int apical_isp_temper_dns_s_strength(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_temper_attr *attr = &(tuning->ctrls.temper_attr);
	unsigned int temper = tuning->ctrls.temper;
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	return 0;
	attr->manual_strength = control->value;
	if(temper == ISPCORE_TEMPER_MODE_MANUAL){
		api.type = TSYSTEM;
		api.dir = COMMAND_SET;
		api.id = SYSTEM_TEMPER_THRESHOLD_TARGET;
		api.value = attr->manual_strength;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	}
	if(status != ISP_SUCCESS)
		ret = -ENOENT;
	return ret;
}

static int apical_isp_temper_dns_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct isp_core_temper_attr *attr = &(tuning->ctrls.temper_attr);
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	struct isp_core_input_control *contrl = &core->contrl;
	unsigned int temper = 0;
	int ret = ISP_SUCCESS;

	copy_from_user(attr, (const void __user*)control->value, sizeof(*attr));
	tuning->ctrls.temper = attr->mode;
	temper = tuning->ctrls.temper;


	switch(temper){
	case ISPCORE_TEMPER_MODE_DISABLE:
		break;
	case ISPCORE_TEMPER_MODE_MANUAL:
		if(attr->manual_strength < 0 || attr->manual_strength > 255){
			printk("%s:%d: Temper strenght is overflow!!!\n",__func__,__LINE__);
			return -1;
		}
		apical_isp_temper_temper2_mode_write(1);
		apical_isp_temper_frame_buffer_active_width_write(contrl->inwidth);
		apical_isp_temper_frame_buffer_active_height_write(contrl->inheight);
		apical_isp_temper_frame_buffer_line_offset_write(contrl->inwidth * 4);

		stab.global_manual_temper = 1;
		if (attr->manual_strength > stab.global_maximum_temper_strength)
			stab.global_maximum_temper_strength = attr->manual_strength;
		if (attr->manual_strength < stab.global_minimum_temper_strength)
			stab.global_minimum_temper_strength = attr->manual_strength;
		stab.global_temper_threshold_target = attr->manual_strength;
		break;
	case ISPCORE_TEMPER_MODE_AUTO:
		apical_isp_temper_temper2_mode_write(1);
		apical_isp_temper_frame_buffer_active_width_write(contrl->inwidth);
		apical_isp_temper_frame_buffer_active_height_write(contrl->inheight);
		apical_isp_temper_frame_buffer_line_offset_write(contrl->inwidth * 4);
		stab.global_maximum_temper_strength = tuning->ctrls.temper_max;
		stab.global_minimum_temper_strength = tuning->ctrls.temper_min;
		stab.global_manual_temper = 0;
		break;
	default:
		printk("%s:%d: Cant not support this mode!!!\n",__func__,__LINE__);
		return -1;
	}

	apical_isp_temper_frame_buffer_axi_port_enable_write(1);
	apical_isp_top_bypass_temper_write(temper?0:1);
	apical_isp_temper_enable_write(temper?1:0);

	return ret;
}

static int apical_isp_temper_dns_s_buf(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	int ret = ISP_SUCCESS;
	unsigned int buf = 0;

	//printk("%s, %d, control->value = 0x%x\n", __func__, __LINE__, control->value);
	buf = control->value;
	//copy_from_user(&buf, (const void __user*)control->value, sizeof(buf));
	if (buf == 0) {
		ISP_INFO("#### DEBUG: ISP can't support temper module! Because that has't enough memory!\n");
	}else{
		/* config temper dma */
		//printk("%s, %d, buf = 0x%x\n", __func__, __LINE__, buf);
		apical_isp_temper_frame_buffer_bank0_base_write(buf);
		apical_isp_temper_frame_buffer_bank1_base_write(buf);
		/* apical_isp_temper_frame_buffer_bank1_base_write(tuning->temper_paddr + (tuning->temper_buffer_size)); */
		apical_isp_temper_frame_buffer_frame_write_cancel_write(0);
		apical_isp_temper_frame_buffer_frame_read_cancel_write(0);
		apical_isp_temper_frame_buffer_frame_write_on_write(1);
		apical_isp_temper_frame_buffer_frame_read_on_write(1);
	}
	return ret;
}

static int apical_isp_temper_dns_g_strength(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	int ret = 0;
	return ret;
}

static int apical_isp_temper_dns_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_temper_attr *attr = &(tuning->ctrls.temper_attr);
	int ret = ISP_SUCCESS;

	if(apical_isp_top_bypass_temper_read())
		attr->mode = ISPCORE_TEMPER_MODE_DISABLE;
	else {
		if(stab.global_manual_temper)
			attr->mode = ISPCORE_TEMPER_MODE_MANUAL;
		else
			attr->mode = ISPCORE_TEMPER_MODE_AUTO;
	}
	attr->manual_strength = stab.global_temper_threshold_target;
	copy_to_user((void __user*)control->value, (const void *)attr, sizeof(*attr));

	return ret;
}

/* Iridix modules */
static inline int rawdrc_value_v4l2_to_apical(int val)
{
	int ret = 0;
	switch(val){
	case 0:
		ret = MANUAL;
		break;
	case 1:
		ret = UNLIMIT;
		break;
	case 2:
		ret = HIGH;
		break;
	case 3:
		ret = MEDIUM;
		break;
	case 4:
		ret = LOW;
		break;
	case 5:
		ret = OFF;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int apical_isp_drc_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	unsigned int drc = 0;
	struct isp_core_drc_attr *attr = &tuning->ctrls.drc_attr;
	ISP_CORE_MODE_DN_E dn = tuning->ctrls.daynight;
	LookupTable** table = NULL;
	TXispPrivParamManage *param = core->param;
	apical_api_control_t api;
	unsigned char status = ISP_SUCCESS;
	int reason = 0;
	int ret = ISP_SUCCESS;

	tuning->ctrls.raw_drc = control->value;
	drc = tuning->ctrls.raw_drc;

	stab.global_manual_iridix = drc == ISPMODULE_DRC_MANUAL ? 1 : 0;

	if(param == NULL)
		goto set_drc_val;

	if(dn == ISP_CORE_RUNING_MODE_DAY_MODE)
		table = param->isp_param[TX_ISP_PRIV_PARAM_DAY_MODE].calibrations;
	else
		table = param->isp_param[TX_ISP_PRIV_PARAM_NIGHT_MODE].calibrations;
	if(drc == ISPMODULE_DRC_MANUAL){
		stab.global_minimum_iridix_strength = 0;
		stab.global_maximum_iridix_strength = 255;
	}else{
		api.type = TIMAGE;
		api.dir = COMMAND_GET;
		api.id = WDR_MODE_ID;
		api.value = -1;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		if(status != ISP_SUCCESS) {
			ISP_PRINT(ISP_WARNING_LEVEL,"Get WDR mode failure!reture value is %d,reason is %d\n",status,reason);
		}

		if (reason == IMAGE_WDR_MODE_LINEAR) {
			stab.global_maximum_iridix_strength = *(uint8_t *)(table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_LINEAR]->ptr);
		} else if (reason == IMAGE_WDR_MODE_FS_HDR) {
			stab.global_maximum_iridix_strength = *(uint8_t *)(table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_WDR]->ptr);
		} else {
			printk("No this mode! Iridix set failure!\n");
		}
		stab.global_minimum_iridix_strength = *(uint8_t *)(table[_CALIBRATION_IRIDIX_MIN_MAX_STR]->ptr);
	}
set_drc_val:
	apical_isp_top_bypass_iridix_write((drc == 5)?1:0);
	attr->mode = drc;
	return ret;
}

static inline int apical_isp_drc_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_drc_attr *attr = &tuning->ctrls.drc_attr;
	apical_api_control_t api;
	unsigned char status = ISP_SUCCESS;
	int reason = 0;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;
	api.id = IRIDIX_STRENGTH_ID;
	api.value = attr->strength;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
#if 0
	apical_isp_iridix_strength_write(attr->strength);
	apical_isp_iridix_slope_max_write(attr->slope_max);
	apical_isp_iridix_slope_min_write(attr->slope_min);
	apical_isp_iridix_black_level_write(attr->black_level);
	apical_isp_iridix_white_level_write(attr->white_level);
#endif
	return ISP_SUCCESS;
}

static inline int apical_isp_drc_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_drc_attr *attr = &tuning->ctrls.drc_attr;

	attr->strength = apical_isp_iridix_strength_read();
	attr->slope_max = apical_isp_iridix_slope_max_read();
	attr->slope_min = apical_isp_iridix_slope_min_read();
	attr->black_level = apical_isp_iridix_black_level_read();
	attr->white_level = apical_isp_iridix_white_level_read();
	copy_to_user((void __user*)control->value, (const void *)attr, sizeof(*attr));

	return 0;
}

/* WDR module */
static inline int apical_isp_noise_profile_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_noise_profile_attr *attr = &tuning->ctrls.np_attr;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
	return ISP_SUCCESS;
}
static inline int apical_isp_wdr_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	struct isp_core_input_control *contrl = &core->contrl;
	unsigned int wdr = 0;
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	if(tuning->wdr_paddr == 0){
		return -EPERM;
	}
	tuning->ctrls.wdr = control->value;
	wdr = tuning->ctrls.wdr;

	api.type = TIMAGE;
	api.dir = COMMAND_SET;
	api.id = WDR_MODE_ID;
	if(wdr == ISPCORE_MODULE_DISABLE){
//		APICAL_WRITE_32(0x240, 0x000300b1); //disable frame buffers
//		APICAL_WRITE_32(0x188, 0x0);
		api.value = IMAGE_WDR_MODE_LINEAR;
	}else{
//		apical_isp_frame_stitch_frame_buffer_active_width_write(contrl->inwidth);
//		apical_isp_frame_stitch_frame_buffer_active_height_write(contrl->inheight);
		apical_isp_frame_stitch_frame_buffer_line_offset_write(contrl->inwidth * 4);
//		APICAL_WRITE_32(0x240, 0x00030041); //enable frame buffers
//		APICAL_WRITE_32(0x188, 0x3);
		api.value = IMAGE_WDR_MODE_FS_HDR;
	}
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	/* enable the moudle [WDR companded frontend lookup table] */
	apical_isp_top_bypass_gamma_fe_write(wdr?0:1);
	/* enable the module mean that disable bypass function */
	apical_isp_frame_stitch_frame_buffer_axi_port_enable_write(wdr?1:0);
	apical_isp_top_bypass_frame_stitch_write(wdr?0:1);
	/** if enable isp wdr, set 1; if enable sensor wdr, set 0 **/
	apical_isp_top_gamma_fe_position_write(wdr?1:0);
	apical_isp_input_port_field_mode_write(wdr?1:0);
	if(status != ISP_SUCCESS)
		ret = -ENOENT;
	return ret;
}

static inline int apical_isp_wdr_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_wdr_attr *attr = &tuning->ctrls.wdr_attr;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
	apical_isp_frame_stitch_short_thresh_write(attr->short_thresh);
	apical_isp_frame_stitch_long_thresh_write(attr->long_thresh);
	apical_isp_frame_stitch_exposure_ratio_write(attr->exp_ratio);
	apical_isp_frame_stitch_stitch_correct_write(attr->stitch_correct);
	apical_isp_frame_stitch_stitch_error_thresh_write(attr->stitch_err_thresh);
	apical_isp_frame_stitch_stitch_error_limit_write(attr->stitch_err_limit);
	apical_isp_frame_stitch_black_level_long_write(attr->black_level_long);
	apical_isp_frame_stitch_black_level_short_write(attr->black_level_short);
	apical_isp_frame_stitch_black_level_out_write(attr->black_level_out);
	return ISP_SUCCESS;
}

static int apical_isp_bypass_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mbus = &core->vin.mbus;

	tuning->ctrls.isp_process = control->value;

	if(control->value == ISPCORE_MODULE_ENABLE){
		switch(mbus->code){
		case V4L2_MBUS_FMT_RGB565_2X8_LE:
		case V4L2_MBUS_FMT_RGB888_3X8_LE:
			apical_isp_top_isp_processing_bypass_mode_write(0);
			apical_isp_top_isp_raw_bypass_write(1);
			break;
		case V4L2_MBUS_FMT_YUYV8_1X16:
			apical_isp_top_isp_processing_bypass_mode_write(3);
			apical_isp_top_isp_raw_bypass_write(1);
			break;
		default:
			apical_isp_top_isp_processing_bypass_mode_write(0);
			apical_isp_top_isp_raw_bypass_write(0);
			break;
		}
		apical_isp_top_ds1_dma_source_write(0);
		core->bypass = TX_ISP_FRAME_CHANNEL_BYPASS_ISP_DISABLE;
		printk("@@@ bypass disable @@@\n");
	}else{
		apical_isp_top_isp_processing_bypass_mode_write(2);
		apical_isp_top_isp_raw_bypass_write(1);
		apical_isp_top_ds1_dma_source_write(1);
		core->bypass = TX_ISP_FRAME_CHANNEL_BYPASS_ISP_ENABLE;
		printk("@@@ bypass enable @@@\n");
	}
	return ISP_SUCCESS;
}

static int apical_isp_freeze_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	tuning->ctrls.freeze_fw = control->value;

	apical_ext_system_freeze_firmware_write(control->value);

	return ISP_SUCCESS;
}

static inline int flicker_value_v4l2_to_apical(int val)
{
	int ret = 0;
	switch(val){
	case V4L2_CID_POWER_LINE_FREQUENCY_DISABLED:
		ret = 0;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
		ret = 50;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
		ret = 60;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static inline int apical_isp_flicker_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	tuning->ctrls.flicker = control->value;
	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;
	api.id = ANTIFLICKER_MODE_ID;
	api.value = flicker_value_v4l2_to_apical(tuning->ctrls.flicker);
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	stab.global_antiflicker_enable = api.value ?1 :0;
	return ret;
}

/* lens shading module */
static int apical_isp_lens_shad_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	tuning->ctrls.lens_shad = control->value;
	apical_isp_mesh_shading_enable_write(control->value);
	apical_isp_top_bypass_mesh_shading_write(control->value?0:1);
	return ISP_SUCCESS;
}

static int apical_isp_lens_shad_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_shading_attr *attr = &tuning->ctrls.shad_attr;
	int i, base;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
	apical_isp_mesh_shading_mesh_alpha_mode_write(attr->mesh_mode);
	apical_isp_mesh_shading_mesh_scale_write(attr->mesh_scale);
	apical_isp_mesh_shading_mesh_page_r_write(attr->r_page);
	apical_isp_mesh_shading_mesh_page_g_write(attr->g_page);
	apical_isp_mesh_shading_mesh_page_b_write(attr->b_page);
	apical_isp_mesh_shading_mesh_alpha_bank_r_write(attr->mesh_alpha_bank_r);
	apical_isp_mesh_shading_mesh_alpha_bank_g_write(attr->mesh_alpha_bank_g);
	apical_isp_mesh_shading_mesh_alpha_bank_b_write(attr->mesh_alpha_bank_b);
	apical_isp_mesh_shading_mesh_alpha_r_write(attr->mesh_alpha_r);
	apical_isp_mesh_shading_mesh_alpha_g_write(attr->mesh_alpha_g);
	apical_isp_mesh_shading_mesh_alpha_b_write(attr->mesh_alpha_b);
//	apical_isp_mesh_shading_mesh_strength_write(attr->mesh_strength);
	if(attr->update_page){
		base = 0x4000;
		for(i = 0; i < 1024; i++){
			APICAL_WRITE_32(base, attr->page0.regs[i]);
			base += 4;
		}
		for(i = 0; i < 1024; i++){
			APICAL_WRITE_32(base, attr->page1.regs[i]);
			base += 4;
		}
		for(i = 0; i < 1024; i++){
			APICAL_WRITE_32(base, attr->page2.regs[i]);
			base += 4;
		}
		apical_isp_mesh_shading_mesh_reload_write(1);
	}

	return ISP_SUCCESS;
}

/* Raw Frontend */
static inline int apical_isp_ge_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_green_eq_attr *attr = &tuning->ctrls.ge_attr;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
	if(attr->mode){
		apical_isp_raw_frontend_ge_strength_write(attr->strength);
		apical_isp_raw_frontend_ge_threshold_write(attr->threshold);
		apical_isp_raw_frontend_ge_slope_write(attr->slope);
		apical_isp_raw_frontend_ge_sens_write(attr->sensitivity);
	}
	apical_isp_raw_frontend_ge_enable_write(attr->mode);
	return ISP_SUCCESS;
}


static int apical_isp_dynamic_dp_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_dynamic_defect_pixel_attr *attr = &tuning->ctrls.ddp_attr;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
	if(attr->mode){
		apical_isp_raw_frontend_dark_disable_write(attr->dark_pixels);
		apical_isp_raw_frontend_bright_disable_write(attr->bright_pixels);
		apical_isp_raw_frontend_dp_threshold_write(attr->d_threshold);
		apical_isp_raw_frontend_dp_slope_write(attr->d_slope);
		apical_isp_raw_frontend_hpdev_threshold_write(attr->hpdev_thresh);
		apical_isp_raw_frontend_line_thresh_write(attr->line_thresh);
		apical_isp_raw_frontend_hp_blend_write(attr->hp_blend);
	}
	apical_isp_raw_frontend_dp_enable_write(attr->mode);

	return ISP_SUCCESS;
}

static int apical_isp_static_dp_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_static_defect_pixel_attr *attr = &tuning->ctrls.sdp_attr;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));

	return ISP_SUCCESS;
}

static int apical_isp_antifog_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	tuning->ctrls.fog = control->value;
	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;
	api.value = ANTIFOG_STRONG;
	if(control->value){
		/* set present mode */
		api.id = ANTIFOG_PRESET_ID;
		switch(control->value){
		case 3:
			api.value = ANTIFOG_WEAK;
			break;
		case 2:
			api.value = ANTIFOG_MEDIUM;
			break;
		case 1:
			api.value = ANTIFOG_STRONG;
			break;
		default:
			break;
		}
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		/* enable anti fog modules */
		api.id = ANTIFOG_MODE_ID;
		api.value = ANTIFOG_ENABLE;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	}else{
		/* disable anti fog modules */
		api.id = ANTIFOG_MODE_ID;
		api.value = ANTIFOG_DISABLE;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	}

	return ret;
}


static inline int scene_value_v4l2_to_apical(int val)
{
	int ret = 0;
	switch(val){
	case V4L2_SCENE_MODE_NONE:
		ret = AUTO;
		break;
	case V4L2_SCENE_MODE_BEACH_SNOW:
		ret = BEACH_SNOW;
		break;
	case V4L2_SCENE_MODE_CANDLE_LIGHT:
		ret = CANDLE;
		break;
	case V4L2_SCENE_MODE_DAWN_DUSK:
		ret = DAWN;
		break;
	case V4L2_SCENE_MODE_FIREWORKS:
		ret = FIREWORKS;
		break;
	case V4L2_SCENE_MODE_LANDSCAPE:
		ret = LANDSCAPE;
		break;
	case V4L2_SCENE_MODE_NIGHT:
		ret = NIGHT;
		break;
	case V4L2_SCENE_MODE_PARTY_INDOOR:
		ret = INDOOR;
		break;
	case V4L2_SCENE_MODE_PORTRAIT:
		ret = PORTRAIT;
		break;
	case V4L2_SCENE_MODE_SPORTS:
		ret = MOTION;
		break;
	case V4L2_SCENE_MODE_SUNSET:
		ret = SUNSET;
		break;
	case V4L2_SCENE_MODE_TEXT:
		ret = TEXT;
		break;
	case V4L2_SCENE_MODE_TEXT + 1:
		ret = NIGHT_PORTRAIT;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static inline int apical_isp_scene_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	return ISP_SUCCESS;
	tuning->ctrls.scene = control->value;
	api.type = TSCENE_MODES;
	api.dir = COMMAND_SET;
	api.id = SCENE_MODE_ID;
	api.value = scene_value_v4l2_to_apical(tuning->ctrls.scene);
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	return ret;
}

static inline int colorfx_value_v4l2_to_apical(int val)
{
	int ret = 0;
	switch(val){
	case V4L2_COLORFX_NONE:
		ret = NORMAL;
		break;
	case V4L2_COLORFX_BW:
		ret = BLACK_AND_WHITE;
		break;
	case V4L2_COLORFX_SEPIA:
		ret = SEPIA;
		break;
	case V4L2_COLORFX_NEGATIVE:
		ret = NEGATIVE;
		break;
	case V4L2_COLORFX_VIVID:
		ret = VIVID;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static inline int apical_isp_colorfx_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	tuning->ctrls.colorfx = control->value;
	api.type = TSCENE_MODES;
	api.dir = COMMAND_SET;
	api.id = COLOR_MODE_ID;
	api.value = colorfx_value_v4l2_to_apical(tuning->ctrls.colorfx);
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	return ret;
}

static inline int apical_isp_day_or_night_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);
	unsigned int  dn= ctrls->daynight;
	control->value = dn;

	return ISP_SUCCESS;
}


static int apical_isp_day_or_night_s_ctrl_internal(image_tuning_vdrv_t *tuning)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);
	int ret = ISP_SUCCESS;
	LookupTable** table = NULL;
	TXispPrivCustomerParamer *customer = NULL;
	unsigned int tmp_top = 0;
	apical_api_control_t api;
	unsigned int reason = 0;
	unsigned int status = 0;

	ISP_CORE_MODE_DN_E dn = ctrls->daynight;
	if(!param){
		ISP_ERROR("Can't get the parameters of isp tuning!\n");
		return -ENOENT;
	}
	{
		tmp_top = APICAL_READ_32(0x40);
		if(dn == ISP_CORE_RUNING_MODE_DAY_MODE){
#if TX_ISP_EXIST_FR_CHANNEL
			apical_isp_fr_cs_conv_clip_min_uv_write(0);
			apical_isp_fr_cs_conv_clip_max_uv_write(1023);
#endif
			apical_isp_ds1_cs_conv_clip_min_uv_write(0);
			apical_isp_ds1_cs_conv_clip_max_uv_write(1023);
#if TX_ISP_EXIST_DS2_CHANNEL
			apical_isp_ds2_cs_conv_clip_min_uv_write(0);
			apical_isp_ds2_cs_conv_clip_max_uv_write(1023);
#endif
			table = param->isp_param[TX_ISP_PRIV_PARAM_DAY_MODE].calibrations;
			customer = &param->customer[TX_ISP_PRIV_PARAM_DAY_MODE];
			/* tmp_top |= param->customer[TX_ISP_PRIV_PARAM_NIGHT_MODE].top; */
		}else{
#if TX_ISP_EXIST_FR_CHANNEL
			apical_isp_fr_cs_conv_clip_min_uv_write(512);
			apical_isp_fr_cs_conv_clip_max_uv_write(512);
#endif
			apical_isp_ds1_cs_conv_clip_min_uv_write(512);
			apical_isp_ds1_cs_conv_clip_max_uv_write(512);
#if TX_ISP_EXIST_DS2_CHANNEL
			apical_isp_ds2_cs_conv_clip_min_uv_write(512);
			apical_isp_ds2_cs_conv_clip_max_uv_write(512);
#endif
			table = param->isp_param[TX_ISP_PRIV_PARAM_NIGHT_MODE].calibrations;
			customer = &param->customer[TX_ISP_PRIV_PARAM_NIGHT_MODE];
			/* tmp_top |= param->customer[TX_ISP_PRIV_PARAM_DAY_MODE].top; */
		}
		if(table && customer){
			tmp_top = (tmp_top | 0x0c02da6c) & (~(customer->top));
			if(TX_ISP_EXIST_FR_CHANNEL == 0)
				tmp_top |= 0x00fc0000;
			/* dynamic calibration */
			apical_api_calibration(CALIBRATION_NP_LUT_MEAN,COMMAND_SET, table[_CALIBRATION_NP_LUT_MEAN]->ptr,
					       table[_CALIBRATION_NP_LUT_MEAN]->rows * table[_CALIBRATION_NP_LUT_MEAN]->cols
					       * table[_CALIBRATION_NP_LUT_MEAN]->width, &ret);
			apical_api_calibration(CALIBRATION_EVTOLUX_PROBABILITY_ENABLE,COMMAND_SET, table[ _CALIBRATION_EVTOLUX_PROBABILITY_ENABLE]->ptr,
					       table[ _CALIBRATION_EVTOLUX_PROBABILITY_ENABLE]->rows * table[ _CALIBRATION_EVTOLUX_PROBABILITY_ENABLE]->cols
					       * table[_CALIBRATION_EVTOLUX_PROBABILITY_ENABLE]->width, &ret);
			apical_api_calibration(CALIBRATION_AE_EXPOSURE_AVG_COEF,COMMAND_SET, table[ _CALIBRATION_AE_EXPOSURE_AVG_COEF]->ptr,
					       table[ _CALIBRATION_AE_EXPOSURE_AVG_COEF]->rows * table[ _CALIBRATION_AE_EXPOSURE_AVG_COEF]->cols
					       * table[ _CALIBRATION_AE_EXPOSURE_AVG_COEF]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_AVG_COEF,COMMAND_SET, table[_CALIBRATION_IRIDIX_AVG_COEF]->ptr,
					       table[_CALIBRATION_IRIDIX_AVG_COEF]->rows * table[_CALIBRATION_IRIDIX_AVG_COEF]->cols
					       * table[_CALIBRATION_IRIDIX_AVG_COEF]->width, &ret);
			apical_api_calibration(CALIBRATION_AF_MIN_TABLE,COMMAND_SET, table[_CALIBRATION_AF_MIN_TABLE]->ptr,
					       table[_CALIBRATION_AF_MIN_TABLE]->rows * table[_CALIBRATION_AF_MIN_TABLE]->cols
					       * table[_CALIBRATION_AF_MIN_TABLE]->width, &ret);
			apical_api_calibration(CALIBRATION_AF_MAX_TABLE,COMMAND_SET, table[_CALIBRATION_AF_MAX_TABLE]->ptr,
					       table[_CALIBRATION_AF_MAX_TABLE]->rows * table[_CALIBRATION_AF_MAX_TABLE]->cols
					       * table[_CALIBRATION_AF_MAX_TABLE]->width, &ret);
			apical_api_calibration(CALIBRATION_AF_WINDOW_RESIZE_TABLE,COMMAND_SET, table[_CALIBRATION_AF_WINDOW_RESIZE_TABLE]->ptr,
					       table[_CALIBRATION_AF_WINDOW_RESIZE_TABLE]->rows * table[_CALIBRATION_AF_WINDOW_RESIZE_TABLE]->cols
					       * table[_CALIBRATION_AF_WINDOW_RESIZE_TABLE]->width, &ret);
			apical_api_calibration(CALIBRATION_EXP_RATIO_TABLE,COMMAND_SET, table[_CALIBRATION_EXP_RATIO_TABLE]->ptr,
					       table[_CALIBRATION_EXP_RATIO_TABLE]->rows * table[_CALIBRATION_EXP_RATIO_TABLE]->cols
					       * table[_CALIBRATION_EXP_RATIO_TABLE]->width, &ret);
			apical_api_calibration(CALIBRATION_CCM_ONE_GAIN_THRESHOLD,COMMAND_SET, table[_CALIBRATION_CCM_ONE_GAIN_THRESHOLD]->ptr,
					       table[_CALIBRATION_CCM_ONE_GAIN_THRESHOLD]->rows * table[_CALIBRATION_CCM_ONE_GAIN_THRESHOLD]->cols
					       * table[_CALIBRATION_CCM_ONE_GAIN_THRESHOLD]->width, &ret);
			apical_api_calibration(CALIBRATION_FLASH_RG,COMMAND_SET, table[_CALIBRATION_FLASH_RG]->ptr,
					       table[_CALIBRATION_FLASH_RG]->rows * table[_CALIBRATION_FLASH_RG]->cols
					       * table[_CALIBRATION_FLASH_RG]->width, &ret);
			apical_api_calibration(CALIBRATION_FLASH_BG,COMMAND_SET, table[_CALIBRATION_FLASH_BG]->ptr,
					       table[_CALIBRATION_FLASH_BG]->rows * table[_CALIBRATION_FLASH_BG]->cols
					       * table[_CALIBRATION_FLASH_BG]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_LINEAR,COMMAND_SET, table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_LINEAR]->ptr,
					       table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_LINEAR]->rows * table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_LINEAR]->cols
					       * table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_WDR,COMMAND_SET, table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_WDR]->ptr,
					       table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_WDR]->rows * table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_WDR]->cols
					       * table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_BLACK_PRC,COMMAND_SET, table[_CALIBRATION_IRIDIX_BLACK_PRC]->ptr,
					       table[_CALIBRATION_IRIDIX_BLACK_PRC]->rows * table[_CALIBRATION_IRIDIX_BLACK_PRC]->cols
					       * table[_CALIBRATION_IRIDIX_BLACK_PRC]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_GAIN_MAX,COMMAND_SET, table[_CALIBRATION_IRIDIX_GAIN_MAX]->ptr,
					       table[_CALIBRATION_IRIDIX_GAIN_MAX]->rows * table[_CALIBRATION_IRIDIX_GAIN_MAX]->cols
					       * table[_CALIBRATION_IRIDIX_GAIN_MAX]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_MIN_MAX_STR,COMMAND_SET, table[_CALIBRATION_IRIDIX_MIN_MAX_STR]->ptr,
					       table[_CALIBRATION_IRIDIX_MIN_MAX_STR]->rows * table[_CALIBRATION_IRIDIX_MIN_MAX_STR]->cols
					       * table[_CALIBRATION_IRIDIX_MIN_MAX_STR]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_EV_LIM_FULL_STR,COMMAND_SET, table[_CALIBRATION_IRIDIX_EV_LIM_FULL_STR]->ptr,
					       table[_CALIBRATION_IRIDIX_EV_LIM_FULL_STR]->rows * table[_CALIBRATION_IRIDIX_EV_LIM_FULL_STR]->cols
					       * table[_CALIBRATION_IRIDIX_EV_LIM_FULL_STR]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_EV_LIM_NO_STR_LINEAR,COMMAND_SET, table[_CALIBRATION_IRIDIX_EV_LIM_NO_STR_LINEAR]->ptr,
					       table[_CALIBRATION_IRIDIX_EV_LIM_NO_STR_LINEAR]->rows * table[_CALIBRATION_IRIDIX_EV_LIM_NO_STR_LINEAR]->cols
					       * table[_CALIBRATION_IRIDIX_EV_LIM_NO_STR_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_EV_LIM_NO_STR_FS_HDR,COMMAND_SET, table[_CALIBRATION_IRIDIX_EV_LIM_NO_STR_FS_HDR]->ptr,
					       table[_CALIBRATION_IRIDIX_EV_LIM_NO_STR_FS_HDR]->rows * table[_CALIBRATION_IRIDIX_EV_LIM_NO_STR_FS_HDR]->cols
					       * table[_CALIBRATION_IRIDIX_EV_LIM_NO_STR_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_AE_CORRECTION_LINEAR,COMMAND_SET, table[_CALIBRATION_AE_CORRECTION_LINEAR]->ptr,
					       table[_CALIBRATION_AE_CORRECTION_LINEAR]->rows * table[_CALIBRATION_AE_CORRECTION_LINEAR]->cols
					       * table[_CALIBRATION_AE_CORRECTION_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_AE_CORRECTION_FS_HDR,COMMAND_SET, table[_CALIBRATION_AE_CORRECTION_FS_HDR]->ptr,
					       table[_CALIBRATION_AE_CORRECTION_FS_HDR]->rows * table[_CALIBRATION_AE_CORRECTION_FS_HDR]->cols
					       * table[_CALIBRATION_AE_CORRECTION_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_AE_EXPOSURE_CORRECTION,COMMAND_SET, table[_CALIBRATION_AE_EXPOSURE_CORRECTION]->ptr,
					       table[_CALIBRATION_AE_EXPOSURE_CORRECTION]->rows * table[_CALIBRATION_AE_EXPOSURE_CORRECTION]->cols
					       * table[_CALIBRATION_AE_EXPOSURE_CORRECTION]->width, &ret);
			apical_api_calibration(CALIBRATION_SINTER_STRENGTH_LINEAR,COMMAND_SET, table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->ptr,
					       table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->rows * table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->cols
					       * table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->width, &ret);

			apical_api_calibration(CALIBRATION_SINTER_STRENGTH_FS_HDR,COMMAND_SET, table[_CALIBRATION_SINTER_STRENGTH_FS_HDR]->ptr,
					       table[_CALIBRATION_SINTER_STRENGTH_FS_HDR]->rows * table[_CALIBRATION_SINTER_STRENGTH_FS_HDR]->cols
					       * table[_CALIBRATION_SINTER_STRENGTH_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SINTER_STRENGTH1_LINEAR,COMMAND_SET, table[_CALIBRATION_SINTER_STRENGTH1_LINEAR]->ptr,
					       table[_CALIBRATION_SINTER_STRENGTH1_LINEAR]->rows * table[_CALIBRATION_SINTER_STRENGTH1_LINEAR]->cols
					       * table[_CALIBRATION_SINTER_STRENGTH1_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SINTER_STRENGTH1_FS_HDR,COMMAND_SET, table[_CALIBRATION_SINTER_STRENGTH1_FS_HDR]->ptr,
					       table[_CALIBRATION_SINTER_STRENGTH1_FS_HDR]->rows * table[_CALIBRATION_SINTER_STRENGTH1_FS_HDR]->cols
					       * table[_CALIBRATION_SINTER_STRENGTH1_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SINTER_THRESH1_LINEAR,COMMAND_SET, table[_CALIBRATION_SINTER_THRESH1_LINEAR]->ptr,
					       table[_CALIBRATION_SINTER_THRESH1_LINEAR]->rows * table[_CALIBRATION_SINTER_THRESH1_LINEAR]->cols
					       * table[_CALIBRATION_SINTER_THRESH1_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SINTER_THRESH1_FS_HDR,COMMAND_SET, table[ _CALIBRATION_SINTER_THRESH1_FS_HDR]->ptr,
					       table[ _CALIBRATION_SINTER_THRESH1_FS_HDR]->rows * table[ _CALIBRATION_SINTER_THRESH1_FS_HDR]->cols
					       * table[ _CALIBRATION_SINTER_THRESH1_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SINTER_THRESH4_LINEAR,COMMAND_SET, table[ _CALIBRATION_SINTER_THRESH4_LINEAR]->ptr,
					       table[ _CALIBRATION_SINTER_THRESH4_LINEAR]->rows * table[ _CALIBRATION_SINTER_THRESH4_LINEAR]->cols
					       * table[ _CALIBRATION_SINTER_THRESH4_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SINTER_THRESH4_FS_HDR,COMMAND_SET, table[ _CALIBRATION_SINTER_THRESH4_FS_HDR]->ptr,
					       table[_CALIBRATION_SINTER_THRESH4_FS_HDR]->rows * table[ _CALIBRATION_SINTER_THRESH4_FS_HDR]->cols
					       * table[_CALIBRATION_SINTER_THRESH4_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHARP_ALT_D_LINEAR,COMMAND_SET, table[ _CALIBRATION_SHARP_ALT_D_LINEAR]->ptr,
					       table[_CALIBRATION_SHARP_ALT_D_LINEAR]->rows * table[_CALIBRATION_SHARP_ALT_D_LINEAR]->cols
					       * table[_CALIBRATION_SHARP_ALT_D_LINEAR]->width, &ret);

			apical_api_calibration(CALIBRATION_SHARP_ALT_D_FS_HDR,COMMAND_SET, table[_CALIBRATION_SHARP_ALT_D_FS_HDR]->ptr,
					       table[_CALIBRATION_SHARP_ALT_D_FS_HDR]->rows * table[_CALIBRATION_SHARP_ALT_D_FS_HDR]->cols
					       * table[_CALIBRATION_SHARP_ALT_D_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHARP_ALT_UD_LINEAR,COMMAND_SET, table[_CALIBRATION_SHARP_ALT_UD_LINEAR]->ptr,
					       table[_CALIBRATION_SHARP_ALT_UD_LINEAR]->rows * table[_CALIBRATION_SHARP_ALT_UD_LINEAR]->cols
					       * table[_CALIBRATION_SHARP_ALT_UD_LINEAR]->width, &ret);

			apical_api_calibration(CALIBRATION_SHARP_ALT_UD_FS_HDR,COMMAND_SET, table[ _CALIBRATION_SHARP_ALT_UD_FS_HDR]->ptr,
					       table[_CALIBRATION_SHARP_ALT_UD_FS_HDR]->rows * table[_CALIBRATION_SHARP_ALT_UD_FS_HDR]->cols
					       * table[_CALIBRATION_SHARP_ALT_UD_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHARPEN_FR_LINEAR,COMMAND_SET, table[ _CALIBRATION_SHARPEN_FR_LINEAR]->ptr,
					       table[_CALIBRATION_SHARPEN_FR_LINEAR]->rows * table[_CALIBRATION_SHARPEN_FR_LINEAR]->cols
					       * table[_CALIBRATION_SHARPEN_FR_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHARPEN_FR_WDR,COMMAND_SET, table[_CALIBRATION_SHARPEN_FR_WDR]->ptr,
					       table[ _CALIBRATION_SHARPEN_FR_WDR]->rows * table[ _CALIBRATION_SHARPEN_FR_WDR]->cols
					       * table[ _CALIBRATION_SHARPEN_FR_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHARPEN_DS1_LINEAR,COMMAND_SET, table[_CALIBRATION_SHARPEN_DS1_LINEAR]->ptr,
					       table[_CALIBRATION_SHARPEN_DS1_LINEAR]->rows * table[_CALIBRATION_SHARPEN_DS1_LINEAR]->cols
					       * table[_CALIBRATION_SHARPEN_DS1_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHARPEN_DS1_WDR,COMMAND_SET, table[_CALIBRATION_SHARPEN_DS1_WDR]->ptr,
					       table[_CALIBRATION_SHARPEN_DS1_WDR]->rows * table[_CALIBRATION_SHARPEN_DS1_WDR]->cols
					       * table[_CALIBRATION_SHARPEN_DS1_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_DEMOSAIC_NP_OFFSET_LINEAR,COMMAND_SET, table[_CALIBRATION_DEMOSAIC_NP_OFFSET_LINEAR]->ptr,
					       table[_CALIBRATION_DEMOSAIC_NP_OFFSET_LINEAR]->rows * table[_CALIBRATION_DEMOSAIC_NP_OFFSET_LINEAR]->cols
					       * table[_CALIBRATION_DEMOSAIC_NP_OFFSET_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_DEMOSAIC_NP_OFFSET_FS_HDR,COMMAND_SET, table[_CALIBRATION_DEMOSAIC_NP_OFFSET_FS_HDR]->ptr,
					       table[_CALIBRATION_DEMOSAIC_NP_OFFSET_FS_HDR]->rows * table[_CALIBRATION_DEMOSAIC_NP_OFFSET_FS_HDR]->cols
					       * table[_CALIBRATION_DEMOSAIC_NP_OFFSET_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_MESH_SHADING_STRENGTH,COMMAND_SET, table[_CALIBRATION_MESH_SHADING_STRENGTH]->ptr,
					       table[_CALIBRATION_MESH_SHADING_STRENGTH]->rows * table[_CALIBRATION_MESH_SHADING_STRENGTH]->cols
					       * table[_CALIBRATION_MESH_SHADING_STRENGTH]->width, &ret);
			apical_api_calibration(CALIBRATION_SATURATION_STRENGTH_LINEAR,COMMAND_SET, table[_CALIBRATION_SATURATION_STRENGTH_LINEAR]->ptr,
					       table[_CALIBRATION_SATURATION_STRENGTH_LINEAR]->rows * table[_CALIBRATION_SATURATION_STRENGTH_LINEAR]->cols
					       * table[_CALIBRATION_SATURATION_STRENGTH_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_TEMPER_STRENGTH,COMMAND_SET, table[_CALIBRATION_TEMPER_STRENGTH]->ptr,
					       table[_CALIBRATION_TEMPER_STRENGTH]->rows * table[_CALIBRATION_TEMPER_STRENGTH]->cols
					       * table[_CALIBRATION_TEMPER_STRENGTH]->width, &ret);

			apical_api_calibration(CALIBRATION_STITCHING_ERROR_THRESH,COMMAND_SET, table[_CALIBRATION_STITCHING_ERROR_THRESH]->ptr,
					       table[_CALIBRATION_STITCHING_ERROR_THRESH]->rows * table[_CALIBRATION_STITCHING_ERROR_THRESH]->cols
					       * table[_CALIBRATION_STITCHING_ERROR_THRESH]->width, &ret);
			apical_api_calibration(CALIBRATION_DP_SLOPE_LINEAR,COMMAND_SET, table[_CALIBRATION_DP_SLOPE_LINEAR]->ptr,
					       table[_CALIBRATION_DP_SLOPE_LINEAR]->rows * table[_CALIBRATION_DP_SLOPE_LINEAR]->cols
					       * table[_CALIBRATION_DP_SLOPE_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_DP_SLOPE_FS_HDR,COMMAND_SET, table[_CALIBRATION_DP_SLOPE_FS_HDR]->ptr,
					       table[_CALIBRATION_DP_SLOPE_FS_HDR]->rows * table[_CALIBRATION_DP_SLOPE_FS_HDR]->cols
					       * table[_CALIBRATION_DP_SLOPE_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_DP_THRESHOLD_LINEAR,COMMAND_SET, table[_CALIBRATION_DP_THRESHOLD_LINEAR]->ptr,
					       table[_CALIBRATION_DP_THRESHOLD_LINEAR]->rows * table[_CALIBRATION_DP_THRESHOLD_LINEAR]->cols
					       * table[_CALIBRATION_DP_THRESHOLD_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_DP_THRESHOLD_FS_HDR,COMMAND_SET, table[_CALIBRATION_DP_THRESHOLD_FS_HDR]->ptr,
					       table[_CALIBRATION_DP_THRESHOLD_FS_HDR]->rows * table[_CALIBRATION_DP_THRESHOLD_FS_HDR]->cols
					       * table[_CALIBRATION_DP_THRESHOLD_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_AE_BALANCED_LINEAR,COMMAND_SET, table[_CALIBRATION_AE_BALANCED_LINEAR]->ptr,
					       table[_CALIBRATION_AE_BALANCED_LINEAR]->rows * table[_CALIBRATION_AE_BALANCED_LINEAR]->cols
					       * table[_CALIBRATION_AE_BALANCED_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_AE_BALANCED_WDR,COMMAND_SET, table[_CALIBRATION_AE_BALANCED_WDR]->ptr,
					       table[_CALIBRATION_AE_BALANCED_WDR]->rows * table[_CALIBRATION_AE_BALANCED_WDR]->cols
					       * table[_CALIBRATION_AE_BALANCED_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_STRENGTH_TABLE,COMMAND_SET, table[_CALIBRATION_IRIDIX_STRENGTH_TABLE]->ptr,
					       table[_CALIBRATION_IRIDIX_STRENGTH_TABLE]->rows * table[_CALIBRATION_IRIDIX_STRENGTH_TABLE]->cols
					       * table[_CALIBRATION_IRIDIX_STRENGTH_TABLE]->width, &ret);

			apical_api_calibration(CALIBRATION_RGB2YUV_CONVERSION,COMMAND_SET, table[_CALIBRATION_RGB2YUV_CONVERSION]->ptr,
					       table[_CALIBRATION_RGB2YUV_CONVERSION]->rows * table[_CALIBRATION_RGB2YUV_CONVERSION]->cols
					       * table[_CALIBRATION_RGB2YUV_CONVERSION]->width, &ret);

			/* static parameter */
			apical_api_calibration(CALIBRATION_EVTOLUX_EV_LUT_LINEAR, COMMAND_SET, table[_CALIBRATION_EVTOLUX_EV_LUT_LINEAR]->ptr,
					       table[_CALIBRATION_EVTOLUX_EV_LUT_LINEAR]->rows * table[_CALIBRATION_EVTOLUX_EV_LUT_LINEAR]->cols
					       * table[_CALIBRATION_EVTOLUX_EV_LUT_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_EVTOLUX_EV_LUT_FS_HDR, COMMAND_SET, table[_CALIBRATION_EVTOLUX_EV_LUT_FS_HDR]->ptr,
					       table[_CALIBRATION_EVTOLUX_EV_LUT_FS_HDR]->rows * table[_CALIBRATION_EVTOLUX_EV_LUT_FS_HDR]->cols
					       * table[_CALIBRATION_EVTOLUX_EV_LUT_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_EVTOLUX_LUX_LUT, COMMAND_SET, table[_CALIBRATION_EVTOLUX_LUX_LUT]->ptr,
					       table[_CALIBRATION_EVTOLUX_LUX_LUT]->rows * table[_CALIBRATION_EVTOLUX_LUX_LUT]->cols
					       * table[_CALIBRATION_EVTOLUX_LUX_LUT]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_A_R_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_A_R_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_A_R_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_A_R_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_A_R_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_A_G_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_A_G_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_A_G_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_A_G_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_A_G_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_A_B_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_A_B_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_A_B_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_A_B_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_A_B_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_TL84_R_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_TL84_R_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_TL84_R_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_TL84_R_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_TL84_R_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_TL84_G_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_TL84_G_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_TL84_G_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_TL84_G_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_TL84_G_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_TL84_B_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_TL84_B_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_TL84_B_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_TL84_B_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_TL84_B_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_D65_R_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_D65_R_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_D65_R_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_D65_R_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_D65_R_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_D65_G_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_D65_G_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_D65_G_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_D65_G_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_D65_G_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_D65_B_LINEAR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_D65_B_LINEAR]->ptr,
					       table[_CALIBRATION_SHADING_LS_D65_B_LINEAR]->rows * table[_CALIBRATION_SHADING_LS_D65_B_LINEAR]->cols
					       * table[_CALIBRATION_SHADING_LS_D65_B_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_A_R_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_A_R_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_A_R_WDR]->rows * table[_CALIBRATION_SHADING_LS_A_R_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_A_R_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_A_G_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_A_G_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_A_G_WDR]->rows * table[_CALIBRATION_SHADING_LS_A_G_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_A_G_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_A_B_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_A_B_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_A_B_WDR]->rows * table[_CALIBRATION_SHADING_LS_A_B_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_A_B_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_TL84_R_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_TL84_R_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_TL84_R_WDR]->rows * table[_CALIBRATION_SHADING_LS_TL84_R_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_TL84_R_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_TL84_G_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_TL84_G_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_TL84_G_WDR]->rows * table[_CALIBRATION_SHADING_LS_TL84_G_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_TL84_G_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_TL84_B_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_TL84_B_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_TL84_B_WDR]->rows * table[_CALIBRATION_SHADING_LS_TL84_B_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_TL84_B_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_D65_R_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_D65_R_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_D65_R_WDR]->rows * table[_CALIBRATION_SHADING_LS_D65_R_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_D65_R_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_D65_G_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_D65_G_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_D65_G_WDR]->rows * table[_CALIBRATION_SHADING_LS_D65_G_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_D65_G_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_SHADING_LS_D65_B_WDR, COMMAND_SET, table[_CALIBRATION_SHADING_LS_D65_B_WDR]->ptr,
					       table[_CALIBRATION_SHADING_LS_D65_B_WDR]->rows * table[_CALIBRATION_SHADING_LS_D65_B_WDR]->cols
					       * table[_CALIBRATION_SHADING_LS_D65_B_WDR]->width, &ret);
			apical_api_calibration(CALIBRATION_NOISE_PROFILE_LINEAR, COMMAND_SET, table[_CALIBRATION_NOISE_PROFILE_LINEAR]->ptr,
					       table[_CALIBRATION_NOISE_PROFILE_LINEAR]->rows * table[_CALIBRATION_NOISE_PROFILE_LINEAR]->cols
					       * table[_CALIBRATION_NOISE_PROFILE_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_DEMOSAIC_LINEAR, COMMAND_SET, table[_CALIBRATION_DEMOSAIC_LINEAR]->ptr,
					       table[_CALIBRATION_DEMOSAIC_LINEAR]->rows * table[_CALIBRATION_DEMOSAIC_LINEAR]->cols
					       * table[_CALIBRATION_DEMOSAIC_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_NOISE_PROFILE_FS_HDR, COMMAND_SET, table[_CALIBRATION_NOISE_PROFILE_FS_HDR]->ptr,
					       table[_CALIBRATION_NOISE_PROFILE_FS_HDR]->rows * table[_CALIBRATION_NOISE_PROFILE_FS_HDR]->cols
					       * table[_CALIBRATION_NOISE_PROFILE_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_DEMOSAIC_FS_HDR, COMMAND_SET, table[_CALIBRATION_DEMOSAIC_FS_HDR]->ptr,
					       table[_CALIBRATION_DEMOSAIC_FS_HDR]->rows * table[_CALIBRATION_DEMOSAIC_FS_HDR]->cols
					       * table[_CALIBRATION_DEMOSAIC_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_GAMMA_FE_0_FS_HDR, COMMAND_SET, table[_CALIBRATION_GAMMA_FE_0_FS_HDR]->ptr,
					       table[_CALIBRATION_GAMMA_FE_0_FS_HDR]->rows * table[_CALIBRATION_GAMMA_FE_0_FS_HDR]->cols
					       * table[_CALIBRATION_GAMMA_FE_0_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_GAMMA_FE_1_FS_HDR, COMMAND_SET, table[_CALIBRATION_GAMMA_FE_1_FS_HDR]->ptr,
					       table[_CALIBRATION_GAMMA_FE_1_FS_HDR]->rows * table[_CALIBRATION_GAMMA_FE_1_FS_HDR]->cols
					       * table[_CALIBRATION_GAMMA_FE_1_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_BLACK_LEVEL_R_LINEAR, COMMAND_SET, table[_CALIBRATION_BLACK_LEVEL_R_LINEAR]->ptr,
					       table[_CALIBRATION_BLACK_LEVEL_R_LINEAR]->rows * table[_CALIBRATION_BLACK_LEVEL_R_LINEAR]->cols
					       * table[_CALIBRATION_BLACK_LEVEL_R_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_BLACK_LEVEL_GR_LINEAR, COMMAND_SET, table[_CALIBRATION_BLACK_LEVEL_GR_LINEAR]->ptr,
					       table[_CALIBRATION_BLACK_LEVEL_GR_LINEAR]->rows * table[_CALIBRATION_BLACK_LEVEL_GR_LINEAR]->cols
					       * table[_CALIBRATION_BLACK_LEVEL_GR_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_BLACK_LEVEL_GB_LINEAR, COMMAND_SET, table[_CALIBRATION_BLACK_LEVEL_GB_LINEAR]->ptr,
					       table[_CALIBRATION_BLACK_LEVEL_GB_LINEAR]->rows * table[_CALIBRATION_BLACK_LEVEL_GB_LINEAR]->cols
					       * table[_CALIBRATION_BLACK_LEVEL_GB_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_BLACK_LEVEL_B_LINEAR, COMMAND_SET, table[_CALIBRATION_BLACK_LEVEL_B_LINEAR]->ptr,
					       table[_CALIBRATION_BLACK_LEVEL_B_LINEAR]->rows * table[_CALIBRATION_BLACK_LEVEL_B_LINEAR]->cols
					       * table[_CALIBRATION_BLACK_LEVEL_B_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_BLACK_LEVEL_R_FS_HDR, COMMAND_SET, table[_CALIBRATION_BLACK_LEVEL_R_FS_HDR]->ptr,
					       table[_CALIBRATION_BLACK_LEVEL_R_FS_HDR]->rows * table[_CALIBRATION_BLACK_LEVEL_R_FS_HDR]->cols
					       * table[_CALIBRATION_BLACK_LEVEL_R_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_BLACK_LEVEL_GR_FS_HDR, COMMAND_SET, table[_CALIBRATION_BLACK_LEVEL_GR_FS_HDR]->ptr,
					       table[_CALIBRATION_BLACK_LEVEL_GR_FS_HDR]->rows * table[_CALIBRATION_BLACK_LEVEL_GR_FS_HDR]->cols
					       * table[_CALIBRATION_BLACK_LEVEL_GR_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_BLACK_LEVEL_GB_FS_HDR, COMMAND_SET, table[_CALIBRATION_BLACK_LEVEL_GB_FS_HDR]->ptr,
					       table[_CALIBRATION_BLACK_LEVEL_GB_FS_HDR]->rows * table[_CALIBRATION_BLACK_LEVEL_GB_FS_HDR]->cols
					       * table[_CALIBRATION_BLACK_LEVEL_GB_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_BLACK_LEVEL_B_FS_HDR, COMMAND_SET, table[_CALIBRATION_BLACK_LEVEL_B_FS_HDR]->ptr,
					       table[_CALIBRATION_BLACK_LEVEL_B_FS_HDR]->rows * table[_CALIBRATION_BLACK_LEVEL_B_FS_HDR]->cols
					       * table[_CALIBRATION_BLACK_LEVEL_B_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_GAMMA_LINEAR, COMMAND_SET, table[_CALIBRATION_GAMMA_LINEAR]->ptr,
					       table[_CALIBRATION_GAMMA_LINEAR]->rows * table[_CALIBRATION_GAMMA_LINEAR]->cols
					       * table[_CALIBRATION_GAMMA_LINEAR]->width, &ret);
			apical_api_calibration(CALIBRATION_GAMMA_FS_HDR, COMMAND_SET, table[_CALIBRATION_GAMMA_FS_HDR]->ptr,
					       table[_CALIBRATION_GAMMA_FS_HDR]->rows * table[_CALIBRATION_GAMMA_FS_HDR]->cols
					       * table[_CALIBRATION_GAMMA_FS_HDR]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_RGB2REC709, COMMAND_SET, table[_CALIBRATION_IRIDIX_RGB2REC709]->ptr,
					       table[_CALIBRATION_IRIDIX_RGB2REC709]->rows * table[_CALIBRATION_IRIDIX_RGB2REC709]->cols
					       * table[_CALIBRATION_IRIDIX_RGB2REC709]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_REC709TORGB, COMMAND_SET, table[_CALIBRATION_IRIDIX_REC709TORGB]->ptr,
					       table[_CALIBRATION_IRIDIX_REC709TORGB]->rows * table[_CALIBRATION_IRIDIX_REC709TORGB]->cols
					       * table[_CALIBRATION_IRIDIX_REC709TORGB]->width, &ret);
			apical_api_calibration(CALIBRATION_IRIDIX_ASYMMETRY, COMMAND_SET, table[_CALIBRATION_IRIDIX_ASYMMETRY]->ptr,
					       table[_CALIBRATION_IRIDIX_ASYMMETRY]->rows * table[_CALIBRATION_IRIDIX_ASYMMETRY]->cols
					       * table[_CALIBRATION_IRIDIX_ASYMMETRY]->width, &ret);
			apical_api_calibration(CALIBRATION_DEFECT_PIXELS, COMMAND_SET, table[_CALIBRATION_DEFECT_PIXELS]->ptr,
					       table[_CALIBRATION_DEFECT_PIXELS]->rows * table[_CALIBRATION_DEFECT_PIXELS]->cols
					       * table[_CALIBRATION_DEFECT_PIXELS]->width, &ret);

			/* green equalization */
			apical_isp_raw_frontend_ge_strength_write(customer->ge_strength);
			apical_isp_raw_frontend_ge_threshold_write(customer->ge_threshold);
			apical_isp_raw_frontend_ge_slope_write(customer->ge_slope);
			apical_isp_raw_frontend_ge_sens_write(customer->ge_sensitivity);

			/* dpc configuration	 */
			apical_isp_raw_frontend_dp_enable_write(customer->dp_module);
			apical_isp_raw_frontend_hpdev_threshold_write(customer->hpdev_threshold);
			apical_isp_raw_frontend_line_thresh_write(customer->line_threshold);
			apical_isp_raw_frontend_hp_blend_write(customer->hp_blend);

			apical_isp_demosaic_vh_slope_write(customer->dmsc_vh_slope);
			apical_isp_demosaic_aa_slope_write(customer->dmsc_aa_slope);
			apical_isp_demosaic_va_slope_write(customer->dmsc_va_slope);
			apical_isp_demosaic_uu_slope_write(customer->dmsc_uu_slope);
			apical_isp_demosaic_sat_slope_write(customer->dmsc_sat_slope);
			apical_isp_demosaic_vh_thresh_write(customer->dmsc_vh_threshold);
			apical_isp_demosaic_aa_thresh_write(customer->dmsc_aa_threshold);
			apical_isp_demosaic_va_thresh_write(customer->dmsc_va_threshold);
			apical_isp_demosaic_uu_thresh_write(customer->dmsc_uu_threshold);
			apical_isp_demosaic_sat_thresh_write(customer->dmsc_sat_threshold);
			apical_isp_demosaic_vh_offset_write(customer->dmsc_vh_offset);
			apical_isp_demosaic_aa_offset_write(customer->dmsc_aa_offset);
			apical_isp_demosaic_va_offset_write(customer->dmsc_va_offset);
			apical_isp_demosaic_uu_offset_write(customer->dmsc_uu_offset);
			apical_isp_demosaic_sat_offset_write(customer->dmsc_sat_offset);
			apical_isp_demosaic_lum_thresh_write(customer->dmsc_luminance_thresh);
			apical_isp_demosaic_np_offset_write(customer->dmsc_np_offset);
			apical_isp_demosaic_dmsc_config_write(customer->dmsc_config);
			apical_isp_demosaic_ac_thresh_write(customer->dmsc_ac_threshold);
			apical_isp_demosaic_ac_slope_write(customer->dmsc_ac_slope);
			apical_isp_demosaic_ac_offset_write(customer->dmsc_ac_offset);
			apical_isp_demosaic_fc_slope_write(customer->dmsc_fc_slope);
			apical_isp_demosaic_fc_alias_slope_write(customer->dmsc_fc_alias_slope);
			apical_isp_demosaic_fc_alias_thresh_write(customer->dmsc_fc_alias_thresh);
			apical_isp_demosaic_np_off_write(customer->dmsc_np_off);
			apical_isp_demosaic_np_off_reflect_write(customer->dmsc_np_reflect);

			apical_isp_temper_recursion_limit_write(customer->temper_recursion_limit);
			apical_isp_frame_stitch_short_thresh_write(customer->wdr_short_thresh);
			apical_isp_frame_stitch_long_thresh_write(customer->wdr_long_thresh);
			apical_isp_frame_stitch_exposure_ratio_write(customer->wdr_expo_ratio_thresh);
			apical_isp_frame_stitch_stitch_correct_write(customer->wdr_stitch_correct);
			apical_isp_frame_stitch_stitch_error_thresh_write(customer->wdr_stitch_error_thresh);
			apical_isp_frame_stitch_stitch_error_limit_write(customer->wdr_stitch_error_limit);
			apical_isp_frame_stitch_black_level_out_write(customer->wdr_stitch_bl_long);
			apical_isp_frame_stitch_black_level_short_write(customer->wdr_stitch_bl_short);
			apical_isp_frame_stitch_black_level_long_write(customer->wdr_stitch_bl_output);

			/* Max ISP Digital Gain */
			api.type = TSYSTEM;
			api.dir = COMMAND_SET;
			api.value = customer->max_isp_dgain;
			api.id = SYSTEM_MAX_ISP_DIGITAL_GAIN;

			status = apical_command(api.type, api.id, api.value, api.dir, &reason);
			if(status != ISP_SUCCESS) {
				ISP_PRINT(ISP_WARNING_LEVEL,"Custom set max isp digital gain failure!reture value is %d,reason is %d\n",status,reason);
			}

			/* Max Sensor Analog Gain */
			api.type = TSYSTEM;
			api.dir = COMMAND_SET;
			api.value = customer->max_sensor_again;
			api.id = SYSTEM_MAX_SENSOR_ANALOG_GAIN;

			status = apical_command(api.type, api.id, api.value, api.dir, &reason);
			if(status != ISP_SUCCESS) {
				ISP_PRINT(ISP_WARNING_LEVEL,"Custom set max isp digital gain failure!reture value is %d,reason is %d\n",status,reason);
			}

			/* modify the node */
			api.type = TIMAGE;
			api.dir = COMMAND_GET;
			api.id = WDR_MODE_ID;
			api.value = -1;
			status = apical_command(api.type, api.id, api.value, api.dir, &reason);
			if(status != ISP_SUCCESS) {
				ISP_PRINT(ISP_WARNING_LEVEL,"Get WDR mode failure!reture value is %d,reason is %d\n",status,reason);
			}

			if (reason == IMAGE_WDR_MODE_LINEAR) {
				stab.global_minimum_sinter_strength = *((uint16_t *)(table[ _CALIBRATION_SINTER_STRENGTH_LINEAR]->ptr) + 1);
				stab.global_maximum_sinter_strength = *((uint16_t *)(table[ _CALIBRATION_SINTER_STRENGTH_LINEAR]->ptr) + table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->rows * table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->cols -1 );

				stab.global_maximum_directional_sharpening = *((uint16_t *)(table[ _CALIBRATION_SHARP_ALT_D_LINEAR]->ptr) + 1);
				stab.global_minimum_directional_sharpening = *((uint16_t *)(table[ _CALIBRATION_SHARP_ALT_D_LINEAR]->ptr) + table[_CALIBRATION_SHARP_ALT_D_LINEAR]->rows * table[_CALIBRATION_SHARP_ALT_D_LINEAR]->cols -1 );

				stab.global_maximum_un_directional_sharpening = *((uint16_t *)(table[ _CALIBRATION_SHARP_ALT_UD_LINEAR]->ptr) + 1);
				stab.global_minimum_un_directional_sharpening = *((uint16_t *)(table[ _CALIBRATION_SHARP_ALT_UD_LINEAR]->ptr) + table[_CALIBRATION_SHARP_ALT_UD_LINEAR]->rows * table[_CALIBRATION_SHARP_ALT_UD_LINEAR]->cols -1 );

				stab.global_maximum_iridix_strength = *(uint8_t *)(table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_LINEAR]->ptr);
			} else if (reason == IMAGE_WDR_MODE_FS_HDR) {
				stab.global_minimum_sinter_strength = *((uint16_t *)(table[ _CALIBRATION_SINTER_STRENGTH_FS_HDR]->ptr) + 1);
				stab.global_maximum_sinter_strength = *((uint16_t *)(table[ _CALIBRATION_SINTER_STRENGTH_FS_HDR]->ptr) + table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->rows * table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->cols -1 );

				stab.global_maximum_directional_sharpening = *((uint16_t *)(table[ _CALIBRATION_SHARP_ALT_D_FS_HDR]->ptr) + 1);
				stab.global_minimum_directional_sharpening = *((uint16_t *)(table[ _CALIBRATION_SHARP_ALT_D_FS_HDR]->ptr) + table[_CALIBRATION_SHARP_ALT_D_LINEAR]->rows * table[_CALIBRATION_SHARP_ALT_D_LINEAR]->cols -1 );

				stab.global_maximum_un_directional_sharpening = *((uint16_t *)(table[ _CALIBRATION_SHARP_ALT_UD_FS_HDR]->ptr) + 1);
				stab.global_minimum_un_directional_sharpening = *((uint16_t *)(table[ _CALIBRATION_SHARP_ALT_UD_FS_HDR]->ptr) + table[_CALIBRATION_SHARP_ALT_UD_LINEAR]->rows * table[_CALIBRATION_SHARP_ALT_UD_LINEAR]->cols -1 );

				stab.global_maximum_iridix_strength = *(uint8_t *)(table[_CALIBRATION_IRIDIX_STRENGTH_MAXIMUM_WDR]->ptr);
			}
			stab.global_minimum_temper_strength = *((uint16_t *)(table[ _CALIBRATION_TEMPER_STRENGTH]->ptr) + 1);
			stab.global_maximum_temper_strength = *((uint16_t *)(table[ _CALIBRATION_TEMPER_STRENGTH]->ptr) + table[_CALIBRATION_TEMPER_STRENGTH]->rows * table[_CALIBRATION_TEMPER_STRENGTH]->cols -1 );
			ctrls->temper_max = *((uint16_t *)(table[ _CALIBRATION_TEMPER_STRENGTH]->ptr) + table[_CALIBRATION_TEMPER_STRENGTH]->rows * table[_CALIBRATION_TEMPER_STRENGTH]->cols -1 );;
			ctrls->temper_min = *((uint16_t *)(table[ _CALIBRATION_TEMPER_STRENGTH]->ptr) + 1);
			stab.global_minimum_iridix_strength = *(uint8_t *)(table[_CALIBRATION_IRIDIX_MIN_MAX_STR]->ptr);

			APICAL_WRITE_32(0x40, tmp_top);
			/* if it is T20,the FR is corresponding to DS2 in bin file. */
			if (customer->top & (1 << 19)){
#if TX_ISP_EXIST_FR_CHANNEL
				apical_isp_top_bypass_fr_gamma_rgb_write(0);
				apical_isp_fr_gamma_rgb_enable_write(1);
#endif
#if TX_ISP_EXIST_DS2_CHANNEL
				apical_isp_top_bypass_ds2_gamma_rgb_write(0);
				apical_isp_ds2_gamma_rgb_enable_write(1);
#endif
			} else {
#if TX_ISP_EXIST_FR_CHANNEL
				apical_isp_top_bypass_fr_gamma_rgb_write(1);
				apical_isp_fr_gamma_rgb_enable_write(0);
#endif
#if TX_ISP_EXIST_DS2_CHANNEL
				apical_isp_top_bypass_ds2_gamma_rgb_write(1);
				apical_isp_ds2_gamma_rgb_enable_write(0);
#endif
			}

			if ((customer->top) & (1 << 20)){
#if TX_ISP_EXIST_FR_CHANNEL
				apical_isp_top_bypass_fr_sharpen_write(0);
				apical_isp_fr_sharpen_enable_write(1);
#endif
#if TX_ISP_EXIST_DS2_CHANNEL
				apical_isp_top_bypass_ds2_sharpen_write(0);
				apical_isp_ds2_sharpen_enable_write(1);
#endif
			} else {
#if TX_ISP_EXIST_FR_CHANNEL
				apical_isp_fr_sharpen_enable_write(1);
				apical_isp_top_bypass_fr_sharpen_write(0);
#endif
#ifdef TX_ISP_EXIST_DS2_CHANNEL
				apical_isp_top_bypass_ds2_sharpen_write(1);
				apical_isp_ds2_sharpen_enable_write(0);
#endif
			}
			if ((customer->top) & (1 << 27))
				apical_isp_ds1_sharpen_enable_write(1);
			else
				apical_isp_ds1_sharpen_enable_write(0);
		}
		ctrls->daynight = dn;
	}
	return ret;
}


static inline int apical_isp_day_or_night_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);
	int ret = ISP_SUCCESS;

	ISP_CORE_MODE_DN_E dn = control->value;
	if(!param){
		ISP_ERROR("Can't get the parameters of isp tuning!\n");
		return -ENOENT;
	}
	if(dn != ctrls->daynight){
		ctrls->daynight = dn;
		core->isp_daynight_switch = 1;
	}
	return ret;
}


static inline int apical_isp_ae_strategy_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	unsigned int reason = 0;
	unsigned int status = 0;

	api.type = TALGORITHMS;
	api.dir = COMMAND_GET;
	api.id = AE_SPLIT_PRESET_ID;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);

	switch(reason){
	case AE_SPLIT_BALANCED:
		control->value = IMPISP_AE_STRATEGY_SPLIT_BALANCED;
		break;
	case AE_SPLIT_INTEGRATION_PRIORITY:
		control->value = IMPISP_AE_STRATEGY_SPLIT_INTEGRATION_PRIORITY;
		break;
	default:
		control->value = IMPISP_AE_STRATEGY_BUTT;
		break;
	}
	if(status != ISP_SUCCESS) {
		ISP_PRINT(ISP_WARNING_LEVEL,"Custom Get AE strategy failure!reture value is %d,reason is %d\n", status, reason);
	}
	return ISP_SUCCESS;
}

static inline int apical_isp_ae_strategy_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	unsigned int reason = 0;
	unsigned int status = 0;
	int ret = ISP_SUCCESS;

	api.type = TALGORITHMS;
	api.id = AE_SPLIT_PRESET_ID;
	api.dir = COMMAND_SET;
	api.value = control->value ? AE_SPLIT_INTEGRATION_PRIORITY : AE_SPLIT_BALANCED;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);

	if(status != ISP_SUCCESS) {
		ISP_PRINT(ISP_WARNING_LEVEL,"Custom Set AE strategy failure!reture value is %d,reason is %d\n",status, reason);
	}
	return ret;
}

static inline int apical_isp_awb_cwf_g_shift(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);
	LookupTable** table = NULL;
	ISP_CORE_MODE_DN_E dn;
	uint16_t rgain = 0;
	uint16_t bgain = 0;

	dn = ctrls->daynight;
	if(param){
		table = param->isp_param[dn].calibrations;

		rgain = *(uint16_t *)(table[_CALIBRATION_LIGHT_SRC]->ptr);
		bgain = *((uint16_t *)(table[_CALIBRATION_LIGHT_SRC]->ptr) + 1);
		control->value = (rgain << 16) + bgain;
	}
	return 0;
}

static inline int apical_isp_awb_cwf_s_shift(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);
	LookupTable** table = NULL;
	ISP_CORE_MODE_DN_E dn;
	int ret = ISP_SUCCESS;

	dn = ctrls->daynight;
	if(param){
		table = param->isp_param[dn].calibrations;

		*(uint16_t *)(table[_CALIBRATION_LIGHT_SRC]->ptr) = control->value >> 16;
		*((uint16_t *)(table[_CALIBRATION_LIGHT_SRC]->ptr) + 1) = control->value & 0xffff;
		apical_api_calibration(CALIBRATION_LIGHT_SRC,COMMAND_SET, table[_CALIBRATION_LIGHT_SRC]->ptr,
				       table[_CALIBRATION_LIGHT_SRC]->rows * table[_CALIBRATION_LIGHT_SRC]->cols
				       * table[_CALIBRATION_LIGHT_SRC]->width, &ret);
	}
	return ret;
}

static inline int apical_isp_sat_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct v4l2_mbus_framefmt *mbus = &core->vin.mbus;
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	unsigned int value = 128;
	unsigned int mid_node = 128;
	int ret = ISP_SUCCESS;

	if(mbus->code == V4L2_MBUS_FMT_YUYV8_1X16){
		return ret;
	}
	/* the original value */
	tuning->ctrls.saturation = control->value & 0xff;
	value = tuning->ctrls.saturation;
	if(param == NULL)
		goto set_saturation_val;

	if(tuning->ctrls.daynight == ISP_CORE_RUNING_MODE_DAY_MODE){
		mid_node = param->customer[TX_ISP_PRIV_PARAM_DAY_MODE].saturation;
	}else{
		mid_node = param->customer[TX_ISP_PRIV_PARAM_NIGHT_MODE].saturation;
	}

	/* the amended value */
	value = value < 128 ? (value * mid_node / 128) : (mid_node + (0xff - mid_node) * (value - 128) / 128);

set_saturation_val:
	api.type = TSCENE_MODES;
	api.dir = COMMAND_SET;
	api.id = SATURATION_STRENGTH_ID;
	api.value = value;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	return ret;
}

static inline int apical_isp_bright_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct v4l2_mbus_framefmt *mbus = &core->vin.mbus;
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	unsigned int value = 128;
	unsigned int mid_node = 128;
	int ret = ISP_SUCCESS;

	if(mbus->code == V4L2_MBUS_FMT_YUYV8_1X16){
		return ret;
	}
	/* the original value */
	tuning->ctrls.brightness = control->value & 0xff;
	value = tuning->ctrls.brightness;

	if(param == NULL)
		goto set_bright_val;

	if(tuning->ctrls.daynight == ISP_CORE_RUNING_MODE_DAY_MODE){
		mid_node = param->customer[TX_ISP_PRIV_PARAM_DAY_MODE].brightness;
	}else{
		mid_node = param->customer[TX_ISP_PRIV_PARAM_NIGHT_MODE].brightness;
	}

	/* the amended value */
	value = value < 128 ? (value * mid_node / 128) : (mid_node + (0xff - mid_node) * (value - 128) / 128);

set_bright_val:
	api.type = TSCENE_MODES;
	api.dir = COMMAND_SET;
	api.id = BRIGHTNESS_STRENGTH_ID;
	api.value = value;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	return ret;
}

static inline int apical_isp_contrast_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct v4l2_mbus_framefmt *mbus = &core->vin.mbus;
	unsigned char (*curves)[2] = NULL;
	unsigned int value = 128;
	int x1, x2;
	unsigned int y1,y2,v1,v2;
	unsigned int total_gain;
	apical_api_control_t api;
	int i = 0;
	unsigned char status = 0;
	int reason = 0;
	int ret = ISP_SUCCESS;

	if(mbus->code == V4L2_MBUS_FMT_YUYV8_1X16){
		return ret;
	}

	tuning->ctrls.contrast = control->value & 0xff;
	value = tuning->ctrls.contrast;

	if(param == NULL)
		goto set_contrast_val;
	/* total_gain format 27.5; for example, the value is 170, is '101.01010'b, is 2^5 + (10 / 32), is 32.2125x */
	total_gain =  stab.global_sensor_analog_gain  + stab.global_sensor_digital_gain + stab.global_isp_digital_gain;

	total_gain = math_exp2(total_gain,5,5);

	if(tuning->ctrls.daynight == ISP_CORE_RUNING_MODE_DAY_MODE){
		curves = param->customer[TX_ISP_PRIV_PARAM_DAY_MODE].contrast;
	}else{
		curves = param->customer[TX_ISP_PRIV_PARAM_NIGHT_MODE].contrast;
	}

	/* When curve[0][0] == 0xff, the curves is invalid. */
	if(curves[0][0] != 0xff){
		for(i = 0; i < CONTRAST_CURVES_MAXNUM; i++){
			if((curves[i][0] == 0xff) || ((curves[i][0] << 5) >= total_gain))
				break;
		}
		if(curves[i][0] == 0xff){
			x1 = i - 1;
			x2 = i - 1;
		}else{
			x2 = i;
			x1 = x2 - 1 < 0 ? 0 : x2 - 1;
		}

		if(x1 == x2){
			y1 = curves[x1][1];
			value = value < 128 ? (value * y1 / 128) : (y1 + (0xff - y1) * (value - 128) / 128);
		}else{
			y1 = curves[x1][1];
			y2 = curves[x2][1];
			v1 = value < 128 ? (value * y1 / 128) : (y1 + (0xff - y1) * (value - 128) / 128);
			v2 = value < 128 ? (value * y2 / 128) : (y2 + (0xff - y2) * (value - 128) / 128);

			x1 = curves[x1][0];
			x2 = curves[x2][0];
			x1 = x1 << 5;
			x2 = x2 << 5;

			value = ((total_gain - x1) * v2 + (x2 - total_gain) * v1) / (x2 - x1);
		}
	}
set_contrast_val:
	api.type = TSCENE_MODES;
	api.dir = COMMAND_SET;
	api.id = CONTRAST_STRENGTH_ID;
	api.value = value;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	return ret;
}

static inline int apical_isp_fps_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	unsigned int fps = control->value;
	int ret = 0;

	if(fps != core->vin.fps){
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SENSOR_FPS, &fps);
		if (ret != ISP_SUCCESS) {
			printk("Failed to set fps!!!!\n");
			return ret;
		}
		api.type = TIMAGE;
		api.dir = COMMAND_SET;
		api.id = SENSOR_FPS_MODE_ID;
		api.value = FPS25;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		tuning->ctrls.fps = fps;
	}
	return 0;
}

static inline int apical_isp_fps_g_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	control->value = core->vin.fps;

	return ISP_SUCCESS;
}

static inline int apical_isp_fc_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_false_color_attr *attr = &tuning->ctrls.fc_attr;

	return ISP_SUCCESS;
	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
	apical_isp_demosaic_fc_slope_write(attr->strength);
	apical_isp_demosaic_fc_alias_slope_write(attr->alias_strength);
	apical_isp_demosaic_fc_alias_thresh_write(attr->alias_thresh);
	return ISP_SUCCESS;
}

static inline int apical_isp_sharp_s_control(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;
	unsigned int value = 128;
	unsigned int mid_node = 128;
	int ret = ISP_SUCCESS;

	/* the original value */
	tuning->ctrls.sharpness = control->value;
	value = tuning->ctrls.sharpness;

	if(param == NULL)
		goto set_sharpness_val;

	if(tuning->ctrls.daynight == ISP_CORE_RUNING_MODE_DAY_MODE){
		mid_node = param->customer[TX_ISP_PRIV_PARAM_DAY_MODE].sharpness;
	}else{
		mid_node = param->customer[TX_ISP_PRIV_PARAM_NIGHT_MODE].sharpness;
	}

	/* the amended value */
	value = value < 128 ? (value * mid_node / 128) : (mid_node + (0xff - mid_node) * (value - 128) / 128);

set_sharpness_val:
	api.type = TSCENE_MODES;
	api.dir = COMMAND_SET;
	api.id = SHARPENING_STRENGTH_ID;
	api.value = value;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	return ret;
}

static inline int apical_isp_sharp_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_sharpness_attr *attr = &tuning->ctrls.sharp_attr;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
#if 1
	apical_isp_fr_sharpen_strength_write(attr->target_sharp);
	apical_isp_ds1_sharpen_strength_write(attr->target_sharp);
	apical_isp_ds2_sharpen_strength_write(attr->target_sharp);
#else
	apical_isp_demosaic_sharp_alt_d_write(attr->demo_strength_d);
	apical_isp_demosaic_sharp_alt_ud_write(attr->demo_strength_ud);
	apical_isp_demosaic_lum_thresh_write(attr->demo_threshold);
	apical_isp_fr_sharpen_enable_write(attr->fr_enable);
	apical_isp_ds1_sharpen_enable_write(attr->ds1_enable);
	apical_isp_ds2_sharpen_enable_write(attr->ds2_enable);
	if(attr->fr_enable){
		apical_isp_fr_sharpen_strength_write(attr->fr_strength);
	}
	if(attr->ds1_enable){
		apical_isp_ds1_sharpen_strength_write(attr->ds1_strength);
	}
	if(attr->ds2_enable){
		apical_isp_ds2_sharpen_strength_write(attr->ds2_strength);
	}
#endif
	return ISP_SUCCESS;
}

static inline int apical_isp_demosaic_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_demosaic_attr *attr = &tuning->ctrls.demo_attr;

	copy_from_user(attr, (const void __user *)control->value, sizeof(*attr));
	apical_isp_demosaic_vh_slope_write(attr->vh_slope);
	apical_isp_demosaic_aa_slope_write(attr->aa_slope);
	apical_isp_demosaic_va_slope_write(attr->va_slope);
	apical_isp_demosaic_uu_slope_write(attr->uu_slope);
	apical_isp_demosaic_sat_slope_write(attr->sat_slope);
	apical_isp_demosaic_vh_thresh_write(attr->vh_thresh);
	apical_isp_demosaic_aa_thresh_write(attr->aa_thresh);
	apical_isp_demosaic_va_thresh_write(attr->va_thresh);
	apical_isp_demosaic_uu_thresh_write(attr->uu_thresh);
	apical_isp_demosaic_sat_thresh_write(attr->sat_thresh);
	apical_isp_demosaic_vh_offset_write(attr->vh_offset);
	apical_isp_demosaic_aa_offset_write(attr->aa_offset);
	apical_isp_demosaic_va_offset_write(attr->va_offset);
	apical_isp_demosaic_uu_offset_write(attr->uu_offset);
	apical_isp_demosaic_sat_offset_write(attr->sat_offset);
	apical_isp_demosaic_dmsc_config_write(attr->dmsc_config);

	return ISP_SUCCESS;
}

static int apical_isp_gamma_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	int ret = ISP_SUCCESS;
	struct isp_core_gamma_attr attr;
	int i = 0;

	for (i = 0; i < 129; i++) {
		attr.gamma[i] = APICAL_READ_32(0x10400+4*i);
	}
	copy_to_user((void __user*)control->value, (const void *)&attr, sizeof(attr));
	return ret;
}

static int apical_isp_stab_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_stab_attr stab_attr;
	memset(&stab_attr, 0, sizeof(stab_attr));
	memcpy(&stab_attr.stab, &stab, sizeof(stab));
	copy_to_user((void __user*)control->value, &stab_attr, sizeof(stab_attr));
	return 0;
}

#define SYSTEM_TAB_ITEM(x) if (stab_attr.stab_ctrl.ctrl_##x) stab.x = stab_attr.stab.x
static int apical_isp_stab_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_stab_attr stab_attr;
	copy_from_user(&stab_attr, (const void __user*)control->value, sizeof(stab_attr));
	SYSTEM_TAB_ITEM(global_freeze_firmware);
	SYSTEM_TAB_ITEM(global_manual_exposure);
	SYSTEM_TAB_ITEM(global_manual_exposure_ratio);
	SYSTEM_TAB_ITEM(global_manual_integration_time);
	SYSTEM_TAB_ITEM(global_manual_sensor_analog_gain);
	SYSTEM_TAB_ITEM(global_manual_sensor_digital_gain);
	SYSTEM_TAB_ITEM(global_manual_isp_digital_gain);
	SYSTEM_TAB_ITEM(global_manual_directional_sharpening);
	SYSTEM_TAB_ITEM(global_manual_un_directional_sharpening);
	SYSTEM_TAB_ITEM(global_manual_iridix);
	SYSTEM_TAB_ITEM(global_manual_sinter);
	SYSTEM_TAB_ITEM(global_manual_temper);
	SYSTEM_TAB_ITEM(global_manual_awb);
	SYSTEM_TAB_ITEM(global_antiflicker_enable);
	SYSTEM_TAB_ITEM(global_slow_frame_rate_enable);
	SYSTEM_TAB_ITEM(global_manual_saturation);
	SYSTEM_TAB_ITEM(global_manual_exposure_time);
	SYSTEM_TAB_ITEM(global_exposure_dark_target);
	SYSTEM_TAB_ITEM(global_exposure_bright_target);
	SYSTEM_TAB_ITEM(global_exposure_ratio);
	SYSTEM_TAB_ITEM(global_max_exposure_ratio);
	SYSTEM_TAB_ITEM(global_integration_time);
	SYSTEM_TAB_ITEM(global_max_integration_time);
	SYSTEM_TAB_ITEM(global_sensor_analog_gain);
	SYSTEM_TAB_ITEM(global_max_sensor_analog_gain);
	SYSTEM_TAB_ITEM(global_sensor_digital_gain);
	SYSTEM_TAB_ITEM(global_max_sensor_digital_gain);
	SYSTEM_TAB_ITEM(global_isp_digital_gain);
	SYSTEM_TAB_ITEM(global_max_isp_digital_gain);
	SYSTEM_TAB_ITEM(global_directional_sharpening_target);
	SYSTEM_TAB_ITEM(global_maximum_directional_sharpening);
	SYSTEM_TAB_ITEM(global_minimum_directional_sharpening);
	SYSTEM_TAB_ITEM(global_un_directional_sharpening_target);
	SYSTEM_TAB_ITEM(global_maximum_un_directional_sharpening);
	SYSTEM_TAB_ITEM(global_minimum_un_directional_sharpening);
	SYSTEM_TAB_ITEM(global_iridix_strength_target);
	SYSTEM_TAB_ITEM(global_maximum_iridix_strength);
	SYSTEM_TAB_ITEM(global_minimum_iridix_strength);
	SYSTEM_TAB_ITEM(global_sinter_threshold_target);
	SYSTEM_TAB_ITEM(global_maximum_sinter_strength);
	SYSTEM_TAB_ITEM(global_minimum_sinter_strength);
	SYSTEM_TAB_ITEM(global_temper_threshold_target);
	SYSTEM_TAB_ITEM(global_maximum_temper_strength);
	SYSTEM_TAB_ITEM(global_minimum_temper_strength);
	SYSTEM_TAB_ITEM(global_awb_red_gain);
	SYSTEM_TAB_ITEM(global_awb_blue_gain);
	SYSTEM_TAB_ITEM(global_saturation_target);
	SYSTEM_TAB_ITEM(global_anti_flicker_frequency);
	SYSTEM_TAB_ITEM(global_ae_compensation);
	SYSTEM_TAB_ITEM(global_calibrate_bad_pixels);
	SYSTEM_TAB_ITEM(global_dis_x);
	SYSTEM_TAB_ITEM(global_dis_y);
	return 0;
}

static int apical_isp_gamma_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	int ret = ISP_SUCCESS;
	struct isp_core_gamma_attr attr;

	if (0 == control->value) {
		apical_api_calibration(CALIBRATION_GAMMA_LINEAR, COMMAND_GET, attr.gamma, sizeof(attr.gamma), &ret);
		if (ret != ISP_SUCCESS)
			goto err_get_def_gamma;
	} else {
		copy_from_user(&attr, (const void __user*)control->value, sizeof(attr));
	}
	apical_api_calibration(CALIBRATION_GAMMA_LINEAR, COMMAND_SET, attr.gamma, sizeof(attr.gamma), &ret);
	if (ret != ISP_SUCCESS)
		goto err_set_def_gamma;
	return ret;

err_set_def_gamma:
err_get_def_gamma:
	return ret;
}

static int apical_isp_ae_weight_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_weight_attr attr;
	unsigned int row,col;

	copy_from_user(&attr, (const void __user*)control->value, sizeof(attr));

	for (row = 0; row < 15; row++){
		for (col = 0; col < 15; col++){
			apical_isp_zones_aexp_weight_write(row, col, (attr.weight[row][col]));
		}
	}

	return 0;
}

static int apical_isp_ae_weight_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_weight_attr attr;
	unsigned int row,col;

	for (row = 0; row < 15; row++){
		for (col = 0; col < 15; col++){
			attr.weight[row][col] = apical_isp_zones_aexp_weight_read(row,col);
		}
	}

	copy_to_user((void __user*)control->value, &attr, sizeof(attr));

	return 0;
}

static int apical_isp_ae_hist_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_ae_sta_info info;

	info.ae_histhresh[0] = apical_isp_metering_hist_thresh_0_1_read();
	info.ae_histhresh[1] = apical_isp_metering_hist_thresh_1_2_read();
	info.ae_histhresh[2] = apical_isp_metering_hist_thresh_3_4_read();
	info.ae_histhresh[3] = apical_isp_metering_hist_thresh_4_5_read();

	info.ae_hist[0] = apical_isp_metering_hist_0_read();
	info.ae_hist[1] = apical_isp_metering_hist_1_read();
	info.ae_hist[3] = apical_isp_metering_hist_3_read();
	info.ae_hist[4] = apical_isp_metering_hist_4_read();
	info.ae_hist[2] = 0xffff - info.ae_hist[0] - info.ae_hist[1] - info.ae_hist[3] - info.ae_hist[4];

	info.ae_stat_nodeh = apical_isp_metering_aexp_nodes_used_horiz_read();
	info.ae_stat_nodev = apical_isp_metering_aexp_nodes_used_vert_read();

	copy_to_user((void __user*)control->value, &info, sizeof(info));
	return 0;
}

static int apical_isp_ae_hist_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_ae_sta_info info;

	copy_from_user(&info, (const void __user*)control->value, sizeof(info));

	apical_isp_metering_hist_thresh_0_1_write(info.ae_histhresh[0]);
	apical_isp_metering_hist_thresh_1_2_write(info.ae_histhresh[1]);
	apical_isp_metering_hist_thresh_3_4_write(info.ae_histhresh[2]);
	apical_isp_metering_hist_thresh_4_5_write(info.ae_histhresh[3]);

	apical_isp_metering_aexp_nodes_used_horiz_write(info.ae_stat_nodeh);
	apical_isp_metering_aexp_nodes_used_vert_write(info.ae_stat_nodev);

	return 0;
}

static int apical_isp_awb_hist_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_awb_sta_info info;

	info.awb_stat.r_gain = apical_isp_metering_awb_rg_read();
	info.awb_stat.b_gain = apical_isp_metering_awb_bg_read();
	info.awb_stat.awb_sum = apical_isp_metering_awb_sum_read();

	info.awb_stats_mode = apical_isp_metering_awb_stats_mode_read()?ISPCORE_AWB_STATS_CURRENT_MODE:ISPCORE_AWB_STATS_LEGACY_MODE;
	info.awb_whitelevel = apical_isp_metering_white_level_awb_read();
	info.awb_blacklevel = apical_isp_metering_black_level_awb_read();
	info.cr_ref_max = apical_isp_metering_cr_ref_max_awb_read();
	info.cr_ref_min = apical_isp_metering_cr_ref_min_awb_read();
	info.cb_ref_max = apical_isp_metering_cb_ref_max_awb_read();
	info.cb_ref_min = apical_isp_metering_cb_ref_min_awb_read();
	info.awb_stat_nodeh = apical_isp_metering_awb_nodes_used_horiz_read();
	info.awb_stat_nodev = apical_isp_metering_awb_nodes_used_vert_read();
	/* info.cr_ref_high = apical_isp_metering_cr_ref_high_awb_read(); */
	/* info.cr_ref_low = apical_isp_metering_cr_ref_low_awb_read(); */
	/* info.cb_ref_high = apical_isp_metering_cb_ref_high_awb_read(); */
	/* info.cb_ref_low = apical_isp_metering_cb_ref_low_awb_read(); */

	copy_to_user((void __user*)control->value, &info, sizeof(info));

	return 0;
}

static int apical_isp_awb_hist_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_awb_sta_info info;
	copy_from_user(&info, (const void __user*)control->value, sizeof(info));

	apical_isp_metering_awb_stats_mode_write(info.awb_stats_mode?1:0);
	apical_isp_metering_white_level_awb_write(info.awb_whitelevel);
	apical_isp_metering_black_level_awb_write(info.awb_blacklevel);
	apical_isp_metering_cr_ref_max_awb_write(info.cr_ref_max);
	apical_isp_metering_cr_ref_min_awb_write(info.cr_ref_min);
	apical_isp_metering_cb_ref_max_awb_write(info.cb_ref_max);
	apical_isp_metering_cb_ref_min_awb_write(info.cb_ref_min);
	apical_isp_metering_awb_nodes_used_horiz_write(info.awb_stat_nodeh);
	apical_isp_metering_awb_nodes_used_vert_write(info.awb_stat_nodev);
	/* apical_isp_metering_cr_ref_high_awb_write(info.cr_ref_high); */
	/* apical_isp_metering_cr_ref_low_awb_write(info.cr_ref_low); */
	/* apical_isp_metering_cb_ref_high_awb_write(info.cb_ref_high); */
	/* apical_isp_metering_cb_ref_low_awb_write(info.cb_ref_low); */

	return 0;
}

static int apical_isp_af_hist_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_af_sta_info info;

	info.af_stat.af_metrics = apical_isp_metering_af_metrics_read();
	info.af_stat.af_metrics_alt = apical_isp_metering_af_metrics_alt_read();
	info.af_stat.af_thresh_read = apical_isp_metering_af_threshold_read_read();
	info.af_stat.af_intensity_read = apical_isp_metering_af_intensity_read_read();
	info.af_stat.af_intensity_zone = apical_isp_metering_af_intensity_zone_read_read();
	info.af_stat.af_total_pixels = apical_isp_metering_total_pixels_read();
	info.af_stat.af_counted_pixels = apical_isp_metering_counted_pixels_read();

	info.af_metrics_shift = apical_isp_metering_af_metrics_shift_read();
	info.af_thresh = apical_isp_metering_af_threshold_write_read();
	info.af_thresh_alt = apical_isp_metering_af_threshold_alt_write_read();
	info.af_stat_nodeh = apical_isp_metering_af_nodes_used_horiz_read();
	info.af_stat_nodev = apical_isp_metering_af_nodes_used_vert_read();
	info.af_np_offset = apical_isp_metering_af_np_offset_read();
	info.af_intensity_mode = apical_isp_metering_af_intensity_norm_mode_read();
	info.af_skipx = apical_isp_metering_skip_x_read();
	info.af_offsetx = apical_isp_metering_offset_x_read();
	info.af_skipy = apical_isp_metering_skip_y_read();
	info.af_offsety = apical_isp_metering_offset_y_read();
	info.af_scale_top = apical_isp_metering_scale_top_read();
	info.af_scale_bottom = apical_isp_metering_scale_bottom_read();

	copy_to_user((void __user*)control->value, &info, sizeof(info));

	return 0;
}

static int apical_isp_af_hist_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_af_sta_info info;
	copy_from_user(&info, (const void __user*)control->value, sizeof(info));

	apical_isp_metering_af_metrics_shift_write(info.af_metrics_shift);
	apical_isp_metering_af_threshold_write_write(info.af_thresh);
	apical_isp_metering_af_threshold_alt_write_write(info.af_thresh_alt);
	apical_isp_metering_af_nodes_used_horiz_write(info.af_stat_nodeh);
	apical_isp_metering_af_nodes_used_vert_write(info.af_stat_nodev);
	apical_isp_metering_af_np_offset_write(info.af_np_offset);
	apical_isp_metering_af_intensity_norm_mode_write(info.af_intensity_mode);
	apical_isp_metering_skip_x_write(info.af_skipx);
	apical_isp_metering_offset_x_write(info.af_offsetx);
	apical_isp_metering_skip_y_write(info.af_skipy);
	apical_isp_metering_offset_y_write(info.af_offsety);
	apical_isp_metering_scale_top_write(info.af_scale_top);
	apical_isp_metering_scale_bottom_write(info.af_scale_bottom);

	return 0;
}

static int apical_isp_awb_weight_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_weight_attr attr;
	unsigned int row,col;

	copy_from_user(&attr, (const void __user*)control->value, sizeof(attr));

	for (row = 0; row < 15; row++){
		for (col = 0; col < 15; col++){
			apical_isp_zones_awb_weight_write(row, col, (attr.weight[row][col]));
		}
	}

	return 0;
}

static int apical_isp_awb_weight_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_weight_attr attr;
	unsigned int row,col;

	for (row = 0; row < 15; row++){
		for (col = 0; col < 15; col++){
			attr.weight[row][col] = apical_isp_zones_awb_weight_read(row,col);
		}
	}

	copy_to_user((void __user*)control->value, &attr, sizeof(attr));

	return 0;
}

static int apical_isp_ae_comp_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	unsigned char status = 0;
	int reason = 0;

	apical_api_control_t api;
	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;

	api.id = AE_COMPENSATION_ID;
	api.value = control->value;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

static int apical_isp_ae_comp_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	unsigned char status = 0;
	int reason = 0;
	apical_api_control_t api;
	api.type = TALGORITHMS;
	api.dir = COMMAND_GET;

	api.id = AE_COMPENSATION_ID;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		return -1;
	}
	control->value = reason;

	return 0;
}

static int apical_isp_expr_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor_attribute *attr = core->vin.attr;
	union isp_core_expr_attr expr_attr;
	apical_api_control_t api;
	int mode = 0;
	unsigned int integration_time = 0;

	unsigned char status = 0;
	int reason = 0;

	copy_from_user(&expr_attr, (const void __user*)control->value, sizeof(expr_attr));

	if (expr_attr.s_attr.mode == ISP_CORE_EXPR_MODE_AUTO) {
		mode = 0;
	} else {
		mode = 1;
	}

	if ((expr_attr.s_attr.unit == ISP_CORE_EXPR_UNIT_US)&&(1 == mode)) {
		if (attr->one_line_expr_in_us != 0)
			integration_time = expr_attr.s_attr.time/attr->one_line_expr_in_us;
		else {
			printk("err: %s,%d one_line_expr_in_us = %d \n", __func__, __LINE__, attr->one_line_expr_in_us);
			goto err_one_line_expr_in_us;
		}
	}

	api.type = TSYSTEM;
	api.dir = COMMAND_SET;
	api.id = SYSTEM_MANUAL_INTEGRATION_TIME;
	api.value = mode;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_set_expr_mode;
	}

	if (mode) {
		api.type = TSYSTEM;
		api.dir = COMMAND_SET;
		api.id = SYSTEM_INTEGRATION_TIME;
		api.value = integration_time;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		if (status != ISP_SUCCESS) {
			printk("err: %s,%d apical_command err \n", __func__, __LINE__);
			goto err_set_integration_time;
		}
	}
	return 0;

err_set_integration_time:
err_set_expr_mode:
err_one_line_expr_in_us:
	return -1;
}

static int apical_isp_expr_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	struct tx_isp_sensor_attribute *attr = core->vin.attr;
	union isp_core_expr_attr expr_attr;
	apical_api_control_t api;
	int mode = 0;
	unsigned int integration_time = 0;

	unsigned char status = 0;
	int reason = 0;

	api.type = TSYSTEM;
	api.dir = COMMAND_GET;
	api.id = SYSTEM_MANUAL_INTEGRATION_TIME;
	api.value = mode;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_expr_mode;
	}
	mode = reason;

	api.type = TSYSTEM;
	api.dir = COMMAND_GET;
	api.id = SYSTEM_INTEGRATION_TIME;
	api.value = integration_time;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_integration_time;
	}
	integration_time = reason;

	expr_attr.g_attr.mode = (0 == mode)?ISP_CORE_EXPR_MODE_AUTO:ISP_CORE_EXPR_MODE_MANUAL;
	expr_attr.g_attr.integration_time = integration_time;
	expr_attr.g_attr.integration_time_min = attr->min_integration_time;
	expr_attr.g_attr.integration_time_max = attr->max_integration_time;
	expr_attr.g_attr.one_line_expr_in_us = attr->one_line_expr_in_us;

	copy_to_user((void __user*)control->value, &expr_attr, sizeof(expr_attr));

	return 0;

err_get_integration_time:
err_get_expr_mode:
	return 0;
}

static int apical_isp_ae_g_roi(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;

	api.type = TALGORITHMS;
	api.dir = COMMAND_GET;
	api.id = AE_ROI_ID;
	api.value = 0;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	control->value = reason;
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		return -1;
	}
	return 0;
}

static int apical_isp_ae_s_roi(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	apical_api_control_t api;
	unsigned char status = 0;
	int reason = 0;

	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;
	api.id = AE_ROI_ID;
	api.value = control->value;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		return -1;
	}
	return 0;
}

static int isp_wb_mode_to_apical(enum isp_core_wb_mode mode)
{
	int apical_mode = -1;
	switch(mode) {
	case ISP_CORE_WB_MODE_AUTO:
		apical_mode = AWB_AUTO;
		break;
	case ISP_CORE_WB_MODE_MANUAL:
		apical_mode = AWB_MANUAL;
		break;
	case ISP_CORE_WB_MODE_DAY_LIGHT:
		apical_mode = AWB_DAY_LIGHT;
		break;
	case ISP_CORE_WB_MODE_CLOUDY:
		apical_mode = AWB_CLOUDY;
		break;
	case ISP_CORE_WB_MODE_INCANDESCENT:
		apical_mode = AWB_INCANDESCENT;
		break;
	case ISP_CORE_WB_MODE_FLOURESCENT:
		apical_mode = AWB_FLOURESCENT;
		break;
	case ISP_CORE_WB_MODE_TWILIGHT:
		apical_mode = AWB_TWILIGHT;
		break;
	case ISP_CORE_WB_MODE_SHADE:
		apical_mode = AWB_SHADE;
		break;
	case ISP_CORE_WB_MODE_WARM_FLOURESCENT:
		apical_mode = AWB_WARM_FLOURESCENT;
		break;
	default:
		break;
	}
	return apical_mode;
}

static int apical_wb_mode_to_isp(int apical_mode)
{
	int isp_mode = -1;
	switch(apical_mode) {
	case AWB_AUTO:
		isp_mode = ISP_CORE_WB_MODE_AUTO;
		break;
	case AWB_MANUAL:
		isp_mode = ISP_CORE_WB_MODE_MANUAL;
		break;
	case AWB_DAY_LIGHT:
		isp_mode = ISP_CORE_WB_MODE_DAY_LIGHT;
		break;
	case AWB_CLOUDY:
		isp_mode = ISP_CORE_WB_MODE_CLOUDY;
		break;
	case AWB_INCANDESCENT:
		isp_mode = ISP_CORE_WB_MODE_INCANDESCENT;
		break;
	case AWB_FLOURESCENT:
		isp_mode = ISP_CORE_WB_MODE_FLOURESCENT;
		break;
	case AWB_TWILIGHT:
		isp_mode = ISP_CORE_WB_MODE_TWILIGHT;
		break;
	case AWB_SHADE:
		isp_mode = ISP_CORE_WB_MODE_SHADE;
		break;
	case AWB_WARM_FLOURESCENT:
		isp_mode = ISP_CORE_WB_MODE_WARM_FLOURESCENT;
		break;
	default:
		break;
	}
	return isp_mode;
}

static int apical_isp_wb_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_wb_attr wb_attr;
	apical_api_control_t api;
	int apical_mode = 0;
	int manual = 0;

	unsigned char status = 0;
	int reason = 0;

	copy_from_user(&wb_attr, (const void __user*)control->value, sizeof(wb_attr));

	apical_mode = isp_wb_mode_to_apical(wb_attr.mode);
	if (-1 == apical_mode) {
		printk("err: %s,%d mode = %d \n", __func__, __LINE__, wb_attr.mode);
		goto err_mode;
	}
	if (wb_attr.mode == ISP_CORE_WB_MODE_MANUAL) {
		manual = 1;
	} else {
		manual = 0;
	}
	api.type = TALGORITHMS;
	api.dir = COMMAND_SET;
	api.id = AWB_MODE_ID;
	api.value = apical_mode;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_set_wb_mode;
	}
	if (manual) {
		api.type = TSYSTEM;
		api.dir = COMMAND_SET;
		api.id = SYSTEM_AWB_RED_GAIN;
		api.value = wb_attr.rgain;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		if (status != ISP_SUCCESS) {
			printk("err: %s,%d apical_command err \n", __func__, __LINE__);
			goto err_set_wb_rgain;
		}
		api.type = TSYSTEM;
		api.dir = COMMAND_SET;
		api.id = SYSTEM_AWB_BLUE_GAIN;
		api.value = wb_attr.bgain;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		if (status != ISP_SUCCESS) {
			printk("err: %s,%d apical_command err \n", __func__, __LINE__);
			goto err_set_wb_bgain;
		}
	}
	return 0;

err_set_wb_bgain:
err_set_wb_rgain:
err_set_wb_mode:
err_mode:
	return -1;
}

static int apical_isp_wb_statis_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_wb_attr wb_attr;

	wb_attr.rgain = apical_isp_metering_awb_rg_read();
	wb_attr.bgain = apical_isp_metering_awb_bg_read();
	control->value = (wb_attr.rgain << 16) + wb_attr.bgain;
	return 0;
}

static int apical_isp_rgb_coefft_wb_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_rgb_coefft_wb_attr rgb_coefft_wb_attr;

	copy_from_user(&rgb_coefft_wb_attr, (const void __user*)control->value, sizeof(rgb_coefft_wb_attr));
	apical_isp_matrix_rgb_coefft_wb_r_write(rgb_coefft_wb_attr.rgb_coefft_wb_r);
	apical_isp_matrix_rgb_coefft_wb_g_write(rgb_coefft_wb_attr.rgb_coefft_wb_g);
	apical_isp_matrix_rgb_coefft_wb_b_write(rgb_coefft_wb_attr.rgb_coefft_wb_b);
	return 0;
}


static int apical_isp_rgb_coefft_wb_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_rgb_coefft_wb_attr rgb_coefft_wb_attr;

	rgb_coefft_wb_attr.rgb_coefft_wb_r = apical_isp_matrix_rgb_coefft_wb_r_read();
	rgb_coefft_wb_attr.rgb_coefft_wb_g = apical_isp_matrix_rgb_coefft_wb_g_read();
	rgb_coefft_wb_attr.rgb_coefft_wb_b = apical_isp_matrix_rgb_coefft_wb_b_read();
	copy_to_user((void __user*)control->value, &rgb_coefft_wb_attr, sizeof(rgb_coefft_wb_attr));
	return 0;
}

static int apical_isp_wb_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_wb_attr wb_attr;
	apical_api_control_t api;

	unsigned char status = 0;
	int reason = 0;
	int isp_mode = 0;

	api.type = TALGORITHMS;
	api.dir = COMMAND_GET;
	api.id = AWB_MODE_ID;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_wb_mode;
	}
	isp_mode = apical_wb_mode_to_isp(reason);
	if (-1 == isp_mode) {
		printk("err: %s,%d isp wb mode :%d  \n", __func__, __LINE__, isp_mode);
		goto err_get_wb_mode;
	}
	wb_attr.mode = isp_mode;

	{
		api.type = TSYSTEM;
		api.dir = COMMAND_GET;
		api.id = SYSTEM_AWB_RED_GAIN;
		api.value = -1;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		if (status != ISP_SUCCESS) {
			printk("err: %s,%d apical_command err \n", __func__, __LINE__);
			goto err_get_wb_rgain;
		}
		wb_attr.rgain = reason;

		api.type = TSYSTEM;
		api.dir = COMMAND_GET;
		api.id = SYSTEM_AWB_BLUE_GAIN;
		api.value = -1;
		status = apical_command(api.type, api.id, api.value, api.dir, &reason);
		if (status != ISP_SUCCESS) {
			printk("err: %s,%d apical_command err \n", __func__, __LINE__);
			goto err_get_wb_bgain;
		}
		wb_attr.bgain = reason;
	}
	copy_to_user((void __user*)control->value, &wb_attr, sizeof(wb_attr));

	return 0;

err_get_wb_bgain:
err_get_wb_rgain:
err_get_wb_mode:
	return -1;
}

static int apical_isp_max_again_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	unsigned char status = 0;
	int reason = 0;

	apical_api_control_t api;
	api.type = TSYSTEM;
	api.dir = COMMAND_SET;

	api.id = SYSTEM_MAX_SENSOR_ANALOG_GAIN;
	api.value = control->value;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_set_max_again_bgain;
	}

	return 0;
err_set_max_again_bgain:
	return 0;
}

static int apical_isp_max_again_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	unsigned char status = 0;
	int reason = 0;
	apical_api_control_t api;
	api.type = TSYSTEM;
	api.dir = COMMAND_GET;

	api.id = SYSTEM_MAX_SENSOR_ANALOG_GAIN;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_max_again_bgain;
	}
	control->value = reason;

	return 0;
err_get_max_again_bgain:
	return 0;
}

static int apical_isp_max_dgain_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	unsigned char status = 0;
	int reason = 0;

	apical_api_control_t api;
	api.type = TSYSTEM;
	api.dir = COMMAND_SET;

	api.id = SYSTEM_MAX_ISP_DIGITAL_GAIN;
	api.value = control->value;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_set_max_dgain;
	}

	return 0;
err_set_max_dgain:
	return 0;
}

static int apical_isp_max_dgain_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	unsigned char status = 0;
	int reason = 0;
	apical_api_control_t api;
	api.type = TSYSTEM;
	api.dir = COMMAND_GET;

	api.id = SYSTEM_MAX_ISP_DIGITAL_GAIN;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_max_dgain;
	}
	control->value = reason;

	return 0;
err_get_max_dgain:
	return 0;
}

static int apical_isp_hi_light_depress_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	unsigned char status = 0;
	int ret = 0;
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);
	int strength = control->value;

	unsigned int wdr = tuning->ctrls.wdr;

	LookupTable** table = NULL;
	ISP_CORE_MODE_DN_E dn;
	unsigned int rows = 0;
	unsigned int cols = 0;
	unsigned int width = 0;
	unsigned int size = 0;
	void *data = 0;

	if(wdr == ISPCORE_MODULE_ENABLE){
		printk("err: wdr enabled\n");
		return -1;
	}

	dn = ctrls->daynight;
	if (NULL == param) {
		printk("err: param == NULL\n");
		return -1;
	}
	table = param->isp_param[dn].calibrations;

	rows = table[_CALIBRATION_AE_BALANCED_LINEAR]->rows;
	cols = table[_CALIBRATION_AE_BALANCED_LINEAR]->cols;
	width = table[_CALIBRATION_AE_BALANCED_LINEAR]->width;
	size = rows*cols*width;

	data = kzalloc(size, GFP_KERNEL);
	if(!data){
		printk("err: Failed to allocate isp table mem\n");
		return -1;
	}
	memcpy(data, table[_CALIBRATION_AE_BALANCED_LINEAR]->ptr, size);
	if (1 == width) {
		uint8_t *v = data;
		v[2] = strength;
	} else if (2 == width) {
		uint16_t *v = data;
		v[2] = strength;
	} else if (4 == width) {
		uint32_t *v = data;
		v[2] = strength;
	} else {
		printk("err: %s(%d),format error !\n", __func__, __LINE__);
		kfree(data);
		return -1;
	}

	status = apical_api_calibration(CALIBRATION_AE_BALANCED_LINEAR, COMMAND_SET, data, size, &ret);
	if (0 != ret) {
		kfree(data);
		printk("err: %s,%d, status = %d, ret = %d\n", __func__, __LINE__, status, ret);
		return -1;
	}
	kfree(data);

	return 0;
}

static int apical_isp_hi_light_depress_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	unsigned char status = 0;
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);
	int ret = 0;

	unsigned int wdr = tuning->ctrls.wdr;

	LookupTable** table = NULL;
	ISP_CORE_MODE_DN_E dn;
	unsigned int rows = 0;
	unsigned int cols = 0;
	unsigned int width = 0;
	unsigned int size = 0;
	void *data = 0;

	if(wdr == ISPCORE_MODULE_ENABLE){
		printk("err: wdr enabled\n");
		return -1;
	}

	dn = ctrls->daynight;
	if (NULL == param) {
		printk("err: param == NULL\n");
		return -1;
	}
	table = param->isp_param[dn].calibrations;

	rows = table[_CALIBRATION_AE_BALANCED_LINEAR]->rows;
	cols = table[_CALIBRATION_AE_BALANCED_LINEAR]->cols;
	width = table[_CALIBRATION_AE_BALANCED_LINEAR]->width;
	size = rows*cols*width;

	data = kzalloc(size, GFP_KERNEL);
	if(!data){
		printk("err: Failed to allocate isp table mem\n");
		return -1;
	}

	status = apical_api_calibration(CALIBRATION_AE_BALANCED_LINEAR, COMMAND_GET, data, size, &ret);
	if (0 != ret)
		printk("err: %s,%d, status = %d, ret = %d\n", __func__, __LINE__, status, ret);

	if (1 == width) {
		uint8_t *v = data;
		control->value = v[2];
	} else if (2 == width) {
		uint16_t *v = data;
		control->value = v[2];
	} else if (4 == width) {
		uint32_t *v = data;
		control->value = v[2];
	} else {
		printk("err: %s(%d),format error !\n", __func__, __LINE__);
		kfree(data);
		return -1;
	}
	kfree(data);

	return 0;
}

struct isp_table_info {
	unsigned int id;
	unsigned int rows;
	unsigned int cols;
	unsigned int width;
	unsigned int tsource;
	void *ptr;
};

static int apical_isp_table_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);

	int ret = ISP_SUCCESS;
	unsigned int status = 0;
	LookupTable** table = NULL;
	ISP_CORE_MODE_DN_E dn;

	unsigned int rows = 0;
	unsigned int cols = 0;
	unsigned int width = 0;
	unsigned int size = 0;
	unsigned int id = 0;
	unsigned int tid = 0;
	struct isp_table_info tinfo;
	void *data = 0;

	dn = ctrls->daynight;
	if (NULL == param) {
		goto err_isp_param;
	}
	table = param->isp_param[dn].calibrations;
	copy_from_user(&tinfo, (const void __user*)control->value, sizeof(tinfo));
	id = tinfo.id;

	switch(id) {
	case CALIBRATION_TEMPER_STRENGTH:
		rows = table[_CALIBRATION_TEMPER_STRENGTH]->rows;
		cols = table[_CALIBRATION_TEMPER_STRENGTH]->cols;
		width = table[_CALIBRATION_TEMPER_STRENGTH]->width;
		tid = _CALIBRATION_TEMPER_STRENGTH;
		break;
	case CALIBRATION_SINTER_STRENGTH_LINEAR:
		rows = table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->rows;
		cols = table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->cols;
		width = table[_CALIBRATION_SINTER_STRENGTH_LINEAR]->width;
		tid = _CALIBRATION_SINTER_STRENGTH_LINEAR;
		break;
	case CALIBRATION_DP_SLOPE_LINEAR:
		rows = table[_CALIBRATION_DP_SLOPE_LINEAR]->rows;
		cols = table[_CALIBRATION_DP_SLOPE_LINEAR]->cols;
		width = table[_CALIBRATION_DP_SLOPE_LINEAR]->width;
		tid = _CALIBRATION_DP_SLOPE_LINEAR;
		break;
	default:
		printk("%s,%d, err id: %d\n", __func__, __LINE__, id);
		ret = -EPERM;
		break;
	}
	tinfo.rows = rows;
	tinfo.cols = cols;
	tinfo.width = width;
	size = rows*cols*width;

	copy_to_user((void __user*)control->value, &tinfo, sizeof(tinfo));
	if (NULL != tinfo.ptr) {
		if (0 == tinfo.tsource) {
			data = kzalloc(size, GFP_KERNEL);
			if(!data){
				printk("Failed to allocate isp table mem\n");
				return -1;
			}
			status = apical_api_calibration(id, COMMAND_GET, tinfo.ptr, size, &ret);
			if (0 != ret)
				printk("%s,%d, status = %d, ret = %d\n", __func__, __LINE__, status, ret);
			copy_to_user((void __user*)tinfo.ptr, data, size);
			kfree(data);
		} else {
			copy_to_user((void __user*)tinfo.ptr, table[tid]->ptr, size);
		}
	}
	return 0;
err_isp_param:
	return -1;
}

static int apical_isp_table_s_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct tx_isp_subdev *sd = tuning->parent;
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);

	int ret = ISP_SUCCESS;
	unsigned int status = 0;
	LookupTable** table = NULL;
	ISP_CORE_MODE_DN_E dn;

	unsigned int rows = 0;
	unsigned int cols = 0;
	unsigned int width = 0;
	unsigned int size = 0;
	unsigned int id = 0;
	struct isp_table_info tinfo;
	void *data = 0;

	if (NULL == param) {
		goto err_isp_param;
	}
	dn = ctrls->daynight;
	table = param->isp_param[dn].calibrations;
	copy_from_user(&tinfo, (const void __user*)control->value, sizeof(tinfo));

	id = tinfo.id;
	rows = tinfo.rows;
	cols = tinfo.cols;
	width = tinfo.width;
	size = rows*cols*width;

	if (NULL != tinfo.ptr) {
		data = kzalloc(size, GFP_KERNEL);
		if(!data){
			printk("Failed to allocate isp table mem\n");
			return -1;
		}
		copy_from_user(data, (const void __user*)tinfo.ptr, size);
		status = apical_api_calibration(id, COMMAND_SET, data, size, &ret);
		if (0 != ret)
			printk("%s,%d, status = %d, ret = %d\n", __func__, __LINE__, status, ret);
		kfree(data);
	} else {
		printk("%s,%d, param err\n", __func__, __LINE__);
	}
	return 0;
err_isp_param:
	return -1;
}

struct isp_frame_done_info {
	unsigned int timeout;
	uint64_t cnt;
	int reserved;
};

unsigned long long frame_done_cnt = 0;
DECLARE_WAIT_QUEUE_HEAD(frame_done_wq);
unsigned long long frame_done_cond = 0;

static int isp_frame_done_wait(int timeout, uint64_t *cnt)
{
	int ret = -1;
	frame_done_cond = 0;
	ret = wait_event_interruptible_timeout(frame_done_wq, (1 == frame_done_cond), timeout);
	*cnt = frame_done_cnt;
	if (-ERESTARTSYS == ret)
		return ret;
	if (!ret)
		return -ETIMEDOUT;
	return 0;
}

static void isp_frame_done_wakeup(void)
{
	frame_done_cnt++;
	frame_done_cond = 1;
	wake_up(&frame_done_wq);
}

static int apical_isp_wait_frame_done(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	int ret = ISP_SUCCESS;
	unsigned int timeout = 0;
	uint64_t cnt = 0;
	struct isp_frame_done_info info;

	copy_from_user(&info, (const void __user*)control->value, sizeof(info));
	timeout = info.timeout;

	ret = isp_frame_done_wait(timeout, &cnt);
	info.cnt = cnt;

	copy_to_user((void __user*)control->value, &info, sizeof(info));
	return ret;

}

static int apical_isp_ev_g_attr(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	struct isp_core_ev_attr ev_attr;
	apical_api_control_t api;

	unsigned char status = 0;
	int reason = 0;


	api.type = TSYSTEM;
	api.dir = COMMAND_GET;

	api.id = SYSTEM_MANUAL_EXPOSURE_TIME;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_ev;
	}
	ev_attr.ev = reason;

	api.id = SYSTEM_SENSOR_ANALOG_GAIN;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_again;
	}
	ev_attr.again = reason;

	api.id = SYSTEM_ISP_DIGITAL_GAIN;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_dgain;
	}
	ev_attr.dgain = reason;

	api.type = TALGORITHMS;
	api.dir = COMMAND_GET;
	api.id = AE_EXPOSURE_ID;
	api.value = -1;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_expr;
	}
	ev_attr.expr_us = reason;

	api.type = CALIBRATION;
	api.id = EXPOSURE_LOG2_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_ev_log2;
	}
	ev_attr.ev_log2 = reason;

	api.id = GAIN_LOG2_ID;
	status = apical_command(api.type, api.id, api.value, api.dir, &reason);
	if (status != ISP_SUCCESS) {
		printk("err: %s,%d apical_command err \n", __func__, __LINE__);
		goto err_get_gain_log2;
	}
	ev_attr.gain_log2 = reason;


	copy_to_user((void __user*)control->value, &ev_attr, sizeof(ev_attr));

	return 0;

err_get_dgain:
err_get_again:
err_get_expr:
err_get_ev:
err_get_ev_log2:
err_get_gain_log2:
	return -1;
}


static int apical_isp_core_ops_g_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *control)
{
	int ret = ISP_SUCCESS;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);

	/* printk("%s[%d] control->id = 0x%08x\n", __func__, __LINE__, control->id); */
	switch(control->id){
	case IMAGE_TUNING_CID_AWB_ATTR:
		break;
	case IMAGE_TUNING_CID_WB_STAINFO:
		break;
	case IMAGE_TUNING_CID_AE_ATTR:
		ret = apical_isp_ae_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_AE_STAINFO:
		break;
	case IMAGE_TUNING_CID_AF_ATTR:
		break;
	case IMAGE_TUNING_CID_AF_STAINFO:
		break;
	case IMAGE_TUNING_CID_TEMPER_STRENGTH:
		ret = apical_isp_temper_dns_g_strength(tuning, control);
		break;
	case IMAGE_TUNING_CID_TEMPER_ATTR:
		ret = apical_isp_temper_dns_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_WDR_ATTR:
		break;
	case IMAGE_TUNING_CID_DIS_STAINFO:
		break;
	case IMAGE_TUNING_CID_FC_ATTR:
		break;
	case IMAGE_TUNING_CID_SHARP_ATTR:
		break;
	case IMAGE_TUNING_CID_DEMO_ATTR:
		break;
	case IMAGE_TUNING_CID_DRC_ATTR:
		ret = apical_isp_drc_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_SHAD_ATTR:
		break;
	case IMAGE_TUNING_CID_CONTROL_FPS:
		ret = apical_isp_fps_g_control(tuning, control);
		break;
	case IMAGE_TUNING_CID_GET_TOTAL_GAIN:
		ret = apical_isp_g_totalgain(tuning, control);
		break;
	case IMAGE_TUNING_CID_DAY_OR_NIGHT:
		ret = apical_isp_day_or_night_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_HVFLIP:
		control->value = (ctrls->hflip<<16)|ctrls->vflip;
		break;
	case IMAGE_TUNING_CID_AE_STRATEGY:
		ret = apical_isp_ae_strategy_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_GAMMA_ATTR:
		ret = apical_isp_gamma_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_SYSTEM_TAB:
		ret = apical_isp_stab_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_EXPR_ATTR:
		ret = apical_isp_expr_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_AE_ROI:
		ret = apical_isp_ae_g_roi(tuning, control);
		break;
	case IMAGE_TUNING_CID_WB_ATTR:
		ret = apical_isp_wb_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_WB_STATIS_ATTR:
		ret = apical_isp_wb_statis_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_AWB_RGB_COEFFT_WB_ATTR:
		ret = apical_isp_rgb_coefft_wb_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_MAX_AGAIN_ATTR:
		ret = apical_isp_max_again_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_MAX_DGAIN_ATTR:
		ret = apical_isp_max_dgain_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_HILIGHT_DEPRESS_STRENGTH:
		ret = apical_isp_hi_light_depress_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_AE_COMP:
		ret = apical_isp_ae_comp_g_ctrl(tuning, control);
		break;
	case IMAGE_TUNING_CID_ISP_TABLE_ATTR:
		ret = apical_isp_table_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_ISP_WAIT_FRAME_ATTR:
		ret = apical_isp_wait_frame_done(tuning, control);
		break;
	case IMAGE_TUNING_CID_ISP_EV_ATTR:
		ret = apical_isp_ev_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_AWB_CWF_SHIFT:
		ret = apical_isp_awb_cwf_g_shift(tuning, control);
		break;
	case IMAGE_TUNING_CID_AE_WEIGHT:
		ret = apical_isp_ae_weight_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_AWB_WEIGHT:
		ret = apical_isp_awb_weight_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_AE_HIST:
		ret = apical_isp_ae_hist_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_AWB_HIST:
		ret = apical_isp_awb_hist_g_attr(tuning, control);
		break;
	case IMAGE_TUNING_CID_AF_HIST:
		ret = apical_isp_af_hist_g_attr(tuning, control);
		break;
	case V4L2_CID_HFLIP:
		control->value = ctrls->hflip;
		break;
	case V4L2_CID_VFLIP:
		control->value = ctrls->vflip;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		control->value = ctrls->flicker;
		break;
	case IMAGE_TUNING_CID_CUSTOM_TEMPER_DNS:
		control->value = ctrls->temper;
		break;
	case V4L2_CID_SCENE_MODE:
		control->value = ctrls->scene;
		break;
	case V4L2_CID_COLORFX:
		control->value = ctrls->colorfx;
		break;
	case V4L2_CID_SATURATION:
		control->value = ctrls->saturation;
		break;
	case V4L2_CID_BRIGHTNESS:
		control->value = ctrls->brightness;
		break;
	case V4L2_CID_CONTRAST:
		control->value = ctrls->contrast;
		break;
	case V4L2_CID_SHARPNESS:
		control->value = ctrls->sharpness;
		break;
	case IMAGE_TUNING_CID_CUSTOM_WDR:
		control->value = ctrls->wdr;
		break;
	default:
		ret = -EPERM;
		break;
	}
	return ret;
}

static int apical_isp_core_ops_s_ctrl(image_tuning_vdrv_t *tuning, struct v4l2_control *ctrl)
{
	int ret = ISP_SUCCESS;
	switch (ctrl->id) {
	case IMAGE_TUNING_CID_MWB_ATTR:
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
		ret = apical_isp_wb_s_control(tuning, ctrl);
		break;
	case V4L2_CID_HFLIP:
		ret = apical_isp_hflip_s_control(tuning, ctrl);
		break;
	case V4L2_CID_VFLIP:
		ret = apical_isp_vflip_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_SINTER_ATTR:
		ret = apical_isp_sinter_dns_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_CUSTOM_TEMPER_DNS:
		ret = apical_isp_temper_dns_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_TEMPER_STRENGTH:
		ret = apical_isp_temper_dns_s_strength(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_TEMPER_ATTR:
		ret = apical_isp_temper_dns_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_TEMPER_BUF:
		ret = apical_isp_temper_dns_s_buf(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_NOISE_PROFILE_ATTR:
		ret = apical_isp_noise_profile_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_CUSTOM_WDR:
		ret = apical_isp_wdr_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_WDR_ATTR:
		ret = apical_isp_wdr_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_CUSTOM_ISP_PROCESS:
		ret = apical_isp_bypass_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_CUSTOM_ISP_FREEZE:
		ret = apical_isp_freeze_s_control(tuning, ctrl);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		ret = apical_isp_flicker_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_CUSTOM_SHAD:
		ret = apical_isp_lens_shad_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_SHAD_ATTR:
		ret = apical_isp_lens_shad_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_GE_ATTR:
		ret = apical_isp_ge_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_DYNAMIC_DP_ATTR:
		ret = apical_isp_dynamic_dp_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_STATIC_DP_ATTR:
		ret = apical_isp_static_dp_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_CUSTOM_ANTI_FOG:
		ret = apical_isp_antifog_s_control(tuning, ctrl);
		break;
	case V4L2_CID_SCENE_MODE:
		ret = apical_isp_scene_s_control(tuning, ctrl);
		break;
	case V4L2_CID_COLORFX:
		ret = apical_isp_colorfx_s_control(tuning, ctrl);
		break;
	case V4L2_CID_SATURATION:
		ret = apical_isp_sat_s_control(tuning, ctrl);
		break;
	case V4L2_CID_BRIGHTNESS:
		ret = apical_isp_bright_s_control(tuning, ctrl);
		break;
	case V4L2_CID_CONTRAST:
		ret = apical_isp_contrast_s_control(tuning, ctrl);
		break;
	case V4L2_CID_SHARPNESS:
		ret = apical_isp_sharp_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_SHARP_ATTR:
		ret = apical_isp_sharp_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_CUSTOM_DRC:
		ret = apical_isp_drc_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_DRC_ATTR:
		ret = apical_isp_drc_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_DEMO_ATTR:
		ret = apical_isp_demosaic_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_FC_ATTR:
		ret = apical_isp_fc_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_CONTROL_FPS:
		ret = apical_isp_fps_s_control(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_DAY_OR_NIGHT:
		ret = apical_isp_day_or_night_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_HVFLIP:
		ret = apical_isp_hvflip_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AE_STRATEGY:
		ret = apical_isp_ae_strategy_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_GAMMA_ATTR:
		ret = apical_isp_gamma_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_SYSTEM_TAB:
		ret = apical_isp_stab_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_EXPR_ATTR:
		ret = apical_isp_expr_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AE_ROI:
		ret = apical_isp_ae_s_roi(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_WB_ATTR:
		ret = apical_isp_wb_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AWB_RGB_COEFFT_WB_ATTR:
		ret = apical_isp_rgb_coefft_wb_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_MAX_AGAIN_ATTR:
		ret = apical_isp_max_again_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_MAX_DGAIN_ATTR:
		ret = apical_isp_max_dgain_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_HILIGHT_DEPRESS_STRENGTH:
		ret = apical_isp_hi_light_depress_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AE_COMP:
		ret = apical_isp_ae_comp_s_ctrl(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_ISP_TABLE_ATTR:
		ret = apical_isp_table_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AWB_CWF_SHIFT:
		ret = apical_isp_awb_cwf_s_shift(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AE_WEIGHT:
		ret = apical_isp_ae_weight_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AWB_WEIGHT:
		ret = apical_isp_awb_weight_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AE_HIST:
		ret = apical_isp_ae_hist_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AWB_HIST:
		ret = apical_isp_awb_hist_s_attr(tuning, ctrl);
		break;
	case IMAGE_TUNING_CID_AF_HIST:
		ret = apical_isp_af_hist_s_attr(tuning, ctrl);
		break;
	default:
		ret = -EPERM;
		break;
	}
	return ret;
}

static long isp_core_tunning_default_ioctl(image_tuning_vdrv_t *tuning, unsigned int cmd, unsigned long arg)
{
	struct isp_image_tuning_default_ctrl ctrl;
	long ret = 0;

	if(copy_from_user(&ctrl, (void __user *)arg, sizeof(ctrl)))
		return -EFAULT;

	/*printk("##### %s %d DEF_CTRL cmd=0x%08x #####\n", __func__,__LINE__, ctrl.control.id);*/
	if(ctrl.dir == TX_ISP_PRIVATE_IOCTL_SET){
		ret = apical_isp_core_ops_s_ctrl(tuning, &(ctrl.control));
	}else{
		ret = apical_isp_core_ops_g_ctrl(tuning, &(ctrl.control));
		if(ret && ret != -ENOIOCTLCMD)
			goto done;
		if (copy_to_user((void __user *)arg, &ctrl, sizeof(ctrl)))
			ret = -EFAULT;
	}
done:
	return ret;
}

static long isp_core_tunning_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_module *module = miscdev_to_module(dev);
	struct tx_isp_subdev *sd = module_to_subdev(module);
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	image_tuning_vdrv_t *tuning = core->tuning;
	struct v4l2_control control;
	long ret = 0;

	/* the file must be opened firstly. */
	if(tuning->state != TX_ISP_MODULE_INIT){
		return -EPERM;
	}

	switch(cmd){
	case VIDIOC_S_CTRL:
		if(copy_from_user(&control, (void __user *)arg, sizeof(control)))
			return -EFAULT;
		/*printk("##### %s %d S_CTRL cmd=0x%08x #####\n", __func__,__LINE__, control.id);*/
		ret = apical_isp_core_ops_s_ctrl(tuning, &control);
		break;
	case VIDIOC_G_CTRL:
		if(copy_from_user(&control, (void __user *)arg, sizeof(control)))
			return -EFAULT;
		/*printk("##### %s %d G_CTRL cmd=0x%08x #####\n", __func__,__LINE__, control.id);*/
		ret = apical_isp_core_ops_g_ctrl(tuning, &control);
		if(ret && ret != -ENOIOCTLCMD)
			goto done;
		if (copy_to_user((void __user *)arg, &control, sizeof(control)))
			ret = -EFAULT;
		break;
	default:
		ret = isp_core_tunning_default_ioctl(tuning, cmd, arg);
		break;
	}

	ret = (ret == -ENOIOCTLCMD) ? 0 : ret;

done:
	return ret;
}


static int isp_core_tunning_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_module *module = miscdev_to_module(dev);
	struct tx_isp_subdev *sd = module_to_subdev(module);
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	TXispPrivParamManage *param = core->param;
	LookupTable** table = NULL;
	/*struct tx_isp_video_in *vin = &core->vin;*/
	image_tuning_vdrv_t *tuning = core->tuning;
	struct image_tuning_ctrls *ctrls = &(tuning->ctrls);

	/* the module must be actived firstly. */
	if(tuning->state != TX_ISP_MODULE_ACTIVATE){
		return -EPERM;
	}

	core->isp_daynight_switch = 1;
	tuning->temper_paddr = 0;
	table = param->isp_param[TX_ISP_PRIV_PARAM_DAY_MODE].calibrations;
	ctrls->temper_max = *((uint16_t *)(table[ _CALIBRATION_TEMPER_STRENGTH]->ptr) + table[_CALIBRATION_TEMPER_STRENGTH]->rows * table[_CALIBRATION_TEMPER_STRENGTH]->cols -1 );;
	ctrls->temper_min = *((uint16_t *)(table[ _CALIBRATION_TEMPER_STRENGTH]->ptr) + 1);

#if 0
	tuning->temper_buffer_size = vin->vi_max_width * vin->vi_max_height * 4;
	tuning->wdr_buffer_size = vin->vi_max_width * vin->vi_max_height * 4;
	tuning->temper_paddr = isp_malloc_buffer(tuning->temper_buffer_size);
	if(tuning->temper_paddr == 0){
		ISP_INFO("#### DEBUG: ISP can't support temper module! Because that has't enough memory!\n");
	}else{
		/* config temper dma */
		apical_isp_temper_frame_buffer_bank0_base_write(tuning->temper_paddr);
		apical_isp_temper_frame_buffer_bank1_base_write(tuning->temper_paddr);
		/* apical_isp_temper_frame_buffer_bank1_base_write(tuning->temper_paddr + (tuning->temper_buffer_size)); */
		apical_isp_temper_frame_buffer_frame_write_cancel_write(0);
		apical_isp_temper_frame_buffer_frame_read_cancel_write(0);
		apical_isp_temper_frame_buffer_frame_write_on_write(1);
		apical_isp_temper_frame_buffer_frame_read_on_write(1);
	}
#endif


	tuning->state = TX_ISP_MODULE_INIT;
	return 0;
}

static int isp_core_tunning_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_module *module = miscdev_to_module(dev);
	struct tx_isp_subdev *sd = module_to_subdev(module);
	struct tx_isp_core_device *core = tx_isp_get_subdevdata(sd);
	image_tuning_vdrv_t *tuning = core->tuning;

	printk("##### %s %d #####\n", __func__,__LINE__);
	if(tuning->state == TX_ISP_MODULE_DEINIT)
		return 0;

	if(tuning->temper_paddr)
		isp_free_buffer(tuning->temper_paddr);

	tuning->state = TX_ISP_MODULE_DEINIT;
	return 0;
}

static struct file_operations isp_core_tunning_fops = {
	.open = isp_core_tunning_open,
	.release = isp_core_tunning_release,
	.unlocked_ioctl = isp_core_tunning_unlocked_ioctl,
};

static int isp_core_tuning_activate(struct isp_core_tuning_driver *tuning)
{
	/*if(tuning->state != TX_ISP_MODULE_SLAKE)*/
	/*return -EPERM;*/

	tuning->state = TX_ISP_MODULE_ACTIVATE;
	return 0;
}

static int isp_core_tuning_slake(struct isp_core_tuning_driver *tuning)
{
	/*if(tuning->state != TX_ISP_MODULE_ACTIVATE)*/
	/*return -EPERM;*/

	tuning->state = TX_ISP_MODULE_SLAKE;
	return 0;
}

static int isp_core_tuning_event(struct isp_core_tuning_driver *tuning, unsigned int event, void *data)
{
	int ret = 0;
	switch(event){
	case TX_ISP_EVENT_ACTIVATE_MODULE:
		ret = isp_core_tuning_activate(tuning);
		break;
	case TX_ISP_EVENT_SLAVE_MODULE:
		ret = isp_core_tuning_slake(tuning);
		break;
	case TX_ISP_EVENT_CORE_FRAME_DONE:
		isp_frame_done_wakeup();
		break;
	case TX_ISP_EVENT_CORE_DAY_NIGHT:
		apical_isp_day_or_night_s_ctrl_internal(tuning);
		break;
	default:
		break;
	}
	return ret;
}

struct isp_core_tuning_driver *isp_core_tuning_init(struct tx_isp_subdev *parent)
{
	image_tuning_vdrv_t *tuning = NULL;

	tuning = (image_tuning_vdrv_t *)kzalloc(sizeof(*tuning), GFP_KERNEL);
	if(!tuning){
		ISP_ERROR("Failed to allocate isp image tuning device\n");
		return NULL;
	}
	memset(tuning, 0, sizeof(*tuning));

	tuning->parent = parent;
	spin_lock_init(&tuning->slock);
	mutex_init(&tuning->mlock);

	tuning->state = TX_ISP_MODULE_SLAKE;
	tuning->fops = &isp_core_tunning_fops;
	tuning->event = isp_core_tuning_event;
	return tuning;
}

void isp_core_tuning_deinit(image_tuning_vdrv_t *tuning)
{
	if(tuning)
		kfree(tuning);
}
