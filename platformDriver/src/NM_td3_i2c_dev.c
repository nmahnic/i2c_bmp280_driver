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

static DECLARE_WAIT_QUEUE_HEAD (queue);
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

	// Creo clase de dispositivo
	if((state.myi2c_class = class_create(THIS_MODULE,CLASS_NAME)) == NULL){
		pr_alert("%s: Cannot create the struct class for device\n", DEVICE_NAME);
		cdev_del(state.myi2c_cdev);
		unregister_chrdev_region(state.myi2c, CANT_DISP);
		return EFAULT;
	}
	
	// change permission /dev/NM_td3_i2c_dev
	state.myi2c_class->dev_uevent = change_permission_cdev;

	// Creo device
	if((device_create(state.myi2c_class, NULL, state.myi2c, NULL, DEVICE_NAME)) == NULL){
		pr_alert("%s: Cannot create the Device\n", DEVICE_NAME);
		cdev_del(state.myi2c_cdev);
		unregister_chrdev_region(state.myi2c, CANT_DISP);
		device_destroy(state.myi2c_class, state.myi2c);
		return EFAULT;
	}
	pr_alert("%s: Inicializacion del modulo terminada exitosamente...\n", DEVICE_NAME);

	if((status = platform_driver_register(&i2c_pd)) <0){
		pr_alert("%s: No se pudo registrar el platformDevice\n", DEVICE_NAME);
		cdev_del(state.myi2c_cdev);
		unregister_chrdev_region(state.myi2c, CANT_DISP);
		device_destroy(state.myi2c_class, state.myi2c);
		class_destroy(state.myi2c_class);
		return status;
	}

	return 0;
}

static void __exit i2c_exit(void){
	printk(KERN_ALERT "%s: Cerrando el CharDriver\n", DEVICE_NAME);
	// cdev_del - remove a cdev from the system - https://manned.org/cdev_del.9
	cdev_del(state.myi2c_cdev);
	// unregister_chrdev_region - unregister a range of device numbers - https://manned.org/unregister_chrdev_region.9
	unregister_chrdev_region(state.myi2c, CANT_DISP);
	//
	device_destroy(state.myi2c_class, state.myi2c);
	//
    class_destroy(state.myi2c_class);
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
    msleep(10); // Espero a que este listo el clock

	pr_info("%s: Termina PROBE ..\n", DEVICE_NAME);
	return status;
}

static int i2c_remove(struct platform_device * i2c_pd) {
	int status = 0;
	
	pr_alert("%s: Entre al REMOVE\n", DEVICE_NAME);
	iounmap(dev_i2c_baseAddr);
	iounmap(cmper_baseAddr);
	iounmap(ctlmod_baseAddr);
	iounmap(i2c2_baseAddr);

	free_irq(virq, NULL);

	pr_alert("%s: Salí del REMOVE\n", DEVICE_NAME);
	return status;
}

