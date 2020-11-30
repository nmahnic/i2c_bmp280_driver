/**
 * @file   td3_driver.c
 * @author Maximiliano Benchimol
 * @date   14 de octubre de 2020
 * @version 0.2 (estable)
 * @brief  Driver para la implementacion de una comunicacion i2c con 
 *         el dispositivo BMP280.
*/

//Includes
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
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
#include <asm/ioctl.h>
#include <linux/kernel.h>	/* We're doing kernel work */


//Defines inicializacion
#define     first_minor     0
#define     minor_numbers   1
#define     MAX_SIZE_STRING 64

// L4_WAKEUP 
// Defines del clock
#define     CM_PER          0x44E00000
#define     CM_PER_L4LS     0x0
#define     CM_PER_I2C1     0x44     // I2C1 0x48 // I2C2 0x44

// Defines del modulo de control
#define     CTRL_MOD        0x44E10000      // Base del modulo de control
#define     CM_I2C1_SDA     0x978    // I2C1 0x95C // I2C2 0x978
#define     CM_I2C1_SCL     0x97C    // I2C1 0x958 // I2C2 0x97C

// L4_PER
//Defines del modulo de registro
#define     RM_I2C1         0x4819C000 // I2C1 0x4802A000  // I2C2 0x4819C000  

#define     I2C_SYSC        0x10            // Registro de configuracion del sistema
#define     I2C_CON         0xA4            // Registro de configuracion del i2c
#define     I2C_OA          0xA8            // Registro de configuracion de la direccion propia
#define     I2C_SA          0xAC            // Registro de configuracion de la direccion del slave
#define     I2C_PSC         0xB0            // Registro de configuracion del pre-escaler
#define     I2C_SCLL        0xB4            // Registro del SCL LOW TIME
#define     I2C_SCLH        0xB8            // Registro del SCH HIGH TIME

#define     I2C_CNT         0x98            // Registro del contador para el bus            
#define     I2C_DATA        0x9C            // Registro para colocar datos en el buffer

#define     OWN_ADDRESS     0x10


// Defines Interrupcion
#define     IRQSTATUS_RAW   0x24
#define     IRQSTATUS       0x28
#define     IRQENABLE_SET   0x2C            // Escribir 1 setea
#define     IRQENABLE_CLR   0x30            // Escribir 1 borra  (Handler)

// Interpretacion de interrupciones

#define     IRQ_XRDY        0x10
#define     IRQ_RRDY        0x08
#define     IRQ_ARDY        0x04
#define     IRQ_NACK        0x02
#define     IRQ_AL          0x01

#define     ISR_NACK        0x1002
#define     ISR_XRDY        0x1010          // bit de busy bus y bit de XRDY
#define     ISR_RRDY        0x1008          // bit de busy bus y bit de RRDY

#define     COMPATIBLE      "NM_td3_i2c_dev"

DECLARE_WAIT_QUEUE_HEAD(queue_driverI2C);
//static declare_wait_queue_head_t queue_driverI2C;

static      dev_t           myDevice;
static      struct cdev     *ptr_I2Cdev;
static      struct class    *miClaseI2C;
static      struct device   *miDevI2C;

static volatile unsigned char *remapI2C_reg;
static volatile unsigned char *remapClock_reg;
static volatile unsigned char *remapI2CPINs_reg;

static volatile unsigned int  virq;

static struct i2c_struct {
	char *ptr_message_RX;
    char *ptr_message_TX;
	unsigned int cant_RX_ISR;
 	unsigned int cant_APP;  // Chequeo contra lo que mando el usuario en este contador  
    unsigned int cant_TX_ISR;
    unsigned char Device_Open;  // Usado para prevenir multiples accesos al driver
}* miDriverI2C; 

// Genero una estructura que voy a usar para el string del buffer    

int         i2c_open        (struct inode *inode, struct file *file);
int         i2c_close       (struct inode *inode, struct file *file);
ssize_t     i2c_read        (struct file *file,  char __user *userbuff, size_t tamano, loff_t *offsito);
ssize_t     i2c_write       (struct file *file, const char __user *userbuff, size_t tamano, loff_t *offsito);
static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env);

