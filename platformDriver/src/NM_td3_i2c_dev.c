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


#include "../inc/BBB_I2C_reg.h"
#include "../inc/BMP280_reg.h"
#include "../inc/NM_td3_i2c_dev.h"

static void __iomem  *i2c_baseAddr, *cmper_baseAddr, *ctlmod_baseAddr;
volatile int virq;

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
/*
struct file_operations i2c_ops;
 = {
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

	//int status = 0;
	i2c_baseAddr = of_iomap(i2c_pd->dev.of_node,0);
	pr_info("%s: i2c_baseAddr: 0x%p\n", DEVICE_NAME, i2c_baseAddr);

	if((cmper_baseAddr = ioremap(CM_PER, CM_PER_LEN)) == NULL)	{
		pr_alert("%s: No pudo mapear CM_PER\n", DEVICE_NAME);
		iounmap(i2c_baseAddr);
		return 1;
	}

	// ----Habilito el clock del I2C----
	set_registers(cmper_baseAddr, 
				  CM_PER_I2C2_CLKCTRL_OFFSET,
				  CM_PER_I2C2_CLKCTRL_MASK,
				  CM_PER_I2C2_CLKCTRL_VALUE);

	
    msleep(10); // Wait until the clock is ready.

	check_registers(cmper_baseAddr, 
				  CM_PER_I2C2_CLKCTRL_OFFSET,
				  CM_PER_I2C2_CLKCTRL_MASK,
				  CM_PER_I2C2_CLKCTRL_VALUE);

	// ----Mapeo el registro CONTROL MODULE----
	if((ctlmod_baseAddr = ioremap(CTRL_MODULE_BASE, CTRL_MODULE_LEN)) == NULL) {
		pr_alert("%s: No pudo mapear CONTROL MODULE\n", DEVICE_NAME);
		iounmap(i2c_baseAddr);
		iounmap(cmper_baseAddr);
		return 1;
	}

	// ----Habilito el capacidades de pines de SDA y SCL-----
	set_registers(ctlmod_baseAddr, 
				  CTRL_MODULE_UART1_CTSN_OFFSET,
				  CTRL_MODULE_UART1_MASK,
				  CTRL_MODULE_UART1_I2C_VALUE);

	set_registers(ctlmod_baseAddr, 
				  CTRL_MODULE_UART1_RTSN_OFFSET,
				  CTRL_MODULE_UART1_MASK,
				  CTRL_MODULE_UART1_I2C_VALUE);

	// ----Seteo de Registros de los pines I2C----
	// Disable I2C2.
    iowrite32(0x0000, i2c_baseAddr + I2C_CON);
    // Prescaler configured at 24Mhz
    iowrite32(0x01, i2c_baseAddr + I2C_PSC);
    // Configure SCL to 1MHz
    iowrite32(0x35, i2c_baseAddr + I2C_SCLL); // tLOW = (SCLL + 7) * ICLK
    iowrite32(0x37, i2c_baseAddr + I2C_SCLH); // tHIGH = (SCLH + 5) * ICLK
    // Random Own Address
    iowrite32(0x77, i2c_baseAddr + I2C_OA);
    // I2C_SYSC has 0h value on reset, don't need to be configured.
    // Slave Address
    iowrite32(BMP280_ADDRESS_ALT, i2c_baseAddr + I2C_SA);
    // configure register -> ENABLE & MASTER & RX & STOP
    // iowrite32(0x8400, i2c_baseAddr + I2C_CON);
    iowrite32(0x8600, i2c_baseAddr + I2C_CON);


	// ----Configuracion de VirtualIRQ----
	// Pido un número de interrupcion
	virq = platform_get_irq(i2c_pd, 0);
	if(virq < 0) {
		pr_alert("%s: No se le pudo asignar una VIRQ\n", DEVICE_NAME);
		iounmap(i2c_baseAddr);
		iounmap(cmper_baseAddr);
		iounmap(ctlmod_baseAddr);
		return 1;
	}

	// Lo asigno a mi handler
	if(request_irq(virq, (irq_handler_t) i2c_irq_handler, IRQF_TRIGGER_RISING, COMPATIBLE, NULL)) {
		pr_alert("%s: No se le pudo bindear VIRQ con handler\n", DEVICE_NAME);
		iounmap(i2c_baseAddr);
		iounmap(cmper_baseAddr);
		iounmap(ctlmod_baseAddr);
		return 1;
	}
	pr_info("%s: Numero de IRQ %d\n", DEVICE_NAME, virq);

	
	return 0;
}

void set_registers (void __iomem *base, uint32_t offset, uint32_t mask, uint32_t value) {
	uint32_t old_value = ioread32 (base + offset);
  	old_value &= ~(mask);
  	value &= mask;
  	value |= old_value;
	pr_info("%s: dirAddr: 0x%X value:0x%X\n", DEVICE_NAME, base+offset, value);
  	iowrite32 (value, base + offset);
}

void check_registers (void __iomem *base, uint32_t offset, uint32_t mask, uint32_t value) {	
	int auxValue = ioread32(base + offset);
    if((auxValue & mask) != value){
        pr_info("%s: checker isn't OK\n", DEVICE_NAME); 
        iounmap(i2c_baseAddr);
        iounmap(cmper_baseAddr);
    }else{
		pr_info("%s: dirAddr: 0x%X, checker is OK\n", DEVICE_NAME, base+offset);
	}
}

irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs) {
  pr_info("%s: Handler de VIRQ\n", DEVICE_NAME); 

  return IRQ_HANDLED;
}

static int i2c_remove(struct platform_device * i2c_pd) {
	pr_alert("%s: Sali del REMOVE\n", DEVICE_NAME);
	iounmap(i2c_baseAddr);
	iounmap(cmper_baseAddr);
	iounmap(ctlmod_baseAddr);

	free_irq(virq, NULL);

	return 0;
}

module_init(i2c_init);
module_exit(i2c_exit);