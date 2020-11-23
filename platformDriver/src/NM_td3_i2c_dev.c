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

	if((status = platform_driver_register(&i2c_pd)) <0){
		pr_alert("%s: No se pudo registrar el platformDevice\n", DEVICE_NAME);
		return status;
	}

	return 0;
}

static void __exit i2c_exit(void){
	printk(KERN_ALERT "%s: Cerrando el CharDriver\n", DEVICE_NAME);
	// cdev_del - remove a cdev from the system - https://manned.org/cdev_del.9
	cdev_del(state.myi2c_cdev);
	//
	device_destroy(state.myi2c_class, state.myi2c);
	//
    class_destroy(state.myi2c_class);
	// unregister_chrdev_region - unregister a range of device numbers - https://manned.org/unregister_chrdev_region.9
	unregister_chrdev_region(state.myi2c, CANT_DISP);
	//
	platform_driver_unregister(&i2c_pd);
}

static int i2c_probe(struct platform_device * i2c_pd) {
	int status = 0;
	
	pr_alert("%s: Entre al PROBE\n", DEVICE_NAME);

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
	iowrite32(0x02, cmper_baseAddr + CM_PER_I2C2_CLKCTRL_OFFSET);

	do{
		msleep(1);
		pr_info("%s: Configurando registro CM_PER..\n", DEVICE_NAME);
	}while(ioread32(cmper_baseAddr + CM_PER_I2C2_CLKCTRL_OFFSET) != 2);
    msleep(10); // Wait until the clock is ready.

	return status;
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

	return status;
}

static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int NMopen(struct inode *inode, struct file *file) {
	char config[2] = {0};

	pr_alert("%s: OPEN file operation HOLI\n", DEVICE_NAME);

	// ----Seteo de Registros de los pines I2C----
	iowrite32(0x00, i2c2_baseAddr + I2C_CON);
	iowrite32(0x03, i2c2_baseAddr + I2C_PSC);
	iowrite32(0x53, i2c2_baseAddr + I2C_SCLL);
	iowrite32(0x55, i2c2_baseAddr + I2C_SCLH);
	iowrite32(0x36, i2c2_baseAddr + I2C_OA);
	iowrite32(0x00, i2c2_baseAddr + I2C_IRQSTATUS_XRDY);
	iowrite32(0x76, i2c2_baseAddr + I2C_SA);
	iowrite32(0x8000, i2c2_baseAddr + I2C_CON);

	// ----Habilito el capacidades de pines de SDA y SCL-----
	iowrite32(0x23, i2c2_baseAddr + CTRL_MODULE_UART1_CTSN);
	iowrite32(0x23, i2c2_baseAddr + CTRL_MODULE_UART1_RTSN);

	if ((data_i2c.buff_rx = (char *) __get_free_page(GFP_KERNEL)) < 0){
		pr_alert("%s: Falla al pedir memoria\n", DEVICE_NAME);
		return -1;
	}

	//config[0] = BMP280_REGISTER_SOFTRESET;      //Le escribo al registro de reset
	//config[1] = MODE_SOFT_RESET_CODE;           //Le escribo el comando RST
	//writeBMP280 (config, sizeof(config));
	return 0;
}

static int NMrelease(struct inode *inode, struct file *file) {
	pr_alert("%s: REALEASE file operation HOLI\n", DEVICE_NAME);
    return 0;
}

static ssize_t NMread (struct file * device_descriptor, char __user * user_buffer, size_t read_len, loff_t * my_loff_t) {
	int status = 0;
	if (access_ok(VERIFY_WRITE, user_buffer, read_len) == 0){
		pr_alert("%s: Falla buffer de usuario\n", DEVICE_NAME);
		return -1;
	}

	if(read_len > 24){
		read_len = 24;
	}

	while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio

	data_i2c.buff_rx[0] = readByteBMP280();

	if((status = copy_to_user(user_buffer, data_i2c.buff_rx, read_len))>0){			//en copia correcta devuelve 0
		pr_info("%s: Falla en copia de buffer de kernel a buffer de usuario\n", DEVICE_NAME);
		return -1;
	}
	
	return 0;	
}