static int NMopen(struct inode *inode, struct file *file) {
	char config[2] = {0};
	char reg[1] = {0};
    char data[24] = {0};
	char dato[8] = {0};
	int i = 0;

	pr_alert("%s: OPEN file operation\n", DEVICE_NAME);

	// ----Seteo de Registros de los pines I2C----
	iowrite32(0x00, i2c2_baseAddr + I2C_CON);
	iowrite32(0x03, i2c2_baseAddr + I2C_PSC);
	iowrite32(I2C_SCLL_100K, i2c2_baseAddr + I2C_SCLL);
	iowrite32(I2C_SCLH_100K, i2c2_baseAddr + I2C_SCLH);
	iowrite32(0x36, i2c2_baseAddr + I2C_OA);
	iowrite32(0x00, i2c2_baseAddr + I2C_IRQSTATUS_XRDY);
	iowrite32(0x76, i2c2_baseAddr + I2C_SA);
	iowrite32(0x8000, i2c2_baseAddr + I2C_CON);

	// ----Habilito el capacidades de pines de SDA y SCL-----
	iowrite32(0x23, i2c2_baseAddr + CTRL_MODULE_UART1_CTSN);
	iowrite32(0x23, i2c2_baseAddr + CTRL_MODULE_UART1_RTSN);

	// pido una pagina para usar como espacio lectura (vector)
	if ((data_i2c.buff_rx = (char *) __get_free_page(GFP_KERNEL)) < 0){
		pr_alert("%s: Falla al pedir memoria\n", DEVICE_NAME);
		return -1;
	}

	//config[0] = BMP280_REGISTER_SOFTRESET;      //Le escribo al registro de reset
	//config[1] = MODE_SOFT_RESET_CODE;  
	//writeBMP280 (config, 2);

	reg[0] = BMP280_REGISTER_CHIPID;
	writeBMP280 (reg, 1);
	pr_alert("%s: chip_ID: 0x%X == 0x58\n", DEVICE_NAME, readByteBMP280());

	msleep(5000);
	
	reg[0] = BMP280_REGISTER_SOFTRESET;
	writeBMP280 (reg, 1);
	pr_alert("%s: SoftReset: 0x%X == 0x0\n", DEVICE_NAME, readByteBMP280());

	msleep(5000);

	reg[0] = BMP280_REGISTER_CHIPID;
	writeBMP280 (reg, 1);
	pr_alert("%s: chip_ID: 0x%X == 0x58\n", DEVICE_NAME, readByteBMP280());


	/*
	reg[0] = BMP280_REGISTER_DIG_T1;
	writeBMP280 (reg, 1);

	for(i = 0 ; i < 24; i++){
		data[i] = readByteBMP280();
		//pr_alert("%s: Dato[%d] = 0x%X\n", DEVICE_NAME,i, data[i]);
	}

	sensor.dig_T1 = data[1] * 256 + data[0];
	sensor.dig_T2 = data[3] * 256 + data[2];
	if (sensor.dig_T2 > 0x7FFF){ sensor.dig_T2 -= 0x10000; }
	sensor.dig_T3 = data[5] * 256 + data[4];
	if (sensor.dig_T3 > 0x7FFF){ sensor.dig_T3 -= 0x10000; }
	
	sensor.dig_P1 = data[7] * 256 + data[6];
	sensor.dig_P2 = data[9] * 256 + data[8];
	if (sensor.dig_P2 > 0x7FFF){ sensor.dig_P2 -= 0x10000; }
	sensor.dig_P3 = data[11] * 256 + data[10];
	if (sensor.dig_P3 > 0x7FFF){ sensor.dig_P3 -= 0x10000; }
	sensor.dig_P4 = data[13] * 256 + data[12];
	if (sensor.dig_P4 > 0x7FFF) { sensor.dig_P4 -= 0x10000; }
	sensor.dig_P5 = data[15] * 256 + data[14];
	if (sensor.dig_P5 > 0x7FFF) { sensor.dig_P5 -= 0x10000; }
	sensor.dig_P6 = data[17] * 256 + data[16];
	if (sensor.dig_P6 > 0x7FFF) { sensor.dig_P6 -= 0x10000; }
	sensor.dig_P7 = data[19] * 256 + data[18];
	if (sensor.dig_P7 > 0x7FFF) { sensor.dig_P7 -= 0x10000; }
	sensor.dig_P8 = data[21] * 256 + data[20];
	if (sensor.dig_P8 > 0x7FFF) { sensor.dig_P8 -= 0x10000; }
	sensor.dig_P9 = data[23] * 256 + data[22];
	if (sensor.dig_P9 > 0x7FFF) { sensor.dig_P9 -= 0x10000; }
	*/

	/*
	config[0] = BMP280_REGISTER_CONTROL;
	config[1] = WEARTHER_MONITOR;
	writeBMP280 (config, 2);

	config[0] = BMP280_REGISTER_CONFIG;
	config[1] = STANDBY_MS_1000;
	writeBMP280 (config, 2);

	reg[0] = BMP280_REGISTER_PRESSUREDATA;
	writeBMP280 (reg, 1);

	for(i = 0 ; i < 8; i++){
		dato[i] = readByteBMP280();
		//pr_alert("%s: Dato[%d] = 0x%X\n", DEVICE_NAME,i, dato[i]);
	}
	*/
	/*
	sensor.adc_p = (((long)dato[0] * 0x10000) + ((long)dato[1] * 256) + (long)(dato[2] & 0xF0)) / 16;
  	sensor.adc_t = (((long)dato[3] * 0x10000) + ((long)dato[4] * 256) + (long)(dato[5] & 0xF0)) / 16;

	pr_alert("%s: TEMPERATURA = %.2f\n", DEVICE_NAME, sensor.adc_p);
	pr_alert("%s: PRESION = %.2f\n", DEVICE_NAME, sensor.adc_t);

	
	sensor.var1 = (((float)sensor.adc_t) / 16384.0 - ((float)sensor.dig_T1) / 1024.0) * ((float)sensor.dig_T2);
	sensor.var2 = ((((float)sensor.adc_t) / 131072.0 - ((float)sensor.dig_T1) / 8192.0) * (((float)sensor.adc_t) / 131072.0 - ((float)sensor.dig_T1) / 8192.0)) * ((float)sensor.dig_T3);
	sensor.t_fine = (long)(sensor.var1 + sensor.var2);
	sensor.cTemp = (sensor.var1 + sensor.var2) / 5120.0;
	// Pressure offset calculations
	sensor.var1 = ((float)sensor.t_fine / 2.0) - 64000.0;
	sensor.var2 = sensor.var1 * sensor.var1 * ((float)sensor.dig_P6) / 32768.0;
	sensor.var2 = sensor.var2 + sensor.var1 * ((float)sensor.dig_P5) * 2.0;
	sensor.var2 = (sensor.var2 / 4.0) + (((float)sensor.dig_P4) * 0x10000);
	sensor.var1 = (((float)sensor.dig_P3) * sensor.var1 * sensor.var1 / 524288.0 + ((float)sensor.dig_P2) * sensor.var1) / 524288.0;
	sensor.var1 = (1.0 + sensor.var1 / 32768.0) * ((float)sensor.dig_P1);
	sensor.p = 1048576.0 - (float)sensor.adc_p;
	sensor.p = (sensor.p - (sensor.var2 / 4096.0)) * 6250.0 / sensor.var1;
	sensor.var1 = ((float)sensor.dig_P9) * sensor.p * sensor.p / 2147483648.0;
	sensor.var2 = sensor.p * ((float)sensor.dig_P8) / 32768.0;
	sensor.pressure = (sensor.p + (sensor.var1 + sensor.var2 + ((float)sensor.dig_P7)) / 16.0) / 100;

	pr_alert("%s: TEMPERATURA = %.2f\n", DEVICE_NAME, sensor.cTemp);
	pr_alert("%s: PRESION = %.2f\n", DEVICE_NAME, sensor.pressure);
	*/
	return 0;
}