static int  i2c_probe       (struct platform_device *);
static int  i2c_remove      (struct platform_device *);
static irqreturn_t handler_i2c_irq     (int irq, void *dev_id);


/***********************************************************************************
 *                  ----- ESTRUCTURAS PARA EL MANEJO DEL DRIVER -----
 * *********************************************************************************/

// ID del dispositivo

static const struct of_device_id td3_i2c_of_match[] = {
	{
            .compatible = COMPATIBLE
	}, { /* sentinel */ }
};   

// Estructura del file operations

static      struct file_operations i2c_fileops = {
            .owner  = THIS_MODULE,
            .open   = i2c_open,
            .read   = i2c_read,
            .write  = i2c_write,
            .release  = i2c_close,
};

// Estructura del platform driver

static struct platform_driver i2c_driver = {
            .probe  = i2c_probe,
            .remove = i2c_remove,
            .driver = {
            .name   = COMPATIBLE,
            .of_match_table = td3_i2c_of_match,
	},
};

/***********************************************************************************
 *                          ----- MODULO PROBE -----
 * *********************************************************************************/

static int i2c_probe(struct platform_device *device){
    //Declaracion de variables
    u32     data;
    
    // Paginacion de memoria a utilizar
    printk(KERN_NOTICE "-----------------------------------------\n");
    printk(KERN_WARNING "Driver I2C: Funcion Probe I2C iniciando \n");
    printk(KERN_NOTICE "-----------------------------------------\n");

    remapI2C_reg    =   ioremap(RM_I2C1,0x1000);            //Pagino el registro del i2c, son 4kBytes
    remapClock_reg  =   ioremap(CM_PER,0x400);              //Pagino el registro del Clock, son 1kBytes
    remapI2CPINs_reg    =   ioremap(CTRL_MOD,0x2000);       //Pagino el registro de Control Module, son 8192 Bytes

    // Habilitacion de Clock y pines

    // Habilitacion Clock 
    iowrite32(0x02, remapClock_reg+CM_PER_I2C1);
    data   =   ioread32(remapClock_reg+CM_PER_I2C1);
    printk(KERN_WARNING "Driver I2C: Registro CM_PER_I2C1 -> %08x\n",data);

    data   =   ioread32(remapClock_reg+CM_PER_L4LS);
    printk(KERN_WARNING "Driver I2C: Registro CM_PER_L4LS -> %08x\n",data);
    
    // Software reset colocando un uno en SRST de SYSC
    printk(KERN_WARNING "Driver I2C: Haciendo un Soft-Reset\n");
    iowrite32(0x00, remapI2C_reg+I2C_CON);
    msleep(4);

    // Habilitacion Pines
     // I2C1_SCL
    iowrite32(0x32, remapI2CPINs_reg+CM_I2C1_SCL);  // Configuro:
                                             //  - Slow Slew Rate, Output, Pull-up y modo 2             
    // I2C1_SDA
    iowrite32(0x32, remapI2CPINs_reg+CM_I2C1_SDA);  // Configuro:
                                             //  - Slow Slew Rate, Output, Pull-up y modo 2 

    // Configuracion del modulo previo a la habilitacion
    printk(KERN_WARNING "Driver I2C: Configuracion del modulo\n");
    
    // 1- PRESCALER, se lo configura para obtener 12MHz
    // Se puede dividir por 1 hasta 256
    // El clock del modulo es de 48MHz y se divide por el valor del PSC + 1
    // 48 / (PSC + 1) = 12 MHz ---> PSC = 3
    iowrite32(3, remapI2C_reg+I2C_PSC); 

    // 2- SCLL y SCLH para obtener 400Kbps
    // Para obtener el tiempo tLOW se basa en la siguiente formula:
    // tLOW = (SCLL + 7) * ICLK
    iowrite32(53, remapI2C_reg+I2C_SCLL);  // 0x35 --> 53 decimal (400Kbps), 0xE9 --> 233 (100Kbps)
    
    // Para obtener el tiempo tHIGH se basa en la siguiente formula:
    // tHIGH = (SCLH + 5) * ICLK
    iowrite32(55, remapI2C_reg+I2C_SCLH);  // 0x37 --> 55 decimal (400Kbps), 0xEB --> 235 (100Kbps)

    // 3- Configuro la direccion propia
    iowrite32(OWN_ADDRESS, remapI2C_reg+I2C_OA);

    // Implantacion de VIRQ e interrupcion
    virq = platform_get_irq(device,0);
    if(request_irq (virq, (irq_handler_t) handler_i2c_irq, IRQF_TRIGGER_RISING, device->name, NULL)){
        printk(KERN_WARNING "Driver I2C: No se pudo hacer request de irq");  
        iounmap(remapI2C_reg); //Deshago la paginacion 
        iounmap(remapClock_reg);
        iounmap(remapI2CPINs_reg);
        return 1;
    }
    //printk(KERN_WARNING "Driver I2C: Request de irq con exito");

    // Finalizacion de funcion probe
    printk(KERN_NOTICE "-----------------------------------------\n");
    printk(KERN_WARNING "Driver I2C: Funcion Probe Finalizada\n"); 
    printk(KERN_NOTICE "-----------------------------------------\n");

    return 0;
}

