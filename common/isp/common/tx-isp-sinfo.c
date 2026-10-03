/*
 * tx-isp sensor info registry — /proc/jz/sensor
 *
 * Replaces the per-driver sensor-info.c boilerplate with live
 * introspection of registered sensors. Two-stage registration:
 *
 *   stage 1 (driver load, via private_i2c_add_driver hook in
 *   sensor-common.h): publishes the sensor name and default i2c
 *   address — the two values a streamer needs before it can call
 *   IMP_ISP_AddSensor (the i2c client, and therefore probe, does
 *   not exist until AddSensor creates it).
 *
 *   stage 2 (probe, via tx_isp_subdev_init hook): binds the live
 *   struct tx_isp_sensor. From then on every read reports actual
 *   state — client address/adapter, configured mbus geometry, fps.
 *
 * A value that is not yet knowable reads as an EMPTY file, never a
 * placeholder. Consumers (rvd, prudynt) treat unparseable/empty as
 * "fall back to config/defaults", whereas a fake "0" or "1" would
 * parse as valid.
 *
 * This file is compiled into the per-family tx-isp module; the
 * family include path selects the right struct variants. Sensor
 * modules symbol-depend on tx-isp, so these proc handlers can never
 * outlive a registered sensor (no rmmod use-after-free, which the
 * old per-module sensor-info.c had).
 */
#define TX_ISP_SINFO_NO_HOOK
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <tx-isp-common.h>

/* T10/T20 use the older v4l2-flavored ISP core: the sensor's subdev is
 * a struct v4l2_subdev and the client hangs off v4l2 subdevdata. */
#if defined(CONFIG_SOC_T10) || defined(CONFIG_SOC_T20)
#define SINFO_V4L2_FLAVOR 1
#include <media/v4l2-device.h>
typedef struct v4l2_subdev sinfo_subdev_t;
#else
typedef struct tx_isp_subdev sinfo_subdev_t;
#endif

#define SINFO_MAX_SENSORS 4

/* Board-wiring defaults are a per-SoC property, not per-sensor. */
#if defined(CONFIG_SOC_T40)
#define SINFO_DEF_I2C_ADAPTER 1
#else
#define SINFO_DEF_I2C_ADAPTER 0
#endif

/*
 * T40-class families carry board wiring (mclk/boot/interface, int
 * gpios) in tx_isp_sensor_register_info; T31-class families do not,
 * so those proc files simply don't exist there. min/max_fps live in
 * tx_isp_video_in only on T40-class.
 */
#if defined(CONFIG_SOC_T40) || defined(CONFIG_SOC_T41)
#define SINFO_HAVE_REGINFO_WIRING 1
#endif
/* T41 only: T40's tx_isp_video_in lacks min_fps/max_fps */
#if defined(CONFIG_SOC_T41)
#define SINFO_HAVE_VIDEO_MINMAX_FPS 1
#endif

/*
 * T32 (vendor codename PRJ007) carries the board wiring too, and nests the
 * fps triple in a tx_isp_sensor_item (value/min/max) instead of three flat
 * unsigned ints. The packed num<<16|den encoding is unchanged, so only the
 * member path differs.
 */
#if defined(CONFIG_SOC_PRJ007)
#define SINFO_HAVE_REGINFO_WIRING 1
#define SINFO_HAVE_VIDEO_MINMAX_FPS 1
#define SINFO_VIDEO_FPS(v)     ((v).fps.value)
#define SINFO_VIDEO_MIN_FPS(v) ((v).fps.min)
#define SINFO_VIDEO_MAX_FPS(v) ((v).fps.max)
#else
#define SINFO_VIDEO_FPS(v)     ((v).fps)
#define SINFO_VIDEO_MIN_FPS(v) ((v).min_fps)
#define SINFO_VIDEO_MAX_FPS(v) ((v).max_fps)
#endif

enum sinfo_key {
	SINFO_NAME,
	SINFO_CHIP_ID,
	SINFO_I2C_ADDR,
	SINFO_I2C_ADAPTER,
	SINFO_WIDTH,
	SINFO_HEIGHT,
	SINFO_FPS,
	SINFO_STATUS,
	SINFO_MIN_FPS,
	SINFO_MAX_FPS,
	SINFO_MCLK,
	SINFO_BOOT,
	SINFO_VIDEO_INTERFACE,
	SINFO_RST_GPIO,
	SINFO_PWDN_GPIO,
	SINFO_NKEYS
};

