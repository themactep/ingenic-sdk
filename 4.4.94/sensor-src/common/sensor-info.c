#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <sensor-info.h>

/* Per-sensor proc context for multi-sensor support */
struct sensor_proc_ctx {
	struct sensor_info *info;
	struct proc_dir_entry *dir;
	char dir_path[64];
};

// Function prototypes for creating proc entries
static ssize_t sensor_name_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_chip_id_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_version_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_fps_min_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_fps_max_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_i2c_addr_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_width_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_height_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_rst_gpio_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_pwdn_gpio_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_boot_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_mclk_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_video_interface_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_i2c_adapter_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);

// File operations for the proc entries
static const struct file_operations name_fops = {
	.read = sensor_name_read,
	.owner = THIS_MODULE,
};

static const struct file_operations chip_id_fops = {
	.read = sensor_chip_id_read,
	.owner = THIS_MODULE,
};

static const struct file_operations version_fops = {
	.read = sensor_version_read,
	.owner = THIS_MODULE,
};

static const struct file_operations min_fps_fops = {
	.read = sensor_fps_min_read,
	.owner = THIS_MODULE,
};

static const struct file_operations max_fps_fops = {
	.read = sensor_fps_max_read,
	.owner = THIS_MODULE,
};

static const struct file_operations i2c_addr_fops = {
	.read = sensor_i2c_addr_read,
	.owner = THIS_MODULE,
};

static const struct file_operations width_fops = {
	.read = sensor_width_read,
	.owner = THIS_MODULE,
};

static const struct file_operations height_fops = {
	.read = sensor_height_read,
	.owner = THIS_MODULE,
};

static const struct file_operations rst_gpio_fops = {
	.read = sensor_rst_gpio_read,
	.owner = THIS_MODULE,
};

static const struct file_operations pwdn_gpio_fops = {
	.read = sensor_pwdn_gpio_read,
	.owner = THIS_MODULE,
};

static const struct file_operations boot_fops = {
	.read = sensor_boot_read,
	.owner = THIS_MODULE,
};

static const struct file_operations mclk_fops = {
	.read = sensor_mclk_read,
	.owner = THIS_MODULE,
};

static const struct file_operations video_interface_fops = {
	.read = sensor_video_interface_read,
	.owner = THIS_MODULE,
};

static const struct file_operations i2c_adapter_fops = {
	.read = sensor_i2c_adapter_read,
	.owner = THIS_MODULE,
};

/* Track if legacy flat paths have been created (for backward compatibility) */
static int legacy_paths_created = 0;