/***********************************************************************************
 *                          ----- MODULO REMOVE -----
 * *********************************************************************************/

static int i2c_remove(struct platform_device *device){
  printk(KERN_WARNING "Driver I2C: funcion remove \n");
  iounmap(remapI2C_reg); //Deshago la paginacion 
  iounmap(remapClock_reg);
  iounmap(remapI2CPINs_reg);
  return 0;
}

/***********************************************************************************
 *                          ----- MODULO IRQHANDLER -----
 * *********************************************************************************/

static irqreturn_t handler_i2c_irq (int irq, void *dev_id){
    u32 aux = 0;
    u32 data;
    char *ptr;

    printk(KERN_WARNING "Driver I2C: Entrando al handler");
    aux =  ioread32(remapI2C_reg+IRQSTATUS); 
    aux = aux & 0xFF; // Me interesan solo los primeros 8 bits
    printk(KERN_WARNING "Driver I2C: Valor de interrupciones %02x, XRDY=0x10,RRDY=0x08,ARDY=0x04,NACK=0x02,AL=0x01\n",aux); 
    
    switch(aux)
    {
        case IRQ_AL:   // Arbitration lost
            printk(KERN_WARNING "Driver I2C: HANDLER IRQ AL");
            // Deshabilito la interrupcion de AL para una proxima transmision
            iowrite32(IRQ_AL, remapI2C_reg+IRQSTATUS);
            break; 
        
        case IRQ_NACK: // No acknowledgement
            printk(KERN_WARNING "Driver I2C: HANDLER IRQ NACK");
            // Deshabilito la interrupcion de NACK para una proxima transmision
            iowrite32(ISR_NACK, remapI2C_reg+IRQSTATUS);

            break;
        case IRQ_ARDY: // Register access ready
            printk(KERN_WARNING "Driver I2C: HANDLER IRQ ARDY");
            // Deshabilito la interrupcion de ARDY para una proxima transmision
            iowrite32(IRQ_ARDY, remapI2C_reg+IRQSTATUS);

            break;
        case IRQ_RRDY: // Receive data ready
            //printk(KERN_WARNING "Driver I2C: HANDLER IRQ RRDY");

            // Obtengo el puntero a mi buffer RX
            ptr = miDriverI2C->ptr_message_RX;

            // Recibo el mensaje en mi buffer
            *(ptr + miDriverI2C->cant_RX_ISR) = ioread32(remapI2C_reg+I2C_DATA);

            // Incremento indice de cantidad de veces interrumpida en RX
            miDriverI2C->cant_RX_ISR++;

            // Deshabilito la interrupcion de RRDY para una proxima recepcion            
            iowrite32(IRQ_RRDY, remapI2C_reg+IRQSTATUS);

            // Comparo contra lo que pidio el usuario
            if (miDriverI2C->cant_RX_ISR == miDriverI2C->cant_APP)
            {
                miDriverI2C->cant_APP = 0;                
                wake_up_interruptible(&queue_driverI2C);  // Permito seguir con el read
            }
            break;

        case IRQ_XRDY: // Transmit data ready
            //printk(KERN_WARNING "Driver I2C: HANDLER IRQ XRDY");

            // Obtengo el puntero a mi buffer TX
            ptr = miDriverI2C->ptr_message_TX;
            
            // Coloco el dato en buffer
            iowrite32(*(ptr + miDriverI2C->cant_TX_ISR), remapI2C_reg+I2C_DATA);

            // Incremento indice de cantidad de veces interrumpida en TX
            miDriverI2C->cant_TX_ISR++;

            // Deshabilito la interrupcion de XRDY para una proxima transmision
            iowrite32(ISR_XRDY, remapI2C_reg+IRQSTATUS);

            // Comparo contra lo que pidio el usuario
            if (miDriverI2C->cant_TX_ISR == miDriverI2C->cant_APP)
            {
                miDriverI2C->cant_APP = 0;                
                wake_up_interruptible(&queue_driverI2C);
            }
            break;

        default:
            printk(KERN_WARNING "Driver I2C: HANDLER DE EVENTO NO RECONOCIDO");
            //iowrite32(aux, remapI2C_reg+IRQSTATUS);
            data = ioread32(remapI2C_reg+IRQSTATUS);
            printk(KERN_WARNING "Driver I2C: Valor de interrupciones %08x\n",data); 
		    return IRQ_NONE;
    }

    // Devuelvo IRQ_HANDLED para avisarle al kernel que mi IRQ fue atendida correctamente
    return IRQ_HANDLED;
}