static const char *const sinfo_key_name[SINFO_NKEYS] = {
	[SINFO_NAME]            = "name",
	[SINFO_CHIP_ID]         = "chip_id",
	[SINFO_I2C_ADDR]        = "i2c_addr",
	[SINFO_I2C_ADAPTER]     = "i2c_adapter",
	[SINFO_WIDTH]           = "width",
	[SINFO_HEIGHT]          = "height",
	[SINFO_FPS]             = "fps",
	[SINFO_STATUS]          = "status",
	[SINFO_MIN_FPS]         = "min_fps",
	[SINFO_MAX_FPS]         = "max_fps",
	[SINFO_MCLK]            = "mclk",
	[SINFO_BOOT]            = "boot",
	[SINFO_VIDEO_INTERFACE] = "video_interface",
	[SINFO_RST_GPIO]        = "rst_gpio",
	[SINFO_PWDN_GPIO]       = "pwdn_gpio",
};

/* Keys this family publishes. */
static bool sinfo_key_supported(enum sinfo_key key)
{
	switch (key) {
#ifndef SINFO_HAVE_VIDEO_MINMAX_FPS
	case SINFO_MIN_FPS:
	case SINFO_MAX_FPS:
		return false;
#endif
#ifndef SINFO_HAVE_REGINFO_WIRING
	case SINFO_MCLK:
	case SINFO_BOOT:
	case SINFO_VIDEO_INTERFACE:
	case SINFO_RST_GPIO:
	case SINFO_PWDN_GPIO:
		return false;
#endif
	default:
		return true;
	}
}

struct sinfo_slot;

struct sinfo_file {
	struct sinfo_slot *slot;
	enum sinfo_key key;
};

struct sinfo_slot {
	bool used;
	struct i2c_driver *drv;         /* stage 1 */
	struct module *owner;
	unsigned short def_i2c_addr;
	struct tx_isp_sensor *sensor;   /* stage 2, NULL until probe */
	struct proc_dir_entry *dir;
	char dirname[16];
	struct sinfo_file files[SINFO_NKEYS];
};

static struct sinfo_slot sinfo_slots[SINFO_MAX_SENSORS];
static DEFINE_MUTEX(sinfo_lock);
static struct proc_dir_entry *sinfo_root;

/* Live attribute pointer: drivers assign video.attr before subdev init. */
static struct tx_isp_sensor_attribute *slot_attr(struct sinfo_slot *s)
{
	if (s->sensor && s->sensor->video.attr)
		return s->sensor->video.attr;
	return NULL;
}

static struct i2c_client *slot_client(struct sinfo_slot *s)
{
	if (!s->sensor)
		return NULL;
#ifdef SINFO_V4L2_FLAVOR
	return v4l2_get_subdevdata(&s->sensor->sd);
#else
	return tx_isp_get_subdevdata(&s->sensor->sd);
#endif
}

