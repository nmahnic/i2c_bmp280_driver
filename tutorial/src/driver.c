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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#include "driver.h"
 
//#define I2C_BUS_AVAILABLE   (           2 )             // I2C Bus available in our Raspberry Pi
#define SLAVE_DEVICE_NAME   ( "NMahnic_BMP280")               // Device and Driver Name

static struct i2c_adapter *etx_i2c_adapter     = NULL;    // I2C Adapter Structure
static struct i2c_client  *etx_i2c_client_sensor = NULL;  // I2C Cient Structure (In our case it is SENSOR)
 
static struct sensor bmp280;

int I2C_BUS_AVAILABLE;                                    // This external param gives number of I2C available
module_param(I2C_BUS_AVAILABLE, int, S_IWUSR);            //integer value

/*
** This function writes the data into the I2C client
**
**  Arguments:
**      buff -> buffer to be sent
**      len  -> Length of the data
**   
*/
static int I2C_Write(unsigned char *buf, unsigned int len)
{
    /*
    ** Sending Start condition, Slave address with R/W bit, 
    ** ACK/NACK and Stop condtions will be handled internally.
    */ 
    int status = i2c_master_send(etx_i2c_client_sensor, buf, len);
    
    return status;
}
 
/*
** This function reads one byte of the data from the I2C client
**
**  Arguments:
**      out_buff -> buffer wherer the data to be copied
**      len      -> Length of the data to be read
** 
*/
static int I2C_Read(unsigned char *out_buf, unsigned int len)
{
    /*
    ** Sending Start condition, Slave address with R/W bit, 
    ** ACK/NACK and Stop condtions will be handled internally.
    */ 
    int status = i2c_master_recv(etx_i2c_client_sensor, out_buf, len);
    
    return status;
}
 
/*
** This function getting called when the slave has been found
** Note : This will be called only once when we load the driver.
*/
static int etx_bmp280_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
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
 
/*
** This function getting called when the slave has been removed
** Note : This will be called only once when we unload the driver.
*/
static int etx_bmp280_remove(struct i2c_client *client)
{   
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
 
/*
** Structure that has slave device id
*/
static const struct i2c_device_id etx_bmp280_id[] = {
        { SLAVE_DEVICE_NAME, 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, etx_bmp280_id);
 
/*
** I2C driver Structure that has to be added to linux
*/
static struct i2c_driver etx_bmp280_driver = {
        .driver = {
            .name   = SLAVE_DEVICE_NAME,
            .owner  = THIS_MODULE,
        },
        .probe          = etx_bmp280_probe,
        .remove         = etx_bmp280_remove,
        .id_table       = etx_bmp280_id,
};
 
/*
** I2C Board Info strucutre
*/
static struct i2c_board_info sensor_i2c_board_info = {
        I2C_BOARD_INFO(SLAVE_DEVICE_NAME, BMP280_ADDRESS_ALT)
    };
 
/*
** Module Init function
*/
static int __init etx_driver_init(void)
{
    int status= -1;

    printk(KERN_INFO "I2C_BUS_AVAILABLE = %d  \n", I2C_BUS_AVAILABLE);

    etx_i2c_adapter     = i2c_get_adapter(I2C_BUS_AVAILABLE);
    
    if( etx_i2c_adapter != NULL )
    {
        etx_i2c_client_sensor = i2c_new_device(etx_i2c_adapter, &sensor_i2c_board_info);
        
        if( etx_i2c_client_sensor != NULL )
        {
            i2c_add_driver(&etx_bmp280_driver);
            status = 0;
        }
        
        i2c_put_adapter(etx_i2c_adapter);
    }
    
    pr_info("%s: Driver Added!!!\n", SLAVE_DEVICE_NAME);
    return status;
}
 
/*
** Module Exit function
*/
static void __exit etx_driver_exit(void)
{
    i2c_unregister_device(etx_i2c_client_sensor);
    i2c_del_driver(&etx_bmp280_driver);
    pr_info("%s: Driver Removed!!!\n", SLAVE_DEVICE_NAME);
}
 
module_init(etx_driver_init);
module_exit(etx_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicolas Mahnic <nico.mahnic94@gmail.com>");
MODULE_DESCRIPTION("Simple I2C driver BMP280 SENSOR");
MODULE_VERSION("0.1");