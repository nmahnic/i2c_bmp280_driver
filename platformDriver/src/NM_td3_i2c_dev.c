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

static void __iomem  *dev_i2c_baseAddr, *cmper_baseAddr, *ctlmod_baseAddr, *i2c2_baseAddr;
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
	int status = 0;
	
	pr_alert("%s: Entre al PROBE\n", DEVICE_NAME);

	dev_i2c_baseAddr = of_iomap(i2c_pd->dev.of_node,0); // se usa para levantar las direcciones del devTree (no la termino usando)
	pr_info("%s: dev_i2c_baseAddr: 0x%X\n", DEVICE_NAME, dev_i2c_baseAddr);

	if((cmper_baseAddr = ioremap(CM_PER, CM_PER_LEN)) == NULL)	{
		pr_alert("%s: No pudo mapear CM_PER\n", DEVICE_NAME);
		iounmap(dev_i2c_baseAddr);
		return 1;
	}
	pr_info("%s: cmper_baseAddr: 0x%X\n", DEVICE_NAME, cmper_baseAddr);

	// ----Mapeo el registro CONTROL MODULE----
	if((ctlmod_baseAddr = ioremap(CTRL_MODULE_BASE, CTRL_MODULE_LEN)) == NULL) {
		pr_alert("%s: No pudo mapear CONTROL MODULE\n", DEVICE_NAME);
		iounmap(dev_i2c_baseAddr);
		iounmap(cmper_baseAddr);
		return 1;
	}
	pr_info("%s: ctlmod_baseAddr: 0x%X\n", DEVICE_NAME, ctlmod_baseAddr);

	// Mapeo el registro I2C2
	if((i2c2_baseAddr = ioremap(I2C2, I2C2_LEN)) == NULL) {
		pr_alert("%s: No pudo mapear I2C\n", DEVICE_NAME);
		iounmap(dev_i2c_baseAddr);
		iounmap(cmper_baseAddr);
		iounmap(ctlmod_baseAddr);
		return 1;
	}
	pr_info("%s: i2c2_baseAddr: 0x%X\n", DEVICE_NAME, i2c2_baseAddr);

	// ----Habilito el clock del I2C----
	set_registers(cmper_baseAddr, 
				  CM_PER_I2C2_CLKCTRL_OFFSET,
				  CM_PER_I2C2_CLKCTRL_MASK,
				  CM_PER_I2C2_CLKCTRL_ENABLE);

	
    msleep(10); // Wait until the clock is ready.

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
	set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_DISABLE); // Disable I2C2.
    set_registers(i2c2_baseAddr, I2C_PSC, I2C_PSC_MASK, I2C_PSC_VALUE); // Prescaler divided by 4
    set_registers(i2c2_baseAddr, I2C_SCLL, I2C_SCLL_MASK, I2C_SCLL_VALUE); // Configure SCL to 1MHz - tLOW = (SCLL + 7) * ICLK
	set_registers(i2c2_baseAddr, I2C_SCLH, I2C_SCLH_MASK, I2C_SCLH_VALUE); // tHIGH = (SCLH + 5) * ICLK 
    set_registers(i2c2_baseAddr, I2C_OA, I2C_OA_MASK, I2C_OA_VALUE); // Random Own Address - como es unico master no importa la direccion
    set_registers(i2c2_baseAddr, I2C_SA, I2C_SA_MASK, BMP280_ADDRESS); // Slave Address
    set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_EN_MST_TX); // configure register -> ENABLE & MASTER & TX & STOP
	set_registers(i2c2_baseAddr, I2C_IRQENABLE_CLR, I2C_IRQENABLE_CLR_MASK, I2C_IRQENABLE_CLR_ALL); // Disable all I2C interrupts

	// ----Configuracion de VirtualIRQ----
	// Pido un número de interrupcion
	virq = platform_get_irq(i2c_pd, 0);
	if(virq < 0) {
		pr_alert("%s: No se le pudo asignar una VIRQ\n", DEVICE_NAME);
		iounmap(dev_i2c_baseAddr);
		iounmap(cmper_baseAddr);
		iounmap(ctlmod_baseAddr);
		iounmap(i2c2_baseAddr);
		return 1;
	}

	// Lo asigno a mi handler
	if(request_irq(virq, (irq_handler_t) i2c_irq_handler, IRQF_TRIGGER_RISING, COMPATIBLE, NULL)) {
		pr_alert("%s: No se le pudo bindear VIRQ con handler\n", DEVICE_NAME);
		iounmap(dev_i2c_baseAddr);
		iounmap(cmper_baseAddr);
		iounmap(ctlmod_baseAddr);
		iounmap(i2c2_baseAddr);
		return 1;
	}
	pr_info("%s: Numero de IRQ %d\n", DEVICE_NAME, virq);

	status = set_charDriver();

	return status;
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
        iounmap(dev_i2c_baseAddr);
		iounmap(cmper_baseAddr);
		iounmap(ctlmod_baseAddr);
		iounmap(i2c2_baseAddr);
    }else{
		pr_info("%s: dirAddr: 0x%X, checker is OK\n", DEVICE_NAME, base+offset);
	}
}

irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs) {
  pr_info("%s: Handler de VIRQ\n", DEVICE_NAME); 

  return IRQ_HANDLED;
}

static int i2c_remove(struct platform_device * i2c_pd) {
	int status = 0;
	
	pr_alert("%s: Sali del REMOVE\n", DEVICE_NAME);
	iounmap(dev_i2c_baseAddr);
	iounmap(cmper_baseAddr);
	iounmap(ctlmod_baseAddr);
	iounmap(i2c2_baseAddr);

	free_irq(virq, NULL);

	platform_device_unregister(i2c_pd);

	status = clr_charDriver();

	return status;
}

static int set_charDriver (void){
	int status = 0;
	
	pr_alert("%s: Set CharDriver\n", DEVICE_NAME);
	state.myi2c_cdev = cdev_alloc();
	
	// alloc_chrdev_region - register a range of char device numbers - https://manned.org/alloc_chrdev_region.9
	if((status = alloc_chrdev_region(&state.myi2c, MENOR, CANT_DISP, DEVICE_NAME)) < 0){
		pr_alert("%s: No es posible asignar el numero mayor\n", DEVICE_NAME);
		return status;
	}

	pr_alert("%s: Numero mayor asignado %d 0x%X\n", DEVICE_NAME, MAJOR(state.myi2c), MAJOR(state.myi2c));
	
	// cdev_init - initialize a cdev structure - https://manned.org/cdev_init.9
	// Equivalent to:  state.myi2c_cdev->ops = &i2c_ops;
	cdev_init(state.myi2c_cdev, &i2c_ops); 
	
	// cdev_add - add a char device to the system - https://manned.org/cdev_add.9
	// Equivalent to: state.myi2c_cdev->owner = THIS_MODULE;
	// 				  state.myi2c_cdev->dev = state.myi2c;
	if((status = cdev_add(state.myi2c_cdev, state.myi2c, CANT_DISP)) < 0){
		// unregister_chrdev_region - unregister a range of device numbers - https://manned.org/unregister_chrdev_region.9
		unregister_chrdev_region(state.myi2c, CANT_DISP);
		pr_alert("%s: No es no es posible registrar el dispositivo\n", DEVICE_NAME);
		return status;
	}

	/*Creating struct class*/
	if((state.myi2c_class = class_create(THIS_MODULE,CLASS_NAME)) == NULL){
		pr_alert("%s: Cannot create the struct class for device\n", DEVICE_NAME);
		unregister_chrdev_region(state.myi2c, CANT_DISP);
		return EFAULT;
	}

	/*Creating device*/
	if((device_create(state.myi2c_class, NULL, state.myi2c, NULL, DEVICE_NAME)) == NULL){
		pr_alert("%s: Cannot create the Device\n", DEVICE_NAME);
		class_destroy(state.myi2c_class);
    	unregister_chrdev_region(state.myi2c, CANT_DISP);
		return EFAULT;
	}
	pr_alert("%s: Inicializacion del modulo terminada exitosamente...\n", DEVICE_NAME);

	return 0;
}

static int clr_charDriver (void){
	printk(KERN_ALERT "%s: Cerrando el CharDriver\n", DEVICE_NAME);
	// cdev_del - remove a cdev from the system - https://manned.org/cdev_del.9
	cdev_del(state.myi2c_cdev);
	//
	device_destroy(state.myi2c_class, state.myi2c);
	//
    class_destroy(state.myi2c_class);
	// unregister_chrdev_region - unregister a range of device numbers - https://manned.org/unregister_chrdev_region.9
	unregister_chrdev_region(state.myi2c, CANT_DISP);
}

static int NMopen(struct inode *inode, struct file *file) {
	pr_alert("%s: OPEN file operation HOLI\n", DEVICE_NAME);
	return 0;
}

static int NMrelease(struct inode *inode, struct file *file) {
	pr_alert("%s: REALEASE file operation HOLI\n", DEVICE_NAME);
    return 0;
}

module_init(i2c_init);
module_exit(i2c_exit);