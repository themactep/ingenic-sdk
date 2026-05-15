#ifndef __TX_ISP_TUNING_H__
#define __TX_ISP_TUNING_H__

/**
 * @fn int tx_isp_tuning_set_brightness(unsigned char bright)
 *
 * 设置ISP 综合效果图片亮度
 *
 * @param[in] bright 图片亮度参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 默认值为128，大于128增加亮度，小于128降低亮度。
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_brightness(unsigned char bright);

/**
 * @fn int tx_isp_tuning_get_brightness(unsigned char *pbright)
 *
 * 获取ISP 综合效果图片亮度
 *
 * @param[in] bright 图片亮度参数指针
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 默认值为128，大于128增加亮度，小于128降低亮度。
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_brightness(unsigned char *pbright);

/**
 * @fn int tx_isp_tuning_set_contrast(unsigned char contrast)
 *
 * 设置ISP 综合效果图片对比度
 *
 * @param[in] contrast 图片对比度参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 默认值为128，大于128增加对比度，小于128降低对比度。
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_contrast(unsigned char contrast);

/**
 * @fn int tx_isp_tuning_get_contrast(unsigned char *pcontrast)
 *
 * 获取ISP 综合效果图片对比度
 *
 * @param[in] contrast 图片对比度参数指针
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 默认值为128，大于128增加对比度，小于128降低对比度。
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_contrast(unsigned char *pcontrast);

 /**
 * @fn int tx_isp_tuning_set_sharpness(unsigned char sharpness)
 *
 * 设置ISP 综合效果图片锐度
 *
 * @param[in] sharpness 图片锐度参数值
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 默认值为128，大于128增加锐度，小于128降低锐度。
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_sharpness(unsigned char sharpness);

/**
 * @fn int tx_isp_tuning_get_sharpness(unsigned char *psharpness)
 *
 * 获取ISP 综合效果图片锐度
 *
 * @param[in] sharpness 图片锐度参数指针
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 默认值为128，大于128增加锐度，小于128降低锐度。
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_sharpness(unsigned char *psharpness);

/**
 * @fn int tx_isp_tuning_set_saturation(unsigned char sat)
 *
 * 设置ISP 综合效果图片饱和度
 *
 * @param[in] sat 图片饱和度参数值
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 默认值为128，大于128增加饱和度，小于128降低饱和度。
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_saturation(unsigned char sat);

/**
 * @fn int tx_isp_tuning_get_saturatio(unsigned char *psat)
 *
 * 获取ISP 综合效果图片饱和度
 *
 * @param[in] sat 图片饱和度参数指针
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @remark 默认值为128，大于128增加饱和度，小于128降低饱和度。
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_saturation(unsigned char *psat);


/**
 * @fn int tx_isp_tuning_set_ae_it_max(unsigned int it_max)
 *
 * 设置AE最大值参数。
 *
 * @param[in] it_max AE最大值参数。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_ae_it_max(unsigned int it_max);

/**
 * @fn int tx_isp_tuning_get_ae_it_max(unsigned int *it_max)
 *
 * 获取AE最大值参数。
 *
 * @param[out] it_max AE最大值信息。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_ae_it_max(unsigned int *it_max);

/**
 * ISP HV Flip模式。
 */
typedef enum {
	ISP_CORE_FLIP_NORMAL_MODE = 0,  /**< 正常模式 */
	ISP_CORE_FLIP_H_MODE = 1,      /**< 镜像模式 */
	ISP_CORE_FLIP_V_MODE = 2,       /**< 翻转模式 */
	ISP_CORE_FLIP_HV_MODE = 3,  /**< 镜像并翻转模式 */
	ISP_CORE_FLIP_MODE_BUTT,
} ISP_CORE_HVFLIP;