void sensor_common_init(struct sensor_info *info) {
	struct sensor_proc_ctx *ctx;
	char path[80];

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return;

	ctx->info = info;
	info->priv = ctx;

	/* Create per-sensor directory: /proc/jz/sensor/<sensor_name>/ */
	snprintf(ctx->dir_path, sizeof(ctx->dir_path), "jz/sensor/%s", info->name);

	/* The platform owns /proc/jz; only create the sensor subtree here. */
	proc_mkdir("jz/sensor", NULL);
	ctx->dir = proc_mkdir(ctx->dir_path, NULL);

	/* Create proc entries under /proc/jz/sensor/<sensor_name>/ */
	snprintf(path, sizeof(path), "%s/name", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &name_fops, ctx);

	snprintf(path, sizeof(path), "%s/chip_id", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &chip_id_fops, ctx);

	snprintf(path, sizeof(path), "%s/version", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &version_fops, ctx);

	snprintf(path, sizeof(path), "%s/min_fps", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &min_fps_fops, ctx);

	snprintf(path, sizeof(path), "%s/max_fps", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &max_fps_fops, ctx);

	snprintf(path, sizeof(path), "%s/i2c_addr", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &i2c_addr_fops, ctx);

	snprintf(path, sizeof(path), "%s/height", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &height_fops, ctx);

	snprintf(path, sizeof(path), "%s/width", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &width_fops, ctx);

	snprintf(path, sizeof(path), "%s/rst_gpio", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &rst_gpio_fops, ctx);

	snprintf(path, sizeof(path), "%s/pwdn_gpio", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &pwdn_gpio_fops, ctx);

	snprintf(path, sizeof(path), "%s/boot", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &boot_fops, ctx);

	snprintf(path, sizeof(path), "%s/mclk", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &mclk_fops, ctx);

	snprintf(path, sizeof(path), "%s/video_interface", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &video_interface_fops, ctx);

	snprintf(path, sizeof(path), "%s/i2c_adapter", ctx->dir_path);
	proc_create_data(path, 0444, NULL, &i2c_adapter_fops, ctx);

	/* Create legacy flat paths for backward compatibility (first sensor only) */
	if (!legacy_paths_created) {
		legacy_paths_created = 1;
		proc_create_data("jz/sensor/name", 0444, NULL, &name_fops, ctx);
		proc_create_data("jz/sensor/chip_id", 0444, NULL, &chip_id_fops, ctx);
		proc_create_data("jz/sensor/version", 0444, NULL, &version_fops, ctx);
		proc_create_data("jz/sensor/min_fps", 0444, NULL, &min_fps_fops, ctx);
		proc_create_data("jz/sensor/max_fps", 0444, NULL, &max_fps_fops, ctx);
		proc_create_data("jz/sensor/i2c_addr", 0444, NULL, &i2c_addr_fops, ctx);
		proc_create_data("jz/sensor/height", 0444, NULL, &height_fops, ctx);
		proc_create_data("jz/sensor/width", 0444, NULL, &width_fops, ctx);
		proc_create_data("jz/sensor/rst_gpio", 0444, NULL, &rst_gpio_fops, ctx);
		proc_create_data("jz/sensor/pwdn_gpio", 0444, NULL, &pwdn_gpio_fops, ctx);
		proc_create_data("jz/sensor/boot", 0444, NULL, &boot_fops, ctx);
		proc_create_data("jz/sensor/mclk", 0444, NULL, &mclk_fops, ctx);
		proc_create_data("jz/sensor/video_interface", 0444, NULL, &video_interface_fops, ctx);
		proc_create_data("jz/sensor/i2c_adapter", 0444, NULL, &i2c_adapter_fops, ctx);
	}
}

void sensor_common_exit(void) {
	/* Note: Each sensor driver should call this with its own info pointer */
	/* For now, we rely on the proc entries being cleaned up when the module unloads */
}

static ssize_t sensor_name_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[128];
	int len = snprintf(buffer, sizeof(buffer), "%s\n", ctx->info->name);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_chip_id_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char chip_id_str[16];
	int len = snprintf(chip_id_str, sizeof(chip_id_str), "0x%x\n", ctx->info->chip_id);
	return simple_read_from_buffer(buf, count, ppos, chip_id_str, len);
}

static ssize_t sensor_version_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[128];
	int len = snprintf(buffer, sizeof(buffer), "%s\n", ctx->info->version);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_fps_min_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->min_fps);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_fps_max_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->max_fps);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_i2c_addr_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char chip_i2c_addr_str[16];
	int len = snprintf(chip_i2c_addr_str, sizeof(chip_i2c_addr_str), "0x%x\n", ctx->info->chip_i2c_addr);
	return simple_read_from_buffer(buf, count, ppos, chip_i2c_addr_str, len);
}

static ssize_t sensor_width_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->width);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_height_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->height);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_rst_gpio_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->rst_gpio);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_pwdn_gpio_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->pwdn_gpio);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_boot_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->boot);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_mclk_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->mclk);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_video_interface_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->video_interface);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_i2c_adapter_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	struct sensor_proc_ctx *ctx = PDE_DATA(file_inode(file));
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", ctx->info->i2c_adapter);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}
