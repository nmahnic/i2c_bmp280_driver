#define FIRST_MINOR 0
#define NBR_DEV 1

#define ADDRESS_CMPER 0x44E00000
#define LENGHT_CMPER 0x400
#define OFFSET_CMPER_I2C2 0x44

#define ADDRESS_I2C2 0x4819C000
#define LENGHT_I2C2 0x1000

#define ADDRESS_CONTROLMODULE 0x44E10000
#define LENGHT_CONTROLMODULE 0x2000

#define OFFSET_CON_I2C2	0xA4
#define OFFSET_PSC_I2C2 0xB0
#define OFFSET_SCLL_I2C2 0xB4
#define OFFSET_SCLH_I2C2 0xB8
#define OFFSET_OA_I2C2 0xA8
#define OFFSET_SYSC_I2C2 0x10
#define OFFSET_SA_I2C2 0xAC
#define OFFSET_DATA_I2C2 0x9C
#define OFFSET_DCONT_I2C2 0x98

#define OFFSET_IRQSTATUS_RAW_I2C2 0x24
#define OFFSET_IRQSTATUS_I2C2 0x28
#define OFFSET_IRQENABLE_SET 0x2C
#define OFFSET_IRQENABLE_CLR 0x30

#define OFFSET_PIN_I2C2_SCL 0x97C				//pin 19 = scl  -0x800
#define OFFSET_PIN_I2C2_SDA 0x978				//pin 20 = sda

//******************************************************************************
//Librerias
//******************************************************************************

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>

#define COMPATIBLE "NM_td3_i2c_dev"

//******************************************************************************
//Prototipos
//******************************************************************************

int i2c_open (struct inode * inode, struct file * file);
ssize_t i2c_write (struct file * file, const char __user * userbuff,size_t tamano, loff_t* offset);
ssize_t i2c_read (struct file * file, char __user * userbuff, size_t tamano, loff_t * offset);

irqreturn_t handler_i2c (int irq, void *dev_id, struct pt_regs *regs);

static int i2c_probe (struct platform_device *);
static int i2c_remove(struct platform_device *);

static void *mmcmper;
static void *mmi2c2;
static void *mmcontmodule;

MODULE_LICENSE("Dual BSD/GPL");

typedef struct struct_datos_i2c2{

	unsigned int posicion_rx;
	unsigned int longitud_buffer_rx;
	unsigned int datos_rx;

	unsigned int posicion_tx;
	unsigned int longitud_buffer_tx;
	unsigned int datos_tx;

	char * buffer_rx;
	char * buffer_tx;
}t_datos_i2c2;

static dev_t mi_dispo;
static struct cdev * p_cdev;
struct class * pclase;
struct device * pdevice_sys;
static int virq;
static unsigned int condicion_wake_rx=0;
static unsigned int condicion_wake_tx=0;
static DECLARE_WAIT_QUEUE_HEAD (mi_queue_rx);
static DECLARE_WAIT_QUEUE_HEAD (mi_queue_tx);
t_datos_i2c2 datos_i2c2;


static struct file_operations i2c_ops = {
	.owner = THIS_MODULE,
	.open = i2c_open,
	.read = i2c_read,
	.write = i2c_write
};

static const struct of_device_id  i2c_of_match[] = {

	{		.compatible= COMPATIBLE},
	{},
};

MODULE_DEVICE_TABLE(of,i2c_of_match);

static struct platform_driver i2c_driver = {

		.probe  = i2c_probe,
		.remove = i2c_remove,
		.driver = {
				.name = COMPATIBLE,
				.of_match_table = of_match_ptr(i2c_of_match),
			},
};

//******************************************************************************
//INIT
//******************************************************************************