static int NMrelease(struct inode *inode, struct file *file) {
	pr_alert("%s: REALEASE file operation\n", DEVICE_NAME);
	// libero la pagina que tome para lectura
	free_page((unsigned long)data_i2c.buff_rx);
    return 0;
}

static ssize_t NMread (struct file * device_descriptor, char __user * user_buffer, size_t read_len, loff_t * my_loff_t) {
	int status = 0;
	char config[2] = {0};
	char reg[1] = {0};
	int i = 0;

	pr_alert("%s: READ file operation\n", DEVICE_NAME);

	if(access_ok(VERIFY_WRITE, user_buffer, read_len) == 0){
		pr_alert("%s: Falla buffer de usuario\n", DEVICE_NAME);
		return -1;
	}

	if(read_len > PAGE_SIZE){
		pr_alert("%s: El kernel reserva una pagina\n", DEVICE_NAME);
		return -1;
	}

	if(read_len > 48){
		read_len = 48;
	}

	reg[0] = BMP280_REGISTER_DIG_T1;
	writeBMP280 (reg, 1);

	readBMP280(0,24);
	/*
	for(i = 0 ; i < 24; i++){
		data_i2c.buff_rx[i] = readByteBMP280();
		//pr_alert("%s: Dato[%d] = 0x%X\n", DEVICE_NAME,i, data[i]);
	}
	*/

	msleep(10); // Espero a que este listo el clock
	
	config[0] = BMP280_REGISTER_SOFTRESET;      //Le escribo al registro de reset
	config[1] = MODE_SOFT_RESET_CODE;  
	writeBMP280 (config, 2);

	msleep(10);

	reg[0] = BMP280_REGISTER_DIG_T1;
	writeBMP280 (reg, 1);

	readBMP280(24,48);

/*
	config[0] = 0xF4;//BMP280_REGISTER_CONTROL;
	config[1] = 0x27;//WEARTHER_MONITOR;
	writeBMP280 (config, 2);

	msleep(10); // Espero a que este listo el clock

	config[0] = 0xF5;//BMP280_REGISTER_CONFIG;
	config[1] = 0xA0;//STANDBY_MS_1000;
	writeBMP280 (config, 2);

	msleep(10); // Espero a que este listo el clock

	reg[0] = 0xF7;//BMP280_REGISTER_PRESSUREDATA;
	writeBMP280 (reg, 1);

	readBMP280(24,32);
*/
	/*
	for( ; i < 32; i++){
		data_i2c.buff_rx[i] = readByteBMP280();
		//pr_alert("%s: Dato[%d] = 0x%X\n", DEVICE_NAME,i, dato[i]);
	}
	*/

	if((status = copy_to_user(user_buffer, data_i2c.buff_rx, read_len))>0){			//en copia correcta devuelve 0
		pr_info("%s: Falla en copia de buffer de kernel a buffer de usuario\n", DEVICE_NAME);
		return -1;
	}
	
	return 0;	
}

