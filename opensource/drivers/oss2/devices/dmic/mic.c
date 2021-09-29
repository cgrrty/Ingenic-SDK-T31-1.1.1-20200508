
#include <linux/interrupt.h>
#include "mic.h"

#include "dmic_hal.h"

struct mic_dev *g_mic_dev = NULL;

int mic_resource_init(
        struct platform_device *pdev,
        struct mic_dev *mic_dev, int type) {
    int ret;
    struct mic *mic = &mic_dev->dmic;

    mic->type = type;
    mic->mic_dev = mic_dev;

    mic->dma_res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
    if (!mic->dma_res) {
        dev_err(&pdev->dev, "failed to get platform dma resource\n");
        return -1;
    }

    mic->io_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!mic->io_res) {
        dev_err(&pdev->dev, "failed to get platform IO resource\n");
        return -1;
    }

    ret = (int)devm_request_mem_region(&pdev->dev,
            mic->io_res->start, resource_size(mic->io_res), pdev->name);
    if (!ret) {
        dev_err(&pdev->dev, "failed to request mem region");
        return -EBUSY;
    }

    mic->dma_type = GET_MAP_TYPE(mic->dma_res->start);

    mic->iomem = devm_ioremap_nocache(&pdev->dev,
            mic->io_res->start, resource_size(mic->io_res));
    if (!mic->iomem) {
        dev_err(&pdev->dev, "Failed to ioremap mmio memory");
        return -ENOMEM;
    }

    return 0;
}

extern int dmic_dma_config(struct mic_dev *mic_dev);

static int mic_probe(struct platform_device *pdev)
{
    struct mic_dev *mic_dev;
	struct mic_file_data *fdata = NULL;

    mic_dev = kmalloc(sizeof(struct mic_dev), GFP_KERNEL);
    if(!mic_dev) {
        printk("voice dev alloc failed\n");
        goto __err_mic;
    }
    memset(mic_dev, 0, sizeof(struct mic_dev));

    mic_dev->dev = &pdev->dev;
    dev_set_drvdata(mic_dev->dev, mic_dev);
    mic_dev->pdev = pdev;
	fdata = (struct mic_file_data *)kmalloc(sizeof(struct mic_file_data), GFP_KERNEL);

    memset(fdata, 0, sizeof(struct mic_file_data));

    fdata->is_enabled = 0;
    fdata->cnt = 0;
    fdata->periods_ms = INT_MAX;
	mic_dev->fdata = fdata;

	g_mic_dev = mic_dev;

	return 0;

__err_mic:
    return -EFAULT;
}

static const  struct platform_device_id mic_id_table[] = {
        { .name = "dmic", },
        {},
};

static struct platform_driver mic_platform_driver = {
        .probe = mic_probe,
        .driver = {
            .name = "dmic",
            .owner = THIS_MODULE,
        },
        .id_table = mic_id_table,
};

int dmic_drv_init(void)
{
    return platform_driver_register(&mic_platform_driver);
}

void dmic_drv_exit(void)
{
    platform_driver_unregister(&mic_platform_driver);
}