static int hello_init(void) {
	int error_check=1;


	error_check = alloc_chrdev_region(&mi_dispo, FIRST_MINOR, NBR_DEV,"i2c_td3");
	if (error_check < 0){//entrega 0 si ok; devuelve número negativo en caso de error
		printk(KERN_ALERT "Falla al asignar numero mayor");
		return -1;
	}
	printk(KERN_ALERT "Numero mayor asignado = %d\n", MAJOR(mi_dispo));


	p_cdev = cdev_alloc();
	if (p_cdev == NULL){//asigna y devuelve estructura cdev; devuelve NULL en caso de error
		printk(KERN_ALERT "Falla al asignar estructura pc_dev");
		unregister_chrdev_region(mi_dispo, NBR_DEV);
		return -1;
	}
	printk(KERN_ALERT "struct cdev allocada\n");
	p_cdev->ops = &i2c_ops;
	p_cdev->owner = THIS_MODULE;
	p_cdev->dev = mi_dispo;


	error_check = cdev_add (p_cdev, mi_dispo, NBR_DEV);
	if (error_check < 0){//Se agrega el dispositivo al sistema; devuelve número negativo en caso de error
		printk(KERN_ALERT "Falla al agregar dispositivo al sistema");
		unregister_chrdev_region(mi_dispo, NBR_DEV);
		return -1;
	}


	pclase = class_create (THIS_MODULE, "td3");
	if (pclase == NULL){//asigna puntero a la estructura de clase; devuelve NULL en caso de error
		printk(KERN_ALERT "Falla al crear estructura de clase");
		cdev_del(p_cdev);
		unregister_chrdev_region(mi_dispo, NBR_DEV);
		return -1;
	}


	pdevice_sys =  device_create (pclase, NULL,mi_dispo , NULL,"i2c_td3");
	if (pdevice_sys == NULL){//asigna puntero a la estructura de dispositivo; devuelve NULL en caso de error
		printk(KERN_ALERT "Falla al crear estructura dispositivo");
		class_destroy(pclase);
		cdev_del(p_cdev);
		unregister_chrdev_region(mi_dispo, NBR_DEV);
		return -1;
	}


	error_check = platform_driver_register(&i2c_driver);
	if (error_check!=0){
		printk(KERN_ALERT "Falla al registrar driver");
		device_destroy (pclase, mi_dispo);
		class_destroy(pclase);
		cdev_del(p_cdev);
		unregister_chrdev_region(mi_dispo, NBR_DEV);
		return -1;
	}

	printk (KERN_ALERT "Modulo asociado\n");

	return 0;
}

//******************************************************************************
//EXIT
//******************************************************************************

static void hello_exit(void) {
	printk(KERN_ALERT "Test HELLO_EXIT");
	platform_driver_unregister(&i2c_driver);
	device_destroy (pclase, mi_dispo);
	class_destroy(pclase);
	cdev_del(p_cdev);
	unregister_chrdev_region(mi_dispo, NBR_DEV);
	printk(KERN_ALERT "Modulo removido\n");
}


//******************************************************************************
//PROBE
//******************************************************************************