static ssize_t NMwrite (struct file * device_descriptor, const char __user * user_buffer, size_t write_len, loff_t * my_loff_t){
	
	pr_alert("%s: WRITE file operation\n", DEVICE_NAME);

	if(access_ok(VERIFY_WRITE, user_buffer, write_len) == 0){
		pr_alert("%s: Falla buffer de usuario\n", DEVICE_NAME);
		return -1;
	}

	if(write_len > 2){
		write_len = 2;
	}

	if((data_i2c.buff_tx = (char *) kmalloc (write_len, GFP_KERNEL)) == NULL){
		pr_info("%s: Falla reserva de memoria para buffer de lectura\n", DEVICE_NAME);
		return -1;
	}

	writeBMP280(data_i2c.buff_tx, sizeof(data_i2c.buff_tx));

	kfree(data_i2c.buff_tx);

	pr_info("%s: Finaliza escritura\n", DEVICE_NAME);
	return write_len;
}

void writeBMP280 (char *writeData, int writeData_size){
    uint32_t aux_regValue = 0;

    pr_info("%s: Inicia escritura\n", DEVICE_NAME);

    // Check irq status (occupied or free)
    while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio

    // Cargo las condiciones iniciales en el objeto que el handler actualiza
	data_i2c.buff_tx = writeData;
	data_i2c.buff_tx_len = writeData_size;
	data_i2c.pos_tx = 0;
	wakeup_tx = 0;

    // Cargo la cantidad de bytes a enviar
    iowrite32(writeData_size, i2c2_baseAddr + I2C_CNT);

    // Setea en modo transmision
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue |= 0x600;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    // Habilita interrupcion de Tx
    iowrite32(I2C_IRQSTATUS_XRDY, i2c2_baseAddr + I2C_IRQENABLE_SET);

    // Condicion de start
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue |= I2C_CON_START;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    // Pone en esta interrupible al proceso que escribe hasta que termine de hacerse todas las escrituras
	wait_event_interruptible (queue, wakeup_tx > 0);

    wakeup_tx = 0;

    // se limpia el bit de start y se levanta el bit de stop
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue &= 0xFFFFFFFE;
    aux_regValue |= I2C_CON_STOP;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    msleep(1);

    pr_info("%s: Finaliza escritura\n", DEVICE_NAME);
}

uint8_t readByteBMP280(void){
    uint32_t aux_regValue = 0;

    pr_info("%s: Inicia lectura\n", DEVICE_NAME);

    // Espero que el bus se libere
    while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio

	// Cargo las condiciones iniciales en el objeto que el handler actualiza
	data_i2c.buff_rx_len = 1;
	data_i2c.pos_rx = 0;
	wakeup_rx = 0;

    // Largo de lo que ba a leer.
    iowrite32(data_i2c.buff_rx_len, i2c2_baseAddr + I2C_CNT);

    // Setea en modo recepcion
    aux_regValue = 0x8400;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    //habilitacion de interrupcion de rx
    iowrite32(I2C_IRQSTATUS_RRDY, i2c2_baseAddr + I2C_IRQENABLE_SET);

    //se activa bit de start
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue &= 0xFFFFFFFC;
    aux_regValue |= I2C_CON_START;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    //tarea en TASK_INTERRUPTIBLE hasta cumplir condicion
    wait_event_interruptible (queue, wakeup_rx > 0);  //tarea en TASK_INTERRUPTIBLE hasta cumplir condicion

    wakeup_rx = 0;
	data_i2c.pos_rx = 0;

    //se envia condicion de stop
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue &= 0xFFFFFFFE;
    aux_regValue |= I2C_CON_STOP;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    return data_i2c.buff_rx[0];
}