/***********************************************************************************
 *                          ----- MODULO OPEN -----
 * *********************************************************************************/

int i2c_open (struct inode *inode, struct file *file){
    if ( miDriverI2C->Device_Open == 1 )
    {
        printk(KERN_ALERT "Driver I2C: Driver en uso\n");
        return -EBUSY;
    }
    miDriverI2C->Device_Open++;
    printk(KERN_NOTICE "-----------------------------------------\n");
	printk(KERN_WARNING "Driver I2C: Driver abierto \n");
    printk(KERN_NOTICE "-----------------------------------------\n");
    
    return 0;
}

/***********************************************************************************
 *                          ----- MODULO READ -----
 * *********************************************************************************/

ssize_t i2c_read (struct file *file, char __user *userbuff, size_t tamano, loff_t *offset){
     // Realizo la cuenta de la cantidad de datos que lei
	int bytes_read = 0;
    int i;
    //u32 data;
    
    pr_info("Driver I2C: READ abierto");

    if(access_ok(VERIFY_WRITE, userbuff, tamano) == 0){
		pr_alert(" Lectura: Falla buffer de usuario\n");
		return -1;
	}

    // Me aseguro que el buffer no exceda el maximo de lectura de datos de kernel
    if (tamano >= MAX_SIZE_STRING) {
		tamano = MAX_SIZE_STRING -1;
	}

	// Si estamos al final del mensaje, devolvemos un 0 EOF
	/*
    if (copy_from_user(miDriverI2C->ptr_message_RX, userbuff, tamano) != 0)
    {   
        printk(KERN_WARNING "Driver I2C: No se puedo copiar el buffer de usario\n");
        return -EFAULT;
    }
    */
    //printk(KERN_WARNING "Driver I2C: buffer copiado a kernel");

    // Establezco el I2C como MASTER
    // - Habilito el bit 10 de I2C_CON (Modo Master)
    // - Deshabilito el bit 9 de I2C_CON (Para recibir)
    iowrite32(0x8400, remapI2C_reg+I2C_CON);
    //printk(KERN_WARNING "Driver I2C: configurado master y receiver");

    // Establezco I2C_IRQENABLE_SET para recibir (RRDY)
    // - Bit 3 del registro: 0x08
    iowrite32(IRQ_RRDY, remapI2C_reg+IRQENABLE_SET);
    //data = ioread32(remapI2C_reg+IRQSTATUS_RAW);
    //printk(KERN_WARNING "Driver I2C: Estado IRQENABLE_SET %08x\n",data); 

    // Coloco la direccion del esclavo
    iowrite32(0x76, remapI2C_reg+I2C_SA);
    //printk(KERN_WARNING "Driver I2C: Valor del SLAVE %08x\n",data);  
 
    // Establezco la cantidad de datos que va a recibir
    iowrite32(tamano, remapI2C_reg+I2C_CNT);
    //data = ioread32(remapI2C_reg+I2C_CNT);
    //printk(KERN_WARNING "Driver I2C: Cantidad de datos a recibir %08x\n",data);

    // Copio el tamaño para chequeo y salir de la cola
    miDriverI2C->cant_APP=tamano;

    // Verifico que el bus no este ocupado
    if((ioread32(remapI2C_reg+IRQSTATUS_RAW)& 0x1000) == 0x1000)
    {
        printk(KERN_WARNING "Driver I2C: BUS Ocupado en READ");
        msleep(10); // Espero un tiempo
    }
    // Llegue aca? bus no ocupado
    //printk(KERN_WARNING "Driver I2C: BUS NO ocupado en READ");

    // Todo configurado, mando bit de start y stop
    // - Bit 1: STOP  
    // - Bit 0: START    
    iowrite32(0x8403, remapI2C_reg+I2C_CON);
    //printk(KERN_WARNING "Driver I2C: Mandando bit start y stop");

    // Espero un tiempo para que interrumpa y no llegue a la espera de la cola
    msleep(10);

    // Coloco una cola para que avance ni bien se atienda la interrupcion
    wait_event_interruptible(queue_driverI2C, (miDriverI2C->cant_APP == 0));

    // Chequeo la cantidad de bytes recibidos
    bytes_read = miDriverI2C->cant_RX_ISR; 

    // Copio el buffer al usuario
	if (copy_to_user(userbuff, miDriverI2C->ptr_message_RX, bytes_read) != 0)
    {   
        printk(KERN_WARNING "Driver I2C: No se puedo copiar el buffer al usuario\n");
        return -EFAULT;
    }

    // Si mi interrupcion llego hasta aca, la variable APP debe ser 0, caso contrario hubo error
    if(miDriverI2C->cant_APP == 0)
    {
        printk(KERN_NOTICE "-----------------------------------------\n");
        printk(KERN_NOTICE "Driver I2C: Lectura realizada con exito\n");
        printk(KERN_NOTICE "Cantidad de bytes recibidos: %08x\n",bytes_read);
        printk(KERN_NOTICE "-----------------------------------------\n");
    }
    else
    {
        printk(KERN_NOTICE "-----------------------------------------\n");
        printk(KERN_NOTICE "Driver I2C: ERROR en la lectura %d\n", miDriverI2C->cant_APP);
        printk(KERN_NOTICE "-----------------------------------------\n");
    }
    
    // Limpio el buffer
    for (i=0; i<MAX_SIZE_STRING; i++)
       *(miDriverI2C->ptr_message_RX + i)=0;
    // Limpio la cant_RX_ISR
    miDriverI2C->cant_RX_ISR=0;
    miDriverI2C->cant_APP=0;

	// La mayoria de funciones devuelven el numero de bytes que se puso en el buffer
	return bytes_read;
}

