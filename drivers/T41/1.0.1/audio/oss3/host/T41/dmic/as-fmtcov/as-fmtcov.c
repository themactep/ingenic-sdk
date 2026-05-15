#include <linux/module.h>
#include <linux/device.h>
#include <linux/list.h>
#include <sound/soc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <sound/pcm_params.h>
#include <asm/io.h>
#include "as-fmtcov.h"
#include "../../../../include/audio_debug.h"

static struct ingenic_as_fmtcov *as_fmtcov;

int ingenic_dmic_fmtcov_cfg(u8 chn_id, int format, int channels)
{
	u32 fmtcfg = 0;
	fmtcfg = DFCR_SS(format) | DFCR_CHNUM(channels);
	writel_relaxed(fmtcfg, as_fmtcov->io_base + DFCR(chn_id));
	return 0;
}

void ingenic_dmic_fmtcov_enable(u8 chn_id, bool enable)
{
	regmap_update_bits(as_fmtcov->regmap, DFCR(chn_id), DFCR_ENABLE, enable ? DFCR_ENABLE : 0);
}

static int ingenic_dmic_fmtcov_platform_probe(struct platform_device *pdev)
{
	struct regmap_config regmap_config = {
		.reg_bits = 32,
		.reg_stride = 4,
		.val_bits = 32,
		.cache_type = REGCACHE_NONE,
	};

	as_fmtcov = devm_kzalloc(&pdev->dev, sizeof(struct ingenic_as_fmtcov), GFP_KERNEL);
	if (!as_fmtcov){
		audio_err_print("ingenic_as_fmtcov kzalloc is failed.\n");
		return -ENOMEM;
	}
	as_fmtcov->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	as_fmtcov->io_base = devm_ioremap_resource(&pdev->dev, as_fmtcov->res);
	if (IS_ERR(as_fmtcov->io_base)){
		audio_err_print("devm_ioremap_resource is failed.\n");
		return PTR_ERR(as_fmtcov->io_base);
	}
	regmap_config.max_register = resource_size(as_fmtcov->res) - 0x4;
	as_fmtcov->regmap = devm_regmap_init_mmio(&pdev->dev, as_fmtcov->io_base,&regmap_config);
	if (IS_ERR(as_fmtcov->regmap)){
		audio_err_print("devm_regmap_init_mmio is failed.\n");
		return PTR_ERR(as_fmtcov->regmap);
	}
	as_fmtcov->dev = &pdev->dev;

	platform_set_drvdata(pdev, as_fmtcov);
    printk("fmtcov init ok\n");
	return 0;
}

static int ingenic_dmic_fmtcov_platform_remove(struct platform_device *pdev)
{
	return 0;
}
struct platform_driver ingenic_dmic_fmtcov_driver = {
	.probe = ingenic_dmic_fmtcov_platform_probe,
	.remove = ingenic_dmic_fmtcov_platform_remove,
	.driver = {
		.name = "dmic-fmtcov",
		.owner = THIS_MODULE,
	},
};
