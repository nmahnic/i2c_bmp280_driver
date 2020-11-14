#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h> //agregar en /dev
#include <linux/device.h> //agregar en /dev
#include <linux/cdev.h>             // Char device: File operation struct,
#include <linux/fs.h>               // Header for the Linux file system support (alloc_chrdev_region y unregister_chrdev_region)
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/of_address.h>       // of_iomap
#include <linux/platform_device.h>  // platform_device
#include <linux/of.h>               // of_match_ptr
#include <linux/io.h>               // ioremap
#include <linux/interrupt.h>        // request_irq
#include <linux/delay.h>            // msleep, udelay y ndelay
#include <linux/uaccess.h>          // copy_to_user - copy_from_user
#include <linux/types.h>            // typedefs varios


#define MENOR          0
#define CANT_DISP      1
#define DEVICE_NAME    "NM_td3_i2c_dev"
#define CLASS_NAME     "NM_td3_i2c_class"
#define COMPATIBLE	   "NM_td3_i2c_dev"

static int i2c_probe(struct platform_device * i2c_pd);
static int i2c_remove(struct platform_device * i2c_pd);

static struct {
	dev_t myi2c;

	struct cdev * myi2c_cdev;
	struct device * myi2c_device;
	struct class * myi2c_class;
} state;

static struct of_device_id i2c_of_device_ids[] = {
	{
		.compatible = COMPATIBLE,
	}, {}
};

MODULE_DEVICE_TABLE(of, i2c_of_device_ids);

static struct platform_driver i2c_pd = {
	.probe = i2c_probe,
	.remove = i2c_remove,
	.driver = {
		.name = COMPATIBLE,
		.of_match_table = of_match_ptr(i2c_of_device_ids)
	},
};

struct file_operations i2c_ops;
/* = {
	.owner = THIS_MODULE,
	.open = myopen,
	.read = myread,
	.write = mywrite,
	.release = myrelease,
	.ioctl = myioctl,
};*/

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nicolas Mahnic R5054");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("TD3_MYI2C LKM");

static int __init i2c_init(void){

	int status = 0;
	pr_alert("%s: Soy el modulo td3_myi2c del kernel\n", DEVICE_NAME);

	if((status = platform_driver_register(&i2c_pd)) <0){
		pr_alert("%s: No se pudo registrar el platformDevice\n", DEVICE_NAME);
		return status;
	}

	return 0;
}

static void __exit i2c_exit(void){
	printk(KERN_ALERT "%s: Adios mundo!. Soy el kernel\n", DEVICE_NAME);
	platform_driver_unregister(&i2c_pd);
}

static int i2c_probe(struct platform_device * i2c_pd) {
	pr_alert("%s: Entre al PROBE\n", DEVICE_NAME);
	return 0;
}

static int i2c_remove(struct platform_device * i2c_pd) {
	pr_alert("%s: Sali del REMOVE\n", DEVICE_NAME);
	return 0;
}

module_init(i2c_init);
module_exit(i2c_exit);