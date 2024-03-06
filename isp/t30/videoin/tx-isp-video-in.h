#ifndef __TX_ISP_SENSOR_H__
#define __TX_ISP_SENSOR_H__
#include <tx-isp-common.h>
#include <linux/proc_fs.h>

struct tx_isp_vin_device {
	struct tx_isp_subdev sd;
	struct list_head sensors;
	struct tx_isp_sensor *active; /* the sensor instance */

	struct mutex mlock;
	int state;
	unsigned int refcnt;
	/*struct tx_isp_subdev_platform_data *pdata;*/
};

#define sd_to_vin_device(sd) (container_of(sd, struct tx_isp_vin_device, sd))
#endif /* __TX_ISP_SENSOR_H__ */