/***********************************************************************************
 *                          ----- MODULO WRITE -----
 * *********************************************************************************/

//Escritura de driver
ssize_t i2c_write (struct file *file, const char __user *userbuff, size_t tamano, loff_t *offset){
    // write es solo para configuraciones o peticiones a los esclavos 
    //u32 data;
	int bytes_write = 0;
    int i;

    //printk(KERN_WARNING "Driver I2C: WRITE abierto");

    // Me aseguro que el buffer no exceda para que no pise datos de kernel
    if (tamano >= MAX_SIZE_STRING) {
		tamano = MAX_SIZE_STRING -1;
	}

	// Si estamos al final del mensaje, devolvemos un 0 EOF
	if (copy_from_user(miDriverI2C->ptr_message_TX, userbuff, tamano) != 0)
    {   
        printk(KERN_WARNING "Driver I2C: No se puedo copiar el buffer de usuario\n");
        return -EFAULT;
    }
    //printk(KERN_WARNING "Driver I2C: buffer copiado a kernel");
    
 
    // Establezco el I2C como MASTER
    // - Habilito el bit 10 de I2C_CON (Modo Master)
    // - Habilito el bit 9 de I2C_CON (Para transmitir)
    iowrite32(0x8600, remapI2C_reg+I2C_CON);
    //data = ioread32(remapI2C_reg+I2C_CON);
    //printk(KERN_WARNING "Driver I2C: Valor de I2C_CON chequeo M y T %08x\n",data); 

    // Establezco I2C_IRQENABLE_SET para transmitir (XRDY)
    // - Bit 4 del registro: 0x10
    iowrite32(IRQ_XRDY, remapI2C_reg+IRQENABLE_SET);
    //data = ioread32(remapI2C_reg+IRQSTATUS);
    //printk(KERN_WARNING "Driver I2C: Estado IRQENABLE_SET %08x\n",data);    

     // Coloco la direccion del esclavo
    iowrite32(0x76, remapI2C_reg+I2C_SA);
    //data = ioread32(remapI2C_reg+I2C_SA);
    //printk(KERN_WARNING "Driver I2C: Valor del SLAVE %08x\n",data);  
 
    // Establezco la cantidad de datos que va a enviar
    iowrite32(tamano, remapI2C_reg+I2C_CNT);
    //data = ioread32(remapI2C_reg+I2C_CNT);
    //printk(KERN_WARNING "Driver I2C: Cantidad de datos a enviar %08x\n",data); 

    miDriverI2C->cant_APP=tamano;

    // Verifico que el bus no este ocupado
    if((ioread32(remapI2C_reg+IRQSTATUS_RAW)& 0x1000) == 0x1000)
    {
        printk(KERN_WARNING "Driver I2C: BUS Ocupado en WRITE");
        msleep(10); // Espero un tiempo para que se libere
    }
    // Llegue aca? bus no ocupado
    //printk(KERN_WARNING "Driver I2C: BUS NO Ocupado WRITE");

    // Todo configurado, mando bit de start y stop
    // - Bit 1: STOP  
    // - Bit 0: START   
    //printk(KERN_WARNING "Driver I2C: Mandando bit start y stop");
    iowrite32(0x8603, remapI2C_reg+I2C_CON);
    
    // Espero un tiempo para que interrumpa y no llegue a la espera de la cola
    msleep(10);

    // Coloco una cola para que avance ni bien se atienda la interrupcion
    wait_event_interruptible(queue_driverI2C, (miDriverI2C->cant_APP == 0));

    // Verifico la cantidad que envie
    bytes_write = miDriverI2C->cant_TX_ISR; 

    // Si mi interrupcion llego hasta aca, la variable APP debe ser 0, caso contrario hubo error
    if(miDriverI2C->cant_APP == 0)
    {
        printk(KERN_NOTICE "-----------------------------------------\n");
        printk(KERN_NOTICE "Driver I2C: Escritura realizada con exito\n");
        printk(KERN_NOTICE "Cantidad de bytes enviados: %08x\n",bytes_write);        
        printk(KERN_NOTICE "-----------------------------------------\n");
    }
    else
    {
        printk(KERN_NOTICE "-----------------------------------------\n");
        printk(KERN_NOTICE "Driver I2C: ERROR en la escritura\n");
        printk(KERN_NOTICE "-----------------------------------------\n");
    }
    
    // Limpio el buffer
    for (i=0; i<MAX_SIZE_STRING; i++)
       *(miDriverI2C->ptr_message_TX + i)=0;
    // Limpio la cant_TX_ISR
    miDriverI2C->cant_TX_ISR=0;
    miDriverI2C->cant_APP=0;


    printk(KERN_NOTICE "Finaliza escritura %d\n",bytes_write);
    return bytes_write;
}

