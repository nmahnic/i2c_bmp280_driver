#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h> 			//agregar en /dev
#include <linux/device.h> 			//agregar en /dev
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
#include <linux/slab.h>				// kmalloc


#include "../inc/BBB_I2C_reg.h"
#include "../inc/BMP280_reg.h"
#include "../inc/NM_td3_i2c_dev.h"

static DECLARE_WAIT_QUEUE_HEAD (queue_rx);
static DECLARE_WAIT_QUEUE_HEAD (queue_tx);
static unsigned int wakeup_rx=0;
static unsigned int wakeup_tx=0;
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

	// se usa para levantar las direcciones del devTree (no la termino usando)
	dev_i2c_baseAddr = of_iomap(i2c_pd->dev.of_node,0); 
	pr_info("%s: dev_i2c_baseAddr: 0x%X\n", DEVICE_NAME, (unsigned int)dev_i2c_baseAddr);

	// ----Mapeo el registro CM_PER----
	if((cmper_baseAddr = ioremap(CM_PER, CM_PER_LEN)) == NULL)	{
		pr_alert("%s: No pudo mapear CM_PER\n", DEVICE_NAME);
		iounmap(dev_i2c_baseAddr);
		return 1;
	}
	pr_info("%s: cmper_baseAddr: 0x%X\n", DEVICE_NAME, (unsigned int)cmper_baseAddr);

	// ----Mapeo el registro CONTROL MODULE----
	if((ctlmod_baseAddr = ioremap(CTRL_MODULE_BASE, CTRL_MODULE_LEN)) == NULL) {
		pr_alert("%s: No pudo mapear CONTROL MODULE\n", DEVICE_NAME);
		iounmap(dev_i2c_baseAddr);
		iounmap(cmper_baseAddr);
		return 1;
	}
	pr_info("%s: ctlmod_baseAddr: 0x%X\n", DEVICE_NAME, (unsigned int)ctlmod_baseAddr);

	// ----Mapeo el registro I2C2----
	if((i2c2_baseAddr = ioremap(I2C2, I2C2_LEN)) == NULL) {
		pr_alert("%s: No pudo mapear I2C\n", DEVICE_NAME);
		iounmap(dev_i2c_baseAddr);
		iounmap(cmper_baseAddr);
		iounmap(ctlmod_baseAddr);
		return 1;
	}
	pr_info("%s: i2c2_baseAddr: 0x%X\n", DEVICE_NAME, (unsigned int)i2c2_baseAddr);

	// ----Habilito el clock del I2C----
	set_registers(cmper_baseAddr, 
				  CM_PER_I2C2_CLKCTRL_OFFSET,
				  CM_PER_I2C2_CLKCTRL_MASK,
				  CM_PER_I2C2_CLKCTRL_ENABLE);

	
    msleep(10); // Wait until the clock is ready.

	// ----Habilito el capacidades de pines de SDA y SCL-----
	set_registers(ctlmod_baseAddr, 
				  CTRL_MODULE_UART1_CTSN,
				  CTRL_MODULE_UART1_MASK,
				  CTRL_MODULE_I2C_PINMODE);

	set_registers(ctlmod_baseAddr, 
				  CTRL_MODULE_UART1_RTSN,
				  CTRL_MODULE_UART1_MASK,
				  CTRL_MODULE_I2C_PINMODE);

	// ----Seteo de Registros de los pines I2C----
	set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_DISABLE); // Disable I2C2.
    set_registers(i2c2_baseAddr, I2C_PSC, I2C_PSC_MASK, I2C_PSC_VALUE); // Prescaler divided by 4
    set_registers(i2c2_baseAddr, I2C_SCLL, I2C_SCLL_MASK, I2C_SCLL_100K);
	set_registers(i2c2_baseAddr, I2C_SCLH, I2C_SCLH_MASK, I2C_SCLH_100K);
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

	if ((data_i2c.buff_rx = (char *) __get_free_page(GFP_KERNEL)) < 0){
		pr_alert("%s: Falla al pedir memoria\n", DEVICE_NAME);
		return -1;
	}

	return status;
}

void set_registers (void __iomem *base, uint32_t offset, uint32_t mask, uint32_t value) {
	uint32_t old_value = ioread32 (base + offset);
  	old_value &= ~(mask);
  	value &= mask;
  	value |= old_value;
	pr_info("%s: dirAddr: 0x%X value:0x%X\n", DEVICE_NAME, (unsigned int)base+offset, value);
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
		pr_info("%s: dirAddr: 0x%X, checker is OK\n", DEVICE_NAME, (unsigned int)base+offset);
	}
}

irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs) {
	char aux = 0;
	int irq_i2c_status = ioread32(i2c2_baseAddr + I2C_IRQSTATUS);
	pr_info("%s: Handler de VIRQ\n", DEVICE_NAME);

	if(irq_i2c_status & I2C_IRQSTATUS_RRDY){
		pr_info("%s: Handler RX\n", DEVICE_NAME);
		data_i2c.buff_rx[data_i2c.pos_rx] = ioread32(i2c2_baseAddr + I2C_DATA);
		aux = data_i2c.buff_rx[data_i2c.pos_rx];
		pr_info("%s: Dato leido %d\n", DEVICE_NAME,(unsigned int)aux);
		data_i2c.pos_rx++;
		
		if(data_i2c.buff_rx_len == data_i2c.pos_rx){ // si termino de leer todos los bytes despierta al proceso
			wakeup_rx = 1;
			wake_up_interruptible(&queue_rx);	
		}
    }
   
    if(irq_i2c_status & I2C_IRQSTATUS_XRDY){
		pr_info("%s: Handler TX\n", DEVICE_NAME);
		iowrite32(data_i2c.buff_tx[data_i2c.pos_tx], i2c2_baseAddr + I2C_DATA);
		data_i2c.pos_tx++;
		
		if(data_i2c.buff_tx_len == data_i2c.pos_tx){ // si escribio todos los bytes que debia despierta el proceso
			wakeup_tx = 1;
			wake_up_interruptible(&queue_tx);	
		}
	}

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
	
	// change permission /dev/NM_td3_i2c_dev
	state.myi2c_class->dev_uevent = change_permission_cdev;

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

	return 0;
}

static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int NMopen(struct inode *inode, struct file *file) {
	pr_alert("%s: OPEN file operation HOLI\n", DEVICE_NAME);
	return 0;
}

static int NMrelease(struct inode *inode, struct file *file) {
	pr_alert("%s: REALEASE file operation HOLI\n", DEVICE_NAME);
    return 0;
}

static ssize_t NMread (struct file * device_descriptor, char __user * user_buffer, size_t read_len, loff_t * my_loff_t) {

	unsigned long result;
	pr_info("%s: Inicia lectura\n", DEVICE_NAME);

	if(read_len > 24){
		read_len = 24;
	}

	while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio

	data_i2c.buff_rx_len = read_len;
	data_i2c.pos_rx = 0;
	wakeup_rx = 0;

	set_registers(i2c2_baseAddr, I2C_CNT, I2C_CNT_MASK, read_len);	// cant de elementos a leer			
	set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_EN_MST_RX); // modo recepcion
	set_registers(i2c2_baseAddr, I2C_IRQENABLE_SET, I2C_IRQENABLE_SET_MASK, I2C_IRQENABLE_SET_RX); //habilito interrupciones
	set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_START);
	wait_event_interruptible (queue_rx, wakeup_rx > 0);  //tarea en TASK_INTERRUPTIBLE hasta cumplir condicion
	set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_STOP);

	if((result = copy_to_user(user_buffer, data_i2c.buff_rx, read_len))>0){			//en copia correcta devuelve 0
		pr_info("%s: Falla en copia de buffer de kernel a buffer de usuario\n", DEVICE_NAME);
		return -1;
	}

	if (!result) // Si todo sale bien, devuelve 0. Yo devuelvo la cant. de bytes copiados
		return (ssize_t)read_len;
	return 0;	//	Sino, devuelvo 0
}

static ssize_t NMwrite (struct file * device_descriptor, const char __user * user_buffer, size_t write_len, loff_t * my_loff_t){
	pr_info("%s: Inicia escritura\n", DEVICE_NAME);

	if(write_len > 2){
		write_len = 2;
	}

	if ((data_i2c.buff_tx = (char *) kmalloc (write_len, GFP_KERNEL)) == NULL){
		pr_info("%s: Falla reserva de memoria para buffer de lectura\n", DEVICE_NAME);
		return -1;
	}

	if(copy_from_user(data_i2c.buff_tx, user_buffer ,write_len)>0){				//en copia correcta devuelve 0
		pr_info("%s: Falla al copiar de buffer de usuario a buffer de kernel\n", DEVICE_NAME);
		return -1;
	}

	while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio

	data_i2c.buff_tx_len = write_len;
	data_i2c.pos_tx = 0;
	wakeup_tx = 0;

	set_registers(i2c2_baseAddr, I2C_CNT, I2C_CNT_MASK, write_len);	// cant de elementos a leer			
	set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_EN_MST_TX); // modo recepcion
	set_registers(i2c2_baseAddr, I2C_IRQENABLE_SET, I2C_IRQENABLE_SET_MASK, I2C_IRQENABLE_SET_TX); //habilito interrupciones
	set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_START);
	pr_info("%s: LLEGUE 1\n", DEVICE_NAME);
	wait_event_interruptible (queue_tx, wakeup_tx > 0);  //tarea en TASK_INTERRUPTIBLE hasta cumplir condicion
	pr_info("%s: LLEGUE 2\n", DEVICE_NAME);
	set_registers(i2c2_baseAddr, I2C_CON, I2C_CON_MASK, I2C_CON_STOP);

	kfree(data_i2c.buff_tx);

	pr_info("%s: Finaliza escritura\n", DEVICE_NAME);
	return write_len;
}

module_init(i2c_init);
module_exit(i2c_exit);