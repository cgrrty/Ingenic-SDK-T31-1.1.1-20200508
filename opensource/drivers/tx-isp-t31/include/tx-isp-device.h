#ifndef __TX_ISP_DEVICE_H__
#define __TX_ISP_DEVICE_H__

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "txx-isp.h"

enum tx_isp_subdev_id {
	TX_ISP_CORE_SUBDEV_ID,
	TX_ISP_MAX_SUBDEV_ID,
};

#define TX_ISP_ENTITY_ENUM_MAX_DEPTH	16
struct tx_isp_module;
struct tx_isp_irq_device;
struct tx_isp_subdev;
struct tx_isp_subdev_pad;
struct tx_isp_subdev_link;

struct tx_isp_module {
	struct tx_isp_descriptor desc;
	struct device *dev;
	const char *name;
	struct miscdevice miscdev;
	/*struct list_head list;*/

	/* the interface */
	struct file_operations *ops;

	/* the interface */
	struct file_operations *debug_ops;

	/* the list header of sub-modules */
	struct tx_isp_module *submods[TX_ISP_ENTITY_ENUM_MAX_DEPTH];
	/* the module's parent */
	void *parent;
	int (*notify)(struct tx_isp_module *module, unsigned int notification, void *data);
};

/* Description of the connection between modules */
struct link_pad_description {
	char *name; 		// the module name
	unsigned char type;	// the pad type
	unsigned char index;	// the index in array of some pad type
};

struct tx_isp_link_config {
	struct link_pad_description src;
	struct link_pad_description dst;
	unsigned int flag;
};

struct tx_isp_link_configs {
	struct tx_isp_link_config *config;
	unsigned int length;
};

/* The description of module entity */
struct tx_isp_subdev_link {
	struct tx_isp_subdev_pad *source;	/* Source pad */
	struct tx_isp_subdev_pad *sink;		/* Sink pad  */
	struct tx_isp_subdev_link *reverse;	/* Link in the reverse direction */
	unsigned int flag;				/* Link flag (TX_ISP_LINKTYPE_*) */
	unsigned int state;				/* Link state (TX_ISP_MODULE_*) */
};

struct tx_isp_subdev_pad {
	struct tx_isp_subdev *sd;	/* Subdev this pad belongs to */
	unsigned char index;			/* Pad index in the entity pads array */
	unsigned char type;			/* Pad type (TX_ISP_PADTYPE_*) */
	unsigned char links_type;			/* Pad link type (TX_ISP_PADLINK_*) */
	unsigned char state;				/* Pad state (TX_ISP_PADSTATE_*) */
	struct tx_isp_subdev_link link;	/* The active link */
	int (*event)(struct tx_isp_subdev_pad *, unsigned int event, void *data);
	void *priv;
};

struct tx_isp_dbg_register {
	char *name;
	unsigned int size;
	unsigned long long reg;
	unsigned long long val;
};

struct tx_isp_chip_ident {
	char name[32];
	char *revision;
	unsigned int ident;
};

struct tx_isp_subdev_core_ops {
	int (*g_chip_ident)(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip);
	int (*init)(struct tx_isp_subdev *sd, int on);		// clk's, power's and init ops.
	int (*reset)(struct tx_isp_subdev *sd, int on);
	int (*g_register)(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg);
	int (*s_register)(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg);
	int (*ioctl)(struct tx_isp_subdev *sd, unsigned int cmd, void *arg);
	irqreturn_t (*interrupt_service_routine)(struct tx_isp_subdev *sd, u32 status, bool *handled);
	irqreturn_t (*interrupt_service_thread)(struct tx_isp_subdev *sd, void *data);
};

struct tx_isp_subdev_video_ops {
	int (*s_stream)(struct tx_isp_subdev *sd, int enable);
	int (*link_stream)(struct tx_isp_subdev *sd, int enable);
	int (*link_setup)(const struct tx_isp_subdev_pad *local,
			  const struct tx_isp_subdev_pad *remote, u32 flags);
};

struct tx_isp_subdev_sensor_ops {
	int (*release_all_sensor)(struct tx_isp_subdev *sd);
	int (*sync_sensor_attr)(struct tx_isp_subdev *sd, void *arg);
	int (*ioctl)(struct tx_isp_subdev *sd, unsigned int cmd, void *arg);
};

struct tx_isp_subdev_pad_ops {
	int (*g_fmt)(struct tx_isp_subdev *sd, struct v4l2_format *f);
	int (*s_fmt)(struct tx_isp_subdev *sd, struct v4l2_format *f);
	int (*streamon)(struct tx_isp_subdev *sd, void *data);
	int (*streamoff)(struct tx_isp_subdev *sd, void *data);
};

struct tx_isp_irq_device {
	spinlock_t slock;
	/*struct mutex mlock;*/
	int irq;
	void (*enable_irq)(struct tx_isp_irq_device *irq_dev);
	void (*disable_irq)(struct tx_isp_irq_device *irq_dev);
};

enum tx_isp_module_state {
	TX_ISP_MODULE_UNDEFINE = 0,
	TX_ISP_MODULE_SLAKE,
	TX_ISP_MODULE_ACTIVATE,
	TX_ISP_MODULE_DEINIT = TX_ISP_MODULE_ACTIVATE,
	TX_ISP_MODULE_INIT,
	TX_ISP_MODULE_RUNNING,
};