static int i2c_probe (struct platform_device *dispo){
	int aux;

	printk(KERN_ALERT "Inicia Probe!");


	virq = platform_get_irq(dispo, 0);						//recibo numero de interrupcion
	if (virq == 0){
		platform_driver_unregister(&i2c_driver);
		device_destroy (pclase, mi_dispo);
		class_destroy(pclase);
		cdev_del(p_cdev);
		unregister_chrdev_region(mi_dispo, NBR_DEV);
		printk(KERN_ALERT "Modulo removido por falla al intentar obtener VIRQ\n");
	}
	printk(KERN_ALERT "VIRQ obtenido: %d\n",virq);


	aux = request_irq(virq, (irq_handler_t) handler_i2c , IRQF_TRIGGER_RISING, dispo->name, NULL);  //Se implanta interrupcion sobre el probe
	if (aux != 0){
		platform_driver_unregister(&i2c_driver);
		device_destroy (pclase, mi_dispo);
		class_destroy(pclase);
		cdev_del(p_cdev);
		unregister_chrdev_region(mi_dispo, NBR_DEV);
		printk(KERN_ALERT "Falla al implantar handler i2c2\n");
	}
	printk(KERN_ALERT "Handler implantado correctamente\n");


	mmi2c2 = ioremap(ADDRESS_I2C2,LENGHT_I2C2);
	if (mmi2c2 == NULL){
		iounmap(mmi2c2);
		printk (KERN_ALERT "Falla al acceder a registros I2C2\n");
	}

	mmcmper = ioremap (ADDRESS_CMPER,LENGHT_CMPER);
	if (mmcmper == NULL){
		iounmap(mmcmper);
		printk (KERN_ALERT "Falla al acceder a registros CMPER\n");
	}

	mmcontmodule = ioremap (ADDRESS_CONTROLMODULE,LENGHT_CONTROLMODULE);
	if (mmcontmodule == NULL){
		iounmap(mmcontmodule);
		printk (KERN_ALERT "Falla al acceder a registros CONTROL_MODULE\n");
	}


	iowrite32(2, mmcmper + OFFSET_CMPER_I2C2); 									//escribo sobre MODULEMODE (clock de i2c2 de cmper)

	while( ioread32(mmcmper + OFFSET_CMPER_I2C2) != 2){				//cuando este operacional, IDLEST = 0 y queda únicamente MODULEMODE
		msleep(1);
		printk(KERN_ALERT "Configurando registro CM_PER..");
	}

	printk(KERN_ALERT "Finaliza Probe!\n");
	return 0;
}

//******************************************************************************
//REMOVE
//******************************************************************************

static int i2c_remove(struct platform_device *mi_dispo){

	printk(KERN_ALERT "TEST_REMOVE");
	free_irq (virq, NULL);			//se elimina el handler implantado
	iounmap(mmi2c2);
	iounmap(mmcmper);
	iounmap(mmcontmodule);
	printk(KERN_ALERT "Removed!");
	return 0;
}


//******************************************************************************
//OPEN
//******************************************************************************

int i2c_open (struct inode * inode, struct file * file){		//se coloca semaforo que por cada instancia que se tome y sea incapaz de correrse por otro proceso esto
	printk(KERN_ALERT "Comienza funcion open\n");							//de esta forma dos procesos no podrán tomar el driver, se tendrá que poner a dormir hasta que devuelva el semaforo

/*
	iowrite32(0x33, mmcontmodule + OFFSET_PIN_I2C2_SCL);
	iowrite32(0x33, mmcontmodule + OFFSET_PIN_I2C2_SDA);  //101 0010 modo dos, pull up habilitado.

	iowrite32(0,mmi2c2 + OFFSET_CON_I2C2);					// 1 enable, 0 reservado 00 opmode, 0 modo normal , 1 master, 0 modo transmision, 0000000, 0 stop condition, 0 start condition - 100 0010 0000 0000  8400 escritura 8600 lectura
	iowrite32(6,mmi2c2 + OFFSET_PSC_I2C2);  				//clock i2c2 = clock / 256
	iowrite32(17,mmi2c2 + OFFSET_SCLL_I2C2);  				//tlow = (scll + 7) * iclk periodo
	iowrite32(19,mmi2c2 + OFFSET_SCLH_I2C2);					//thigh = (scll + 5) * iclk periodo

	iowrite32(0x36,mmi2c2 + OFFSET_OA_I2C2);				//own address = 10 cambiar a otro valor
	iowrite32(0,mmi2c2 + OFFSET_SYSC_I2C2);		  // 11 clock activity,000reservado , 01 sin modo idle,0 deshabilito wakeup,0 soft reset,0 auto idle 11 0000 1000 = 308

	iowrite32(0x76,mmi2c2 + OFFSET_SA_I2C2);				// Slave address = 0x77
	iowrite32(0x8400,mmi2c2 + OFFSET_CON_I2C2); //1000 0110 0000 0000 (escritura) - 1000 0100 0000 0000(lectura)
*/

	iowrite32(0x00, i2c2_baseAddr + OFFSET_CON_I2C2);
	iowrite32(0x03, i2c2_baseAddr + OFFSET_PSC_I2C2);
	iowrite32(0xE9, i2c2_baseAddr + OFFSET_SCLL_I2C2);
	iowrite32(0xEB, i2c2_baseAddr + OFFSET_SCLH_I2C2);
	iowrite32(0x36, i2c2_baseAddr + OFFSET_OA_I2C2);
	//iowrite32(0x00, i2c2_baseAddr + I2C_IRQSTATUS_XRDY);
	iowrite32(0x76, i2c2_baseAddr + OFFSET_SA_I2C2);
	iowrite32(0x8000, i2c2_baseAddr + OFFSET_CON_I2C2);

	// ----Habilito el capacidades de pines de SDA y SCL-----
	iowrite32(0x33, i2c2_baseAddr + OFFSET_PIN_I2C2_SCL);
	iowrite32(0x33, i2c2_baseAddr + OFFSET_PIN_I2C2_SDA);


	datos_i2c2.buffer_rx = (char *) __get_free_page(GFP_KERNEL);
	if (datos_i2c2.buffer_rx < 0){
		printk (KERN_ALERT "Falla al pedir memoria\n");
		return -1;
	}
	printk(KERN_ALERT "Finaliza funcion open\n");
	return 0;
}

