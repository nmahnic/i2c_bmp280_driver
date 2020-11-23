#define MENOR          0
#define CANT_DISP      1
#define DEVICE_NAME    "NM_td3_i2c_dev"
#define CLASS_NAME     "NM_td3_i2c_class"
#define COMPATIBLE	   "NM_td3_i2c_dev"

// Prototipos
static int i2c_probe(struct platform_device * i2c_pd);
static int i2c_remove(struct platform_device * i2c_pd);
static int NMopen(struct inode *inode, struct file *file);
static int NMrelease(struct inode *inode, struct file *file);
static ssize_t NMread (struct file * device_descriptor, char __user * user_buffer, size_t read_len, loff_t * my_loff_t);
static ssize_t NMwrite (struct file * device_descriptor, const char __user * user_buffer, size_t write_len, loff_t * my_loff_t);
irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs);
static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env);
void writeBMP280 (char *writeData, int writeData_size);
uint8_t readByteBMP280(void);


static struct {
	dev_t myi2c;

	struct cdev * myi2c_cdev;
	struct device * myi2c_device;
	struct class * myi2c_class;
} state;

static struct {
	int pos_rx;
	int buff_rx_len;
	int data_rx;

	int pos_tx;
	int buff_tx_len;
	int data_tx;

	char * buff_rx;
	char * buff_tx;
} data_i2c;

struct file_operations i2c_ops = {
	.owner = THIS_MODULE,
	.open = NMopen,
	.read = NMread,
	.write = NMwrite,
	.release = NMrelease,
	//.ioctl = NMioctl,
};

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nicolas Mahnic R5054");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("TD3_MYI2C LKM");