/**
 * @fn int tx_isp_tuning_set_hv_flip(ISPHVFLIP hvflip)
 *
 * 设置HV Flip的模式.
 *
 * @param[in] hvflip HV Flip模式.
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_hv_flip(ISP_CORE_HVFLIP hvflip);

/**
 * @fn int tx_isp_tuning_get_hv_flip(ISPHVFLIP *hvflip)
 *
 * 获取HV Flip的模式.
 *
 * @param[out] hvflip HV Flip模式.
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_hv_flip(ISP_CORE_HVFLIP *hvflip);

/**
 * @fn int tx_isp_tuning_set_max_again(unsigned int again)
 *
 * 设置sensor可以设置最大Again。
 *
 * @param[in] gain sensor可以设置的最大again.0表示1x，32表示2x，依次类推。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_max_again(unsigned int again);

/**
 * @fn int tx_isp_tuning_get_max_again(unsigned int *again)
 *
 * 获取sensor可以设置最大Again。
 *
 * @param[out] gain sensor可以设置的最大again.0表示1x，32表示2x，依次类推。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_max_again(unsigned int *again);

/**
 * @fn int tx_isp_tuning_set_max_dgain(unsigned int dgain)
 *
 * 设置ISP可以设置的最大Dgain。
 *
 * @param[in] ISP Dgain 可以设置的最大dgain.0表示1x，32表示2x，依次类推。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_max_dgain(unsigned int dgain);

/**
 * @fn int tx_isp_tuning_get_max_dgain(unsigned int *dgain)
 *
 * 获取ISP设置的最大Dgain。
 *
 * @param[out] ISP Dgain 可以得到设置的最大的dgain.0表示1x，32表示2x，依次类推。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_max_dgain(unsigned int *dgain);

/* expr */
enum isp_core_expr_mode {
	ISP_CORE_EXPR_MODE_AUTO,
	ISP_CORE_EXPR_MODE_MANUAL,
};

enum isp_core_expr_unit {
	ISP_CORE_EXPR_UNIT_LINE,
	ISP_CORE_EXPR_UNIT_US,
};

union isp_core_expr_attr{
	struct {
		enum isp_core_expr_mode mode;
		enum isp_core_expr_unit unit;
		unsigned short time;
	} s_attr;
	struct {
		enum isp_core_expr_mode mode;
		unsigned short integration_time;
		unsigned short integration_time_min;
		unsigned short integration_time_max;
		unsigned short one_line_expr_in_us;
	} g_attr;
};

/**
 * @fn int tx_isp_tuning_set_expr(union isp_core_expr_attr attr)
 *
 * 设置AE参数。
 *
 * @param[in] expr AE参数。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_expr(union isp_core_expr_attr attr);

/**
 * @fn int tx_isp_tuning_get_expr(union isp_core_expr_attr *attr)
 *
 * 获取AE参数。
 *
 * @param[out] expr AE参数。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_expr(union isp_core_expr_attr *attr);

/**
 * @fn int tx_isp_tuning_set_sinter_strength(unsigned int ratio)
 *
 * 设置2D降噪强度。
 *
 * @param[in] ratio 强度调节比例.如果设置为90则表示设置为默认值的90%.取值范围为［50-150].
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_sinter_strength(unsigned int ratio);

/**
 * gamma
 */
struct isp_core_gamma_attr{
	unsigned short gamma[129]; /**< gamma参数数组，有129个点 */
};

/**
 * @fn int tx_isp_tuning_set_gamma(struct isp_core_gamma_attr attr)
 *
 * 设置GAMMA参数.
 * @param[in] gamma gamma参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_gamma(struct isp_core_gamma_attr attr);

/**
 * @fn int tx_isp_tuning_get_gamma(struct isp_core_gamma_attr *attr)
 *
 * 获取GAMMA参数.
 * @param[out] gamma gamma参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_gamma(struct isp_core_gamma_attr *attr);

/**
 * @fn int tx_isp_tuning_set_fps(uint32_t fps_num, uint32_t fps_den)
 *
 * 设置摄像头输出帧率
 *
 * @param[in] fps_num 设定帧率的分子参数
 * @param[in] fps_den 设定帧率的分母参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_fps(uint32_t fps_num, uint32_t fps_den);

/**
 * @fn int tx_isp_tuning_set_hilight_depress(uint32_t strength)
 *
 * 设置强光抑制强度。
 *
 * @param[in] strength 强光抑制强度参数.取值范围为［0-10], 0表示关闭功能。
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_set_hilight_depress(uint32_t strength);

/**
 * @fn int tx_isp_tuning_get_ae_luma(unsigned char *luma)
 *
 * 获取画面平均亮度。
 *
 * @param[out] luma AE亮度参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_ae_luma(unsigned char *luma);

/* ev */
struct isp_core_ev_attr {
    unsigned int ev;
    unsigned int expr_us;
    unsigned int ev_log2;
    unsigned int again;
    unsigned int dgain;
    unsigned int gain_log2;
};

/**
 * @fn int tx_isp_tuning_get_ev_attr(struct isp_core_ev_attr *ev_attr)
 *
 * 获取EV属性。
 * @param[out] attr EV属性参数
 *
 * @retval 0 成功
 * @retval 非0 失败，返回错误码
 *
 * @attention 在使用这个函数之前，必须保证ISP效果调试功能已使能.
 */
int tx_isp_tuning_get_ev_attr(struct isp_core_ev_attr *ev_attr);

#endif // _TX_ISP_TUNING_H__

