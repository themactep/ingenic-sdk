#include <linux/proc_fs.h>
#include <sensor-info.h>

static struct sensor_info *sensor_info_ptr;

// Function prototypes for creating proc entries
static ssize_t sensor_name_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_chip_id_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_version_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_fps_min_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_fps_max_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_i2c_addr_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_width_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static ssize_t sensor_height_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);

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

void sensor_common_init(struct sensor_info *info) {
	sensor_info_ptr = info;

	// Create the /proc/jz/sensor directory
	proc_mkdir("jz/sensor", NULL);

	// Create proc entries under /proc/jz/sensor
	proc_create("jz/sensor/name", 0444, NULL, &name_fops);
	proc_create("jz/sensor/chip_id", 0444, NULL, &chip_id_fops);
	proc_create("jz/sensor/version", 0444, NULL, &version_fops);
	proc_create("jz/sensor/min_fps", 0444, NULL, &min_fps_fops);
	proc_create("jz/sensor/max_fps", 0444, NULL, &max_fps_fops);
	proc_create("jz/sensor/i2c_addr", 0444, NULL, &i2c_addr_fops);
	proc_create("jz/sensor/height", 0444, NULL, &height_fops);
	proc_create("jz/sensor/width", 0444, NULL, &width_fops);
}

void sensor_common_exit(void) {
	// Remove proc entries and the directory
	remove_proc_entry("jz/sensor/name", NULL);
	remove_proc_entry("jz/sensor/chip_id", NULL);
	remove_proc_entry("jz/sensor/version", NULL);
	remove_proc_entry("jz/sensor/min_fps", NULL);
	remove_proc_entry("jz/sensor/max_fps", NULL);
	remove_proc_entry("jz/sensor/i2c_addr", NULL);
	remove_proc_entry("jz/sensor/height", NULL);
	remove_proc_entry("jz/sensor/width", NULL);
	remove_proc_entry("jz/sensor", NULL);
}

static ssize_t sensor_name_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char buffer[128];
	int len = snprintf(buffer, sizeof(buffer), "%s\n", sensor_info_ptr->name);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_chip_id_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char chip_id_str[16]; // Enough for "0x" followed by 8 hex digits and a newline
	int len = snprintf(chip_id_str, sizeof(chip_id_str), "0x%x\n", sensor_info_ptr->chip_id);
	return simple_read_from_buffer(buf, count, ppos, chip_id_str, len);
}

static ssize_t sensor_version_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char buffer[128];
	int len = snprintf(buffer, sizeof(buffer), "%s\n", sensor_info_ptr->version);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_fps_min_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char buffer[32]; // Enough for an integer value and a newline
	int len = snprintf(buffer, sizeof(buffer), "%d\n", sensor_info_ptr->min_fps);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_fps_max_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", sensor_info_ptr->max_fps);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_i2c_addr_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char chip_i2c_addr_str[16];
	int len = snprintf(chip_i2c_addr_str, sizeof(chip_i2c_addr_str), "0x%x\n", sensor_info_ptr->chip_i2c_addr);
	return simple_read_from_buffer(buf, count, ppos, chip_i2c_addr_str, len);
}

static ssize_t sensor_width_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", sensor_info_ptr->width);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static ssize_t sensor_height_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%d\n", sensor_info_ptr->height);
	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}