static int sinfo_show(struct seq_file *m, void *v)
{
	struct sinfo_file *f = m->private;
	struct sinfo_slot *s = f->slot;
	struct tx_isp_sensor_attribute *attr;
	struct i2c_client *client;

	mutex_lock(&sinfo_lock);
	if (!s->used) {
		mutex_unlock(&sinfo_lock);
		return 0;
	}
	attr = slot_attr(s);
	client = slot_client(s);

	switch (f->key) {
	case SINFO_NAME:
		if (attr && attr->name)
			seq_printf(m, "%s\n", attr->name);
		else if (s->drv)
			seq_printf(m, "%s\n", s->drv->driver.name);
		break;
	case SINFO_CHIP_ID:
		if (attr)
			seq_printf(m, "0x%x\n", attr->chip_id);
		break;
	case SINFO_I2C_ADDR:
		if (client)
			seq_printf(m, "0x%x\n", client->addr);
		else
			seq_printf(m, "0x%x\n", s->def_i2c_addr);
		break;
	case SINFO_I2C_ADAPTER:
		if (client && client->adapter)
			seq_printf(m, "%d\n", client->adapter->nr);
		else
			seq_printf(m, "%d\n", SINFO_DEF_I2C_ADAPTER);
		break;
	case SINFO_WIDTH:
		if (s->sensor && s->sensor->video.mbus.width)
			seq_printf(m, "%d\n", s->sensor->video.mbus.width);
		break;
	case SINFO_HEIGHT:
		if (s->sensor && s->sensor->video.mbus.height)
			seq_printf(m, "%d\n", s->sensor->video.mbus.height);
		break;
	case SINFO_FPS:
		if (s->sensor && SINFO_VIDEO_FPS(s->sensor->video)) {
			unsigned int fps = SINFO_VIDEO_FPS(s->sensor->video);
			unsigned int den = fps & 0xffff;
			seq_printf(m, "%u\n", (fps >> 16) / (den ? den : 1));
		}
		break;
	case SINFO_STATUS:
		seq_printf(m, "%s\n", s->sensor ? "active" : "loaded");
		break;
#ifdef SINFO_HAVE_VIDEO_MINMAX_FPS
	case SINFO_MIN_FPS:
		if (s->sensor && SINFO_VIDEO_MIN_FPS(s->sensor->video)) {
			unsigned int fps = SINFO_VIDEO_MIN_FPS(s->sensor->video);
			unsigned int den = fps & 0xffff;
			seq_printf(m, "%u\n", (fps >> 16) / (den ? den : 1));
		}
		break;
	case SINFO_MAX_FPS:
		if (s->sensor && SINFO_VIDEO_MAX_FPS(s->sensor->video)) {
			unsigned int fps = SINFO_VIDEO_MAX_FPS(s->sensor->video);
			unsigned int den = fps & 0xffff;
			seq_printf(m, "%u\n", (fps >> 16) / (den ? den : 1));
		}
		break;
#endif
#ifdef SINFO_HAVE_REGINFO_WIRING
	/*
	 * Wiring values come from the register_info userspace passed to
	 * AddSensor; pre-bind they read as the per-SoC defaults. The
	 * enum fields are unsigned, hence the int casts.
	 */
	case SINFO_MCLK:
		if (s->sensor)
			seq_printf(m, "%d\n", (int)s->sensor->info.mclk);
		else
			seq_printf(m, "1\n");
		break;
	case SINFO_BOOT:
		if (s->sensor)
			seq_printf(m, "%d\n", (int)s->sensor->info.default_boot);
		else
			seq_printf(m, "0\n");
		break;
	case SINFO_VIDEO_INTERFACE:
		if (s->sensor)
			seq_printf(m, "%d\n", (int)s->sensor->info.video_interface);
		else
			seq_printf(m, "0\n");
		break;
	case SINFO_RST_GPIO:
		if (s->sensor)
			seq_printf(m, "%d\n", s->sensor->info.rst_gpio);
		else
			seq_printf(m, "-1\n");
		break;
	case SINFO_PWDN_GPIO:
		if (s->sensor)
			seq_printf(m, "%d\n", s->sensor->info.pwdn_gpio);
		else
			seq_printf(m, "-1\n");
		break;
#endif
	default:
		break;
	}
	mutex_unlock(&sinfo_lock);
	return 0;
}

static int sinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, sinfo_show, PDE_DATA(inode));
}

static const struct file_operations sinfo_fops = {
	.owner   = THIS_MODULE,
	.open    = sinfo_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int sinfo_count_show(struct seq_file *m, void *v)
{
	int i, n = 0;

	mutex_lock(&sinfo_lock);
	for (i = 0; i < SINFO_MAX_SENSORS; i++)
		if (sinfo_slots[i].used)
			n++;
	mutex_unlock(&sinfo_lock);
	seq_printf(m, "%d\n", n);
	return 0;
}

static int sinfo_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, sinfo_count_show, NULL);
}