/***********************************************************************************
 *                          ----- MODULO CLOSE -----
 * *********************************************************************************/

int i2c_close (struct inode * inode, struct file * file){
    miDriverI2C->Device_Open--;
	printk(KERN_WARNING "Driver I2C: Cerrando el driver \n");
    printk(KERN_NOTICE "-----------------------------------------\n");

    return 0;
}

/***********************************************************************************
 *                          ----- MODULO INIT -----
 * *********************************************************************************/

static int __init i2c_init(void) {
    int i = 0;

    printk(KERN_NOTICE "-----------------------------------------\n");    
    printk(KERN_WARNING "Driver I2C: Instalando Modulo..\n");
    printk(KERN_NOTICE "-----------------------------------------\n");
    
    // Hago la registracion
    alloc_chrdev_region (&myDevice, first_minor, minor_numbers, COMPATIBLE); 
    ptr_I2Cdev = cdev_alloc(); //Esta funcion no tiene par, no necesita nada en el exit
    
    //Asocio la estructura cdev con el puntero correspondiente a file operations

    ptr_I2Cdev -> ops = &i2c_fileops;
    ptr_I2Cdev -> owner = THIS_MODULE;
    ptr_I2Cdev -> dev = myDevice;

    //Se completa la registración del dispositivo
    cdev_add (ptr_I2Cdev,myDevice,1);
    //Creo la clase
    miClaseI2C = class_create(THIS_MODULE, "Temperatura");
    miClaseI2C->dev_uevent = change_permission_cdev;
    //Creo el device
    miDevI2C = device_create(miClaseI2C,NULL,myDevice,NULL,"Temperatura_I2C");
    //Registro el driver
    platform_driver_register(&i2c_driver);
    
    // Pido memoria para la estructura y buffers
    miDriverI2C = (struct i2c_struct *) kmalloc(sizeof(struct i2c_struct), GFP_KERNEL);
    miDriverI2C ->  ptr_message_RX = (char *) kmalloc(MAX_SIZE_STRING, GFP_KERNEL);
    miDriverI2C ->  ptr_message_TX = (char *) kmalloc(MAX_SIZE_STRING, GFP_KERNEL);
    
    // Inicializo los buffers en 0
    for (i=0; i<MAX_SIZE_STRING; i++)
    {
       *(miDriverI2C->ptr_message_RX + i)=0;
       *(miDriverI2C->ptr_message_TX + i)=0;
    }
    // Inicializo contadores
    miDriverI2C->cant_RX_ISR=0;
    miDriverI2C->cant_APP=0;
    miDriverI2C->cant_TX_ISR=0;
    miDriverI2C->Device_Open=0;

    // Inicializo mi cola
    init_waitqueue_head(&queue_driverI2C);

    printk (KERN_WARNING  "Driver I2C: Modulo instalado, numero mayor asignado:%d\n",MAJOR(myDevice));

    return 0;
}

