#ifndef __TX_ISP_INTERRUPT_H__
#define __TX_ISP_INTERRUPT_H__

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <tx-isp-device.h>
int tx_isp_request_irq(struct platform_device *pdev, struct tx_isp_irq_device *irqdev);
void tx_isp_free_irq(struct tx_isp_irq_device *irqdev);
#endif /* __TX_ISP_INTERRUPT_H__ */