/*
 * Internal ops. Never call this from drivers, only the tx isp device can call
 * these ops.
 *
 * activate_module: called when this subdev is enabled. When called the module
 * could be operated;
 *
 * slake_module: called when this subdev is disabled. When called the
 *	module couldn't be operated.
 *
 */
struct tx_isp_subdev_internal_ops {
	int (*activate_module)(struct tx_isp_subdev *sd);
	int (*slake_module)(struct tx_isp_subdev *sd);
};

struct tx_isp_subdev_ops {
	struct tx_isp_subdev_core_ops		*core;
	struct tx_isp_subdev_video_ops		*video;
	struct tx_isp_subdev_pad_ops		*pad;
	struct tx_isp_subdev_sensor_ops		*sensor;
	struct tx_isp_subdev_internal_ops	*internal;
};

#define tx_isp_call_module_notify(ent, args...)				\
	(!(ent) ? -ENOENT : (((ent)->notify) ?				\
			     (ent)->notify(((ent)->parent), ##args) : -ENOIOCTLCMD))

#define tx_isp_call_subdev_notify(ent, args...)				\
	(!(ent) ? -ENOENT : (((ent)->module.notify) ?			\
			     ((ent)->module.notify(&((ent)->module), ##args)): -ENOIOCTLCMD))

#define tx_isp_call_subdev_event(ent, args...)				\
	(!(ent) ? -ENOENT : (((ent)->event) ?				\
			     (ent)->event((ent), ##args) : -ENOIOCTLCMD))

#define tx_isp_subdev_call(sd, o, f, args...)				\
	(!(sd) ? -ENODEV : (((sd)->ops->o && (sd)->ops->o->f) ?		\
			    (sd)->ops->o->f((sd) , ##args) : -ENOIOCTLCMD))

struct tx_isp_subdev {
	struct tx_isp_module module;
	struct tx_isp_irq_device irqdev;
	struct tx_isp_chip_ident chip;

	/* basic members */
	struct resource *res;
	void __iomem *base;
	struct clk **clks;
	unsigned int clk_num;
	struct tx_isp_subdev_ops *ops;

	/* expanded members */
	unsigned short num_outpads;			/* Number of sink pads */
	unsigned short num_inpads;			/* Number of source pads */

	struct tx_isp_subdev_pad *outpads;		/* OutPads array (num_pads elements) */
	struct tx_isp_subdev_pad *inpads;		/* InPads array (num_pads elements) */

	void *dev_priv;
	void *host_priv;
};

#define TX_ISP_PLATFORM_MAX_NUM 16

struct tx_isp_platform {
	struct platform_device *dev;
	struct platform_driver *drv;
};

struct tx_isp_device {
	struct tx_isp_module module;
	unsigned int pdev_num;
	struct tx_isp_platform pdevs[TX_ISP_PLATFORM_MAX_NUM];

	char *version;
	struct mutex mlock;
	spinlock_t slock;
	int refcnt;

	int active_link;
	/* debug parameters */
	struct proc_dir_entry *proc;
};

#define miscdev_to_module(mdev) (container_of(mdev, struct tx_isp_module, miscdev))
#define module_to_subdev(mod) (container_of(mod, struct tx_isp_subdev, module))
#define irqdev_to_subdev(dev) (container_of(dev, struct tx_isp_subdev, irqdev))
#define module_to_ispdev(mod) (container_of(mod, struct tx_isp_device, module))


#define tx_isp_sd_readl(sd, reg)		\
	tx_isp_readl(((sd)->base), reg)
#define tx_isp_sd_writel(sd, reg, value)	\
	tx_isp_writel(((sd)->base), reg, value)

int private_reset_tx_isp_module(enum tx_isp_subdev_id id);
int tx_isp_reg_set(struct tx_isp_subdev *sd, unsigned int reg, int start, int end, int val);

int tx_isp_subdev_init(struct platform_device *pdev, struct tx_isp_subdev *sd, struct tx_isp_subdev_ops *ops);
void tx_isp_subdev_deinit(struct tx_isp_subdev *sd);
int tx_isp_send_event_to_remote(struct tx_isp_subdev_pad *pad, unsigned int cmd, void *data);

static inline void tx_isp_set_module_nodeops(struct tx_isp_module *module, struct file_operations *ops)
{
	module->ops = ops;
}

static inline void tx_isp_set_module_debugops(struct tx_isp_module *module, struct file_operations *ops)
{
	module->debug_ops = ops;
}

static inline void tx_isp_set_subdev_nodeops(struct tx_isp_subdev *sd, struct file_operations *ops)
{
	tx_isp_set_module_nodeops(&sd->module, ops);
}

static inline void tx_isp_set_subdev_debugops(struct tx_isp_subdev *sd, struct file_operations *ops)
{
	tx_isp_set_module_debugops(&sd->module, ops);
}

static inline void tx_isp_set_subdevdata(struct tx_isp_subdev *sd, void *data)
{
	sd->dev_priv = data;
}

static inline void *tx_isp_get_subdevdata(struct tx_isp_subdev *sd)
{
	return sd->dev_priv;
}

static inline void tx_isp_set_subdev_hostdata(struct tx_isp_subdev *sd, void *data)
{
	sd->host_priv = data;
}

static inline void *tx_isp_get_subdev_hostdata(struct tx_isp_subdev *sd)
{
	return sd->host_priv;
}
#endif/*__TX_ISP_DEVICE_H__*/
