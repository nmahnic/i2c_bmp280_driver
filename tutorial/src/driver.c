/***************************************************************************//**
*  \file       driver.c
*
*  \details    Simple I2C driver BMP280 SENSOR
*
*  \author     Nicolas Mahnic
*
*  \brief      Using a functions from the bus driver (drivers/i2c/busses/)
*
*  \Tested with Linux arm 4.19.94-ti-r36 for Beaglebone Black rev.C
*
* *******************************************************************************/
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/cdev.h>             // Char device: File operation struct,
#include <linux/kdev_t.h> 			//agregar en /dev
#include <linux/device.h> 			//agregar en /dev

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

#include "driver.h"
 
//#define I2C_BUS_AVAILABLE   (           2 )             // I2C Bus available in our Raspberry Pi
#define SLAVE_DEVICE_NAME   ( "NMahnic_BMP280")               // Device and Driver Name

static struct i2c_adapter *etx_i2c_adapter     = NULL;    // I2C Adapter Structure
static struct i2c_client  *etx_i2c_client_sensor = NULL;  // I2C Cient Structure (In our case it is SENSOR)
 
static struct sensor bmp280;

int I2C_BUS_AVAILABLE;                                    // This external param gives number of I2C available
module_param(I2C_BUS_AVAILABLE, int, S_IWUSR);            //integer value


static int I2C_Write(unsigned char *buf, unsigned int len){
    int status = i2c_master_send(etx_i2c_client_sensor, buf, len);
    return status;
}
 
static int I2C_Read(unsigned char *out_buf, unsigned int len){
    int status = i2c_master_recv(etx_i2c_client_sensor, out_buf, len);
    return status;
}
 
static int etx_bmp280_probe(struct i2c_client *client, const struct i2c_device_id *id){
    int status;
    char config[2] = {0};
    char reg[1] = {0};
    char data[2] = {0};

    config[0] = BMP280_REGISTER_SOFTRESET;      //Le escribo al registro de reset
	config[1] = MODE_SOFT_RESET_CODE;           //Le escribo el comando RST
    if((status = I2C_Write(config, 2)) < 0){
		pr_alert("%s: It's not possible Reset\n", SLAVE_DEVICE_NAME);
		return status;
	}
    pr_info("%s: Reset and Probed\n", SLAVE_DEVICE_NAME);

    reg[0] = BMP280_REGISTER_CHIPID;            //Le indico que registro quiero leer
    if((status = I2C_Write(reg, 1)) < 0){
		pr_alert("%s: It's not possible ask for CHIPID\n", SLAVE_DEVICE_NAME);
		return status;
	}
    if((status = I2C_Read(data, 1)) < 0){
		pr_alert("%s: It's not possible get CHIPID\n", SLAVE_DEVICE_NAME);
		return status;
	}                   
    if(data[0] != BMP280_CHIPID){
        pr_info("%s: Driver wasn't probe, it's removed!!!\n", SLAVE_DEVICE_NAME);
    }
    pr_info("%s: chip_ID = 0x%X\n", SLAVE_DEVICE_NAME,data[0]);
    
    bmp280.dig_T1 = 2;

    return 0;
}
 
static int etx_bmp280_remove(struct i2c_client *client){   
    int status;
    char config[2] = {0};;

    pr_info("%s: dig_T1: %x\n", SLAVE_DEVICE_NAME, bmp280.dig_T1);

    config[0] = BMP280_REGISTER_SOFTRESET;      //Le escribo al registro de reset
	config[1] = MODE_SOFT_RESET_CODE;           //Le escribo el comando RST
    if((status = I2C_Write(config, 2)) < 0){
		pr_alert("%s: It's not possible Reset\n", SLAVE_DEVICE_NAME);
		return status;
	}
    pr_info("%s: Reset and Removed\n", SLAVE_DEVICE_NAME);

    return 0;
}
 
static const struct i2c_device_id etx_bmp280_id[] = {
        { SLAVE_DEVICE_NAME, 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, etx_bmp280_id);
 
static struct i2c_driver etx_bmp280_driver = {
        .driver = {
            .name   = SLAVE_DEVICE_NAME,
            .owner  = THIS_MODULE,
        },
        .probe          = etx_bmp280_probe,
        .remove         = etx_bmp280_remove,
        .id_table       = etx_bmp280_id,
};
 
static struct i2c_board_info sensor_i2c_board_info = {
        I2C_BOARD_INFO(SLAVE_DEVICE_NAME, BMP280_ADDRESS_ALT)
};
 
static int __init etx_driver_init(void){
    int status= -1;

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

    printk(KERN_INFO "I2C_BUS_AVAILABLE = %d  \n", I2C_BUS_AVAILABLE);

    etx_i2c_adapter     = i2c_get_adapter(I2C_BUS_AVAILABLE);
    
    if( etx_i2c_adapter != NULL ){
        etx_i2c_client_sensor = i2c_new_device(etx_i2c_adapter, &sensor_i2c_board_info);
        
        if( etx_i2c_client_sensor != NULL ){
            i2c_add_driver(&etx_bmp280_driver);
            status = 0;
        }
        
        i2c_put_adapter(etx_i2c_adapter);
    }
    
    pr_info("%s: Driver Added!!!\n", SLAVE_DEVICE_NAME);
    return status;
}

static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}
 
/*
** Module Exit function
*/
static void __exit etx_driver_exit(void){
    i2c_unregister_device(etx_i2c_client_sensor);
    i2c_del_driver(&etx_bmp280_driver);
    pr_info("%s: Driver Removed!!!\n", SLAVE_DEVICE_NAME);
}
 
static int NMopen(struct inode *inode, struct file *file) {
    int status;
    char config[2] = {0};
    char reg[1] = {0};
    char data[2] = {0};

    config[0] = BMP280_REGISTER_SOFTRESET;      //Le escribo al registro de reset
	config[1] = MODE_SOFT_RESET_CODE;           //Le escribo el comando RST
    if((status = I2C_Write(config, 2)) < 0){
		pr_alert("%s: It's not possible Reset\n", SLAVE_DEVICE_NAME);
		return status;
	}
    pr_info("%s: Reset and Probed\n", SLAVE_DEVICE_NAME);

    reg[0] = BMP280_REGISTER_CHIPID;            //Le indico que registro quiero leer
    if((status = I2C_Write(reg, 1)) < 0){
		pr_alert("%s: It's not possible ask for CHIPID\n", SLAVE_DEVICE_NAME);
		return status;
	}
    if((status = I2C_Read(data, 1)) < 0){
		pr_alert("%s: It's not possible get CHIPID\n", SLAVE_DEVICE_NAME);
		return status;
	}                   
    if(data[0] != BMP280_CHIPID){
        pr_info("%s: Driver wasn't probe, it's removed!!!\n", SLAVE_DEVICE_NAME);
    }
    pr_info("%s: chip_ID = 0x%X\n", SLAVE_DEVICE_NAME,data[0]);
    
    bmp280.dig_T1 = 2;

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

	//LEER

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

	//ESCRIBIR

	kfree(data_i2c.buff_tx);

	pr_info("%s: Finaliza escritura\n", DEVICE_NAME);
	return write_len;
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicolas Mahnic <nico.mahnic94@gmail.com>");
MODULE_DESCRIPTION("Simple I2C driver BMP280 SENSOR");
MODULE_VERSION("0.1");