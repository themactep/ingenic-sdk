#include <tx-isp-common.h>
#include "tx-isp-interrupt.h"

static void tx_isp_enable_irq(struct tx_isp_irq_device *irq_dev)
{
	/*unsigned long flags = 0;*/
	/*private_spin_lock_irqsave(&irq_dev->slock, flags);*/
	private_enable_irq(irq_dev->irq);
	/*private_spin_unlock_irqrestore(&irq_dev->slock, flags);*/
}
static void tx_isp_disable_irq(struct tx_isp_irq_device *irq_dev)
{
	/*unsigned long flags = 0;*/
	/*private_spin_lock_irqsave(&irq_dev->slock, flags);*/
	private_disable_irq(irq_dev->irq);
	/*private_spin_unlock_irqrestore(&irq_dev->slock, flags);*/
}

static irqreturn_t isp_irq_handle(int this_irq, void *dev)
{
	struct tx_isp_irq_device *irqdev = dev;
	struct tx_isp_subdev *sd = irqdev_to_subdev(irqdev);
	struct tx_isp_module *module = &sd->module;
	struct tx_isp_module *submodule = NULL;
	int index = 0;
	irqreturn_t ret = IRQ_HANDLED;
	irqreturn_t retval = IRQ_HANDLED;

	/* call irq server of this module */
	ret = tx_isp_subdev_call(sd, core, interrupt_service_routine, 0, NULL);
	if(ret == IRQ_WAKE_THREAD)
		retval = IRQ_WAKE_THREAD;

	/* call irq server of subdev */
	while(index < TX_ISP_ENTITY_ENUM_MAX_DEPTH){
		if(module->submods[index]){
			submodule = module->submods[index];
			ret = tx_isp_subdev_call(module_to_subdev(submodule), core,
					interrupt_service_routine, 0, NULL);
			if(ret == IRQ_WAKE_THREAD)
				retval = IRQ_WAKE_THREAD;
		}
		index++;
	}
	return retval;
}

static irqreturn_t isp_irq_thread_handle(int this_irq, void *dev)
{
	struct tx_isp_irq_device *irqdev = dev;
	struct tx_isp_subdev *sd = irqdev_to_subdev(irqdev);
	struct tx_isp_module *module = &sd->module;
	struct tx_isp_module *submodule = NULL;
	int index = 0;
	/* call irq server of this module */
	tx_isp_subdev_call(sd, core, interrupt_service_thread, NULL);

	/* call irq server of subdev */
	while(index < TX_ISP_ENTITY_ENUM_MAX_DEPTH){
		if(module->submods[index]){
			submodule = module->submods[index];
			tx_isp_subdev_call(module_to_subdev(submodule), core,
					interrupt_service_thread, NULL);
		}
		index++;
	}
	return IRQ_HANDLED;
}

int tx_isp_request_irq(struct platform_device *pdev, struct tx_isp_irq_device *irqdev)
{
	int irq;
	int ret = 0;

	if(!pdev || !irqdev){
		printk("%s[%d] the parameters are invalid!\n",__func__,__LINE__);
		ret = -EINVAL;
		goto exit;

	}

	irq = private_platform_get_irq(pdev, 0);
	if (irq < 0) {
		irqdev->irq = 0;
		goto done;
	}

	private_spin_lock_init(&irqdev->slock);

	ret = private_request_threaded_irq(irq, isp_irq_handle, isp_irq_thread_handle, IRQF_ONESHOT, pdev->name, irqdev);
	if(ret){
		ISP_ERROR("%s[%d] Failed to request irq(%d).\n", __func__,__LINE__, irq);
		ret = -EINTR;
		irqdev->irq = 0;
		goto err_req_irq;
	}

	irqdev->irq = irq;
	irqdev->enable_irq = tx_isp_enable_irq;
	irqdev->disable_irq = tx_isp_disable_irq;
	tx_isp_disable_irq(irqdev);
	/*printk("^^ %s[%d] %s irq = %d ^^\n", __func__,__LINE__, pdev->name, irq);*/

done:
	return 0;
err_req_irq:
exit:
	return ret;
}

void tx_isp_free_irq(struct tx_isp_irq_device *irqdev)
{
	if(!irqdev){
		return;
	}
	if(irqdev->irq)
		private_free_irq(irqdev->irq, irqdev);
	irqdev->irq = 0;
}