static const struct file_operations sinfo_count_fops = {
	.owner   = THIS_MODULE,
	.open    = sinfo_count_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

/* Caller holds sinfo_lock. */
static void sinfo_slot_publish(struct sinfo_slot *s, int index)
{
	int k;

	snprintf(s->dirname, sizeof(s->dirname), "sensor%d", index);
	s->dir = proc_mkdir(s->dirname, sinfo_root);
	if (!s->dir)
		return;

	for (k = 0; k < SINFO_NKEYS; k++) {
		if (!sinfo_key_supported(k))
			continue;
		s->files[k].slot = s;
		s->files[k].key = k;
		proc_create_data(sinfo_key_name[k], 0444, s->dir,
				 &sinfo_fops, &s->files[k]);
	}
}

/* Caller holds sinfo_lock. */
static void sinfo_slot_unpublish(struct sinfo_slot *s)
{
	if (s->dir) {
		remove_proc_subtree(s->dirname, sinfo_root);
		s->dir = NULL;
	}
}

int tx_isp_sinfo_driver_add(struct i2c_driver *drv, int def_i2c_addr,
			    struct module *owner)
{
	int i;

	if (!drv || !sinfo_root)
		return -EINVAL;

	mutex_lock(&sinfo_lock);
	for (i = 0; i < SINFO_MAX_SENSORS; i++) {
		if (!sinfo_slots[i].used) {
			struct sinfo_slot *s = &sinfo_slots[i];

			s->used = true;
			s->drv = drv;
			s->owner = owner;
			s->def_i2c_addr = (unsigned short)def_i2c_addr;
			s->sensor = NULL;
			sinfo_slot_publish(s, i);
			break;
		}
	}
	mutex_unlock(&sinfo_lock);
	if (i == SINFO_MAX_SENSORS) {
		pr_warn("tx-isp-sinfo: no free sensor slot for %s\n",
			drv->driver.name);
		return -ENOSPC;
	}
	return 0;
}
EXPORT_SYMBOL(tx_isp_sinfo_driver_add);

void tx_isp_sinfo_driver_del(struct i2c_driver *drv)
{
	int i;

	mutex_lock(&sinfo_lock);
	for (i = 0; i < SINFO_MAX_SENSORS; i++) {
		struct sinfo_slot *s = &sinfo_slots[i];

		if (s->used && s->drv == drv) {
			sinfo_slot_unpublish(s);
			s->used = false;
			s->drv = NULL;
			s->owner = NULL;
			s->sensor = NULL;
		}
	}
	mutex_unlock(&sinfo_lock);
}
EXPORT_SYMBOL(tx_isp_sinfo_driver_del);

int tx_isp_sinfo_sensor_bind(sinfo_subdev_t *sd, struct module *owner)
{
	struct tx_isp_sensor *sensor;
	int i;

	if (!sd)
		return -EINVAL;
	sensor = container_of(sd, struct tx_isp_sensor, sd);

	mutex_lock(&sinfo_lock);
	/* First unbound slot owned by this module. */
	for (i = 0; i < SINFO_MAX_SENSORS; i++) {
		struct sinfo_slot *s = &sinfo_slots[i];

		if (s->used && s->owner == owner && !s->sensor) {
			s->sensor = sensor;
			break;
		}
	}
	/* Second probe of the same module (dual same-model): clone a slot. */
	if (i == SINFO_MAX_SENSORS) {
		for (i = 0; i < SINFO_MAX_SENSORS; i++) {
			struct sinfo_slot *s = &sinfo_slots[i];

			if (!s->used) {
				struct sinfo_slot *first = NULL;
				int j;

				for (j = 0; j < SINFO_MAX_SENSORS; j++) {
					if (sinfo_slots[j].used &&
					    sinfo_slots[j].owner == owner) {
						first = &sinfo_slots[j];
						break;
					}
				}
				s->used = true;
				s->drv = first ? first->drv : NULL;
				s->owner = owner;
				s->def_i2c_addr = first ? first->def_i2c_addr : 0;
				s->sensor = sensor;
				sinfo_slot_publish(s, i);
				break;
			}
		}
	}
	mutex_unlock(&sinfo_lock);
	return 0;
}
EXPORT_SYMBOL(tx_isp_sinfo_sensor_bind);

void tx_isp_sinfo_sensor_unbind(sinfo_subdev_t *sd, struct module *owner)
{
	struct tx_isp_sensor *sensor;
	int i;

	if (!sd)
		return;
	sensor = container_of(sd, struct tx_isp_sensor, sd);

	mutex_lock(&sinfo_lock);
	for (i = 0; i < SINFO_MAX_SENSORS; i++) {
		struct sinfo_slot *s = &sinfo_slots[i];

		if (s->used && s->sensor == sensor)
			s->sensor = NULL;
	}
	mutex_unlock(&sinfo_lock);
}
EXPORT_SYMBOL(tx_isp_sinfo_sensor_unbind);

int tx_isp_sinfo_init(void)
{
	/* /proc/jz is created by the platform kernel. */
	sinfo_root = proc_mkdir("jz/sensor", NULL);
	if (!sinfo_root) {
		pr_warn("tx-isp-sinfo: cannot create /proc/jz/sensor\n");
		return 0; /* non-fatal for the ISP itself */
	}
	proc_create("count", 0444, sinfo_root, &sinfo_count_fops);
	return 0;
}

void tx_isp_sinfo_exit(void)
{
	mutex_lock(&sinfo_lock);
	memset(sinfo_slots, 0, sizeof(sinfo_slots));
	mutex_unlock(&sinfo_lock);
	if (sinfo_root) {
		remove_proc_subtree("jz/sensor", NULL);
		sinfo_root = NULL;
	}
}