/***********************************************************************************
 *                          ----- MODULO EXIT -----
 * *********************************************************************************/

//Funcion de cierre
static void __exit i2c_exit(void) {
    printk(KERN_WARNING "Driver I2C: Quitando el driver..\n");

    //Des-habilito las interrupciones
    free_irq (virq, NULL);
     //Lo primero en deshacer es lo ultimo que hice, o sea libero la memoria
    kfree(miDriverI2C->ptr_message_TX);
    kfree(miDriverI2C->ptr_message_RX);
    kfree(miDriverI2C);

    //Des-registro el driver
    platform_driver_unregister(&i2c_driver);
    //Destruyo el dispositivo
    device_destroy (miClaseI2C,myDevice);
    //Destruyo la clase
    class_destroy (miClaseI2C);
    //Desasocio la estructura
    cdev_del(ptr_I2Cdev);

    //Elimino el registro del device
    unregister_chrdev_region (myDevice,minor_numbers);
    printk(KERN_NOTICE "-----------------------------------------\n");    
    printk(KERN_WARNING "Driver I2C: Driver quitado exitosamente\n");
    printk(KERN_NOTICE "-----------------------------------------\n");
}

static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}


module_init(i2c_init);
module_exit(i2c_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Maximiliano Benchimol");   
MODULE_DESCRIPTION("Driver I2C para TDIII");  
MODULE_VERSION("0.1");            
MODULE_DEVICE_TABLE(of, td3_i2c_of_match);