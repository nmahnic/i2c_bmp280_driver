#define MENOR          0
#define CANT_DISP      1
#define DEVICE_NAME    "NM_td3_i2c_dev"
#define CLASS_NAME     "NM_td3_i2c_class"
#define COMPATIBLE	   "NM_td3_i2c_dev"

// Prototipos
static int i2c_probe(struct platform_device * i2c_pd);
static int i2c_remove(struct platform_device * i2c_pd);
void set_registers (void __iomem *base, uint32_t offset, uint32_t mask, uint32_t value);
void check_registers (void __iomem *base, uint32_t offset, uint32_t mask, uint32_t value);	
static int set_charDriver (void);
static int clr_charDriver (void);
static int NMopen(struct inode *inode, struct file *file);
static int NMrelease(struct inode *inode, struct file *file);
irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs);

static struct {
	dev_t myi2c;

	struct cdev * myi2c_cdev;
	struct device * myi2c_device;
	struct class * myi2c_class;
} state;

struct file_operations i2c_ops = {
	.owner = THIS_MODULE,
	.open = NMopen,
	//.read = NMread,
	//.write = NMwrite,
	.release = NMrelease,
	//.ioctl = NMioctl,
};

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nicolas Mahnic R5054");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("TD3_MYI2C LKM");