uint8_t readBMP280(int inicio,int len){
    uint32_t aux_regValue = 0;

    pr_info("%s: Inicia lectura\n", DEVICE_NAME);

    // Espero que el bus se libere
    while (ioread32(i2c2_baseAddr + I2C_IRQSTATUS_RAW) & 0x1000){msleep(1);} //espero que el bus este vacio

	// Cargo las condiciones iniciales en el objeto que el handler actualiza
	data_i2c.buff_rx_len = len;
	data_i2c.pos_rx = inicio;
	wakeup_rx = 0;

    // Largo de lo que ba a leer.
    iowrite32(data_i2c.buff_rx_len, i2c2_baseAddr + I2C_CNT);

    // Setea en modo recepcion
    aux_regValue = 0x8400;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    //habilitacion de interrupcion de rx
    iowrite32(I2C_IRQSTATUS_RRDY, i2c2_baseAddr + I2C_IRQENABLE_SET);

    //se activa bit de start
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue &= 0xFFFFFFFC;
    aux_regValue |= I2C_CON_START;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    //tarea en TASK_INTERRUPTIBLE hasta cumplir condicion
    wait_event_interruptible (queue, wakeup_rx > 0);  //tarea en TASK_INTERRUPTIBLE hasta cumplir condicion

    wakeup_rx = 0;

    //se envia condicion de stop
    aux_regValue = ioread32(i2c2_baseAddr + I2C_CON);
    aux_regValue &= 0xFFFFFFFE;
    aux_regValue |= I2C_CON_STOP;
    iowrite32(aux_regValue, i2c2_baseAddr + I2C_CON);

    return data_i2c.buff_rx;
}

irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs) {
	char aux = 0;
	int irq_i2c_status = ioread32(i2c2_baseAddr + I2C_IRQSTATUS);
	iowrite32(irq_i2c_status, i2c2_baseAddr + I2C_IRQSTATUS);
	//pr_info("%s: Handler de VIRQ\n", DEVICE_NAME);

	if(irq_i2c_status & I2C_IRQSTATUS_RRDY){
		//pr_info("%s: Handler RX\n", DEVICE_NAME);
		data_i2c.buff_rx[data_i2c.pos_rx] = ioread32(i2c2_baseAddr + I2C_DATA);
		aux = data_i2c.buff_rx[data_i2c.pos_rx];
		pr_info("%s: Dato leido 0x%X\n", DEVICE_NAME,(unsigned int)aux);
		data_i2c.pos_rx++;
		pr_info("%s: LEN 0x%X  POS0x%X\n", DEVICE_NAME,data_i2c.buff_rx_len,data_i2c.pos_rx);
		if(data_i2c.buff_rx_len == data_i2c.pos_rx){ // si termino de leer todos los bytes despierta al proceso
			iowrite32(0x08, i2c2_baseAddr + I2C_IRQENABLE_CLR);
			wakeup_rx = 1;
			wake_up_interruptible(&queue);	
		}
    }
   
    if(irq_i2c_status & I2C_IRQSTATUS_XRDY){
		//pr_info("%s: Handler TX\n", DEVICE_NAME);
		pr_info("%s: Dato escrito 0x%X\n", DEVICE_NAME,data_i2c.buff_tx[data_i2c.pos_tx]);
		iowrite32(data_i2c.buff_tx[data_i2c.pos_tx], i2c2_baseAddr + I2C_DATA);
		data_i2c.pos_tx++;
		
		if(data_i2c.buff_tx_len == data_i2c.pos_tx){ // si escribio todos los bytes que debia despierta el proceso
			iowrite32(0x10, i2c2_baseAddr + I2C_IRQENABLE_CLR);
			wakeup_tx = 1;
			wake_up_interruptible(&queue);	
		}
	}

	return IRQ_HANDLED;
}

static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

module_init(i2c_init);
module_exit(i2c_exit);