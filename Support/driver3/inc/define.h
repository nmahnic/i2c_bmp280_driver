#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h> 
#include <linux/device.h> 
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/types.h> 

/*
	Defines para el BUS
*/
#define I2C2_BASE     				0x4819C000
#define I2C_CNT						0x98
#define I2C_CON						0xA4
#define I2C_CON_ENABLE				0x8000
#define I2C_CON_EN_MA_RX			0x8400
#define I2C_CON_EN_MA_RX_STT_STP	0x8403
#define I2C_CON_EN_MA_TX_STT_STP	0x8603 
#define I2C_CON_DISABLE				0x0
#define I2C_DATA					0x9C
#define I2C_IRQENABLE_CLR			0x30
#define I2C_IRQENABLE_CLR_CONF		0x0
#define I2C_IRQENABLE_SET			0x2C
#define I2C_IRQENABLE_SET_CONF		0x0
#define I2C_IRQSTATUS				0x28
#define I2C_IRQSTATUS_RAW			0x24
#define I2C_IRQSTATUS_RRDY			0x08
#define I2C_IRQSTATUS_XRDY			0x10
#define I2C2_LENGTH   				0x1000
#define	I2C_OA						0xA8
#define	I2C_OA_CONF					0x36
#define I2C_PSC						0xB0
#define I2C_PSC_CONF				0x3
#define	I2C_SA						0xAC
#define	I2C_SA_CONF					0x76
#define	I2C_SCLH					0xB8
#define	I2C_SCLH_CONF				0x37
#define I2C_SCLL					0xB4
#define I2C_SCLL_CONF				0x35
#define I2C_SYSC                	0x10

#define CM_PER_BASE             	0x44E00000
#define CM_PER_LENGTH           	0x400
#define CM_PER_I2C2_CLKCTRL			0x44
#define	MODULEMODE					2
#define CONTROL_MODULE_BASE     	0x44E10000
#define CONTROL_MODULE_LENGTH   	0x2000

#define MENOR          				0
#define CANT_DISP      				1
#define DEVICE_NAME    				"NM_td3_i2c_dev"
#define CLASS_NAME     				"td3_i2c_class"
#define COMPATIBLE	   				"NM_td3_i2c_dev"
#define I2C_SLAVE					0x0703
#define IRQ_BORRA_FLAG				0x38



#ifndef DECLARACIONES
    #define DECLARACIONES
/* 
    Declaración de Funciones
*/

static int myopen (struct inode *inode, struct file *file);
static int myclose (struct inode *inode, struct file *file);
static int myread (struct file *filp, char *buffer, size_t len, loff_t *f_pos);
static int mywrite (struct file *filp, const char *buffer, size_t len, loff_t *f_pos);
static long myioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int i2c_probe(struct platform_device * i2c_pd);
static int i2c_remove(struct platform_device * i2c_pd);
irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs);
static int cambio_permisos(struct device *dev, struct kobj_uevent_env *env);

/*
	Variables
*/

struct file_operations i2c_ops = {
	.owner = THIS_MODULE,
	.open = myopen,
    .release = myclose,
    .read = myread,
	.write = mywrite,
	.unlocked_ioctl = myioctl,
};

static struct {
	dev_t myi2c;
	struct cdev * myi2c_cdev;
	struct device * myi2c_device;
	struct class * myi2c_class;
} char_dev;

static volatile void *td3_i2c_base, *cm_per_base, *control_module_base;
volatile int virq;
uint8_t datos_tx[2] = {0}, datos[24] = {0};
uint32_t cant = 0;
int condicion = 0;
wait_queue_head_t wq = __WAIT_QUEUE_HEAD_INITIALIZER(wq), wq_ctrlopen = __WAIT_QUEUE_HEAD_INITIALIZER(wq_ctrlopen);

#endif
