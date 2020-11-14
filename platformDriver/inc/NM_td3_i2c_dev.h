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
irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs);

static struct {
	dev_t myi2c;

	struct cdev * myi2c_cdev;
	struct device * myi2c_device;
	struct class * myi2c_class;
} state;