static ssize_t NMwrite (struct file * device_descriptor, const char __user * user_buffer, size_t write_len, loff_t * my_loff_t){
	
	if (access_ok(VERIFY_WRITE, user_buffer, write_len) == 0){
		pr_alert("%s: Falla buffer de usuario\n", DEVICE_NAME);
		return -1;
	}

	if(write_len > 2){
		write_len = 2;
	}

	if ((data_i2c.buff_tx = (char *) kmalloc (write_len, GFP_KERNEL)) == NULL){
		pr_info("%s: Falla reserva de memoria para buffer de lectura\n", DEVICE_NAME);
		return -1;
	}

	while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio

	writeBMP280 (data_i2c.buff_tx, sizeof(data_i2c.buff_tx));

	kfree(data_i2c.buff_tx);

	pr_info("%s: Finaliza escritura\n", DEVICE_NAME);
	return write_len;
}

void writeBMP280 (char *writeData, int writeData_size){
    uint32_t aux_regValue = 0;

    pr_info("%s: Inicia escritura\n", DEVICE_NAME);

    // Check irq status (occupied or free)
    while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio

    // Load data to the variable of the irq.
	data_i2c.buff_tx = writeData;
	data_i2c.buff_tx_len = writeData_size;
	data_i2c.pos_tx = 0;
	wakeup_tx = 0;

    // Set the data length to 1 byte.
    iowrite32(writeData_size, i2c2_baseAddr + I2C_CNT);

    // Set I2C_CON to Master transmitter.
    //  0000 0110 0000 0000 b = 0x600
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue |= 0x600;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    // Transmit data ready IRQ enabled status => Transmit data ready.
    iowrite32(I2C_IRQSTATUS_XRDY, i2c2_baseAddr + I2C_IRQENABLE_SET);

    // Start condition queried.
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue |= I2C_CON_START;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    // Wait transmission end.
	wait_event_interruptible (queue_tx, wakeup_tx > 0);

    wakeup_tx = 0;

    // Stop condition queried. (Must clear I2C_CON_STT).
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue &= 0xFFFFFFFE;
    aux_regValue |= I2C_CON_STOP;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    msleep(1);

    pr_info("%s: Finaliza escritura\n", DEVICE_NAME);
}

uint8_t readByteBMP280(void){
    uint32_t aux_regValue = 0;
    uint8_t readData;

    pr_info("%s: Inicia lectura\n", DEVICE_NAME);

    // Check irq status (occupied or free)
    while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio


	data_i2c.buff_rx_len = 1;
	data_i2c.pos_rx = 0;
	wakeup_rx = 0;
    // Set the data length to 1 byte.
    iowrite32(1, i2c2_baseAddr + I2C_CNT);

    // configure register -> ENABLE & MASTER & RX & STOP
    aux_regValue = 0x8400;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    // Receive data ready IRQ enabled status => Receive data available.
    iowrite32(I2C_IRQSTATUS_RRDY, i2c2_baseAddr + I2C_IRQENABLE_SET);

    // Start condition queried. (Must clear I2C_CON_STP).
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue &= 0xFFFFFFFC;
    aux_regValue |= I2C_CON_START;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    // Wait reception end.
    wait_event_interruptible (queue_rx, wakeup_rx > 0);  //tarea en TASK_INTERRUPTIBLE hasta cumplir condicion

    wakeup_rx = 0;

    // Stop condition queried. (Must clear I2C_CON_STT).
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue &= 0xFFFFFFFE;
    aux_regValue |= I2C_CON_STOP;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    // Retrieve data.
    readData = data_i2c.buff_rx[data_i2c.pos_rx];

    return readData;
}


module_init(i2c_init);
module_exit(i2c_exit);