//******************************************************************************
//WRITE
//******************************************************************************

ssize_t i2c_write (struct file * file, const char __user * userbuff,size_t tamano, loff_t* offset){
	int aux;

	printk(KERN_ALERT "Inicio escritura\n");

	if (tamano > PAGE_SIZE){
		printk (KERN_ALERT "Datos recibidos mayor a tamaño de pagina reservada\n");
		return -1;
	}

	if (access_ok(VERIFY_WRITE, userbuff,tamano) == 0){
		printk (KERN_ALERT "Falla buffer de usuario\n");
		return -1;
	}

	if ((datos_i2c2.buffer_tx = (char *) kmalloc (tamano, GFP_KERNEL)) == NULL){
		printk(KERN_ALERT "Falla reserva de memoria para buffer de tx\n");
		return -1;
	}

	if(copy_from_user(datos_i2c2.buffer_tx, userbuff,tamano)>0){				//en copia correcta devuelve 0
		printk(KERN_ALERT "Falla al copiar de buffer de usuario a buffer de kernel\n");
		return -1;
	}

	while (ioread32(mmi2c2 + OFFSET_IRQSTATUS_RAW_I2C2) & 0x1000){
		msleep(1);
	}

	datos_i2c2.longitud_buffer_tx = tamano;
	datos_i2c2.posicion_tx = 0;
	condicion_wake_tx = 0;

	iowrite32 (tamano, mmi2c2 + OFFSET_DCONT_I2C2);				//cantidad de elementos a escribir
	iowrite32 (0x8600, mmi2c2 + OFFSET_CON_I2C2);					//modo transmision

	iowrite32 (0x10, mmi2c2 + OFFSET_IRQENABLE_SET);			//habilitacion de interrupcion de tx
	aux = ioread32 (mmi2c2 + OFFSET_CON_I2C2);
	aux = aux | 1;																				//se activa bit de start
	iowrite32(aux, mmi2c2 + OFFSET_CON_I2C2);

	wait_event_interruptible (mi_queue_tx,condicion_wake_tx > 0);		//tarea en TASK_INTERRUPTIBLE hasta cumplir condicion

	aux = ioread32 (mmi2c2 + OFFSET_CON_I2C2);
	aux = aux | 2;
	iowrite32(aux, mmi2c2 + OFFSET_CON_I2C2);							//se envia condicion de stop

	kfree(datos_i2c2.buffer_tx);

	printk(KERN_ALERT "Finaliza escritura\n");
	return tamano;
}


//******************************************************************************
//READ
//******************************************************************************

ssize_t i2c_read (struct file * file, char __user * userbuff, size_t tamano, loff_t * offset){
	int aux;
	printk(KERN_ALERT "Inicio lectura.\n");

	if (tamano > PAGE_SIZE){
		printk (KERN_ALERT "Datos recibidos mayor a tamaño de pagina reservada.\n");
		return -1;
	}

	if (access_ok(VERIFY_WRITE, userbuff,tamano) == 0){
		printk (KERN_ALERT "Falla buffer de usuario.\n");
		return -1;
	}

	while (ioread32(mmi2c2 + OFFSET_IRQSTATUS_RAW_I2C2) & 0x1000){
		msleep(1);
	}

	datos_i2c2.longitud_buffer_rx = tamano;
	datos_i2c2.posicion_rx = 0;
	condicion_wake_rx = 0;
	iowrite32(tamano, mmi2c2 + OFFSET_DCONT_I2C2);				//cantidad de elementos a leer
	iowrite32(0x8400, mmi2c2 + OFFSET_CON_I2C2);					//modo recepcion

	iowrite32 (0x08, mmi2c2 + OFFSET_IRQENABLE_SET);			//habilitacion de interrupcion de rx
	aux = ioread32(mmi2c2 + OFFSET_CON_I2C2);
	aux = aux | 1;																				//se activa bit de start
	iowrite32(aux, mmi2c2 + OFFSET_CON_I2C2);

	wait_event_interruptible (mi_queue_rx,condicion_wake_rx > 0);		//tarea en TASK_INTERRUPTIBLE hasta cumplir condicion

	aux = ioread32 (mmi2c2 + OFFSET_CON_I2C2);
	aux = aux | 2;
	iowrite32(aux, mmi2c2 + OFFSET_CON_I2C2);							//se envia condicion de stop

	if(copy_to_user(userbuff, datos_i2c2.buffer_rx, tamano)>0){			//en copia correcta devuelve 0
		printk(KERN_ALERT "Falla en copia de buffer de kernel a buffer de usuario\n");
		return -1;
	}

	printk(KERN_ALERT "Finaliza lectura\n");
	return tamano;
}


//******************************************************************************
//HANDLER IRQ
//******************************************************************************

irqreturn_t handler_i2c (int irq, void *dev_id, struct pt_regs *regs){

	int aux;

	aux = ioread32 (mmi2c2 + OFFSET_IRQSTATUS_I2C2);
	iowrite32 (0xFFFF, mmi2c2 + OFFSET_IRQSTATUS_I2C2);		//limpio registros de interrupcion mediante escritura

	if (aux & 0x10){																			//interrupcion de transmision

			printk(KERN_ALERT "Dato escrito:%d\n",datos_i2c2.buffer_tx[datos_i2c2.posicion_tx]);
			iowrite32(datos_i2c2.buffer_tx[datos_i2c2.posicion_tx], mmi2c2 + OFFSET_DATA_I2C2);
			datos_i2c2.posicion_tx++;

			if (datos_i2c2.posicion_tx == datos_i2c2.longitud_buffer_tx){
				iowrite32(0x10, mmi2c2 + OFFSET_IRQENABLE_CLR);						//se desactiva interrupcion al haber terminado de leer
				condicion_wake_tx=1;
				wake_up_interruptible(&mi_queue_tx);
			}
	}

	if (aux & 0x08){																			//interrupcion de recepcion

			datos_i2c2.buffer_rx[datos_i2c2.posicion_rx] = ioread32(mmi2c2 + OFFSET_DATA_I2C2);	//leo datos
			aux = datos_i2c2.buffer_rx[datos_i2c2.posicion_rx];
			printk(KERN_ALERT "Dato leido:%d",aux);
			datos_i2c2.posicion_rx++;

			if (datos_i2c2.posicion_rx == datos_i2c2.longitud_buffer_rx){
				iowrite32(0x08, mmi2c2 + OFFSET_IRQENABLE_CLR);						//se desactiva interrupcion al haber terminado de leer
				condicion_wake_rx=1;
				wake_up_interruptible(&mi_queue_rx);
			}
	}

	return IRQ_HANDLED;
}

module_init(hello_init);
module_exit(hello_exit);
