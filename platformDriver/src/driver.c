/***************************************************************************//**
*  \file       driver.c
*
*  \details    Simple I2C driver for BMP280
*
*  \author     Nicolas Mahnic
*
*  \Tested with Beaglebone Black rev.C
*
* *******************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#include "driver.h"
 
#define I2C_BUS_AVAILABLE   (            2 )            // I2C Bus available in our Raspberry Pi
#define SLAVE_DEVICE_NAME   ( "ETX_SENSOR" )            // Device and Driver Name
#define SSD1306_SLAVE_ADDR  (         0x76 )            // SSD1306 OLED Slave Address
 
static struct i2c_adapter *etx_i2c_adapter     = NULL;  // I2C Adapter Structure
static struct i2c_client  *etx_i2c_client_sensor = NULL;  // I2C Cient Structure (In our case it is OLED)
 
static struct sensor bmp280;
/*
//This function writes the data into the I2C client
//
// Arguments:
//     buff -> buffer to be sent
//     len  -> Length of the data
//  
*/
static int I2C_Write(unsigned char *buf, unsigned int len)
{
    /*
    //Sending Start condition, Slave address with R/W bit, 
    //ACK/NACK and Stop condtions will be handled internally.
    */ 
    int ret = i2c_master_send(etx_i2c_client_sensor, buf, len);
    
    return ret;
}
 
/*
//This function reads one byte of the data from the I2C client
//
// Arguments:
//     out_buff -> buffer wherer the data to be copied
//     len      -> Length of the data to be read
//
*/
static int I2C_Read(unsigned char *out_buf, unsigned int len)
{
    /*
    //Sending Start condition, Slave address with R/W bit, 
    //ACK/NACK and Stop condtions will be handled internally.
    */ 
    int ret = i2c_master_recv(etx_i2c_client_sensor, out_buf, len);
    
    return ret;
}
 
/*
//This function is specific to the SSD_1306 OLED.
//This function sends the command/data to the OLED.
//
// Arguments:
//     is_cmd -> true = command, flase = data
//     data   -> data to be written
//
*/
/*
static void BMP280_Write(bool is_cmd, unsigned char data)
{
    unsigned char buf[2] = {0};
    int ret;
    
    
    //First byte is always control byte. Data is followed after that.
    //
    //There are two types of data in SSD_1306 OLED.
    //1. Command
    //2. Data
    //
    //Control byte decides that the next byte is, command or data.
    //
    //-------------------------------------------------------                        
    //|              Control byte's | 6th bit  |   7th bit  |
    //|-----------------------------|----------|------------|    
    //|   Command                   |   0      |     0      |
    //|-----------------------------|----------|------------|
    //|   data                      |   1      |     0      |
    //|-----------------------------|----------|------------|
    //
    //Please refer the datasheet for more information. 
    //   
    
    if( is_cmd == true )
    {
        buf[0] = 0x00;
    }
    else
    {
        buf[0] = 0x40;
    }
    
    buf[1] = data;
    
    ret = I2C_Write(buf, 2);
}
*/

/*
static float readTemperature() {
  int32_t var1, var2;

  int32_t adc_T = read24(BMP280_REGISTER_TEMPDATA);
  adc_T >>= 4;

  var1 = ((((adc_T >> 3) - ((int32_t)_bmp280_calib.dig_T1 << 1))) *
          ((int32_t)_bmp280_calib.dig_T2)) >>
         11;

  var2 = (((((adc_T >> 4) - ((int32_t)_bmp280_calib.dig_T1)) *
            ((adc_T >> 4) - ((int32_t)_bmp280_calib.dig_T1))) >>
           12) *
          ((int32_t)_bmp280_calib.dig_T3)) >>
         14;

  t_fine = var1 + var2;

  float T = (t_fine * 5 + 128) >> 8;
  return T / 100;
}
*/
static int readCoefficient(unsigned char registro)
{
    char reg[1] = {0};
    char data[2] = {0};
    int valor, ret;
    reg[0] = registro;            
    ret = I2C_Write(reg, 1);                    
    ret = I2C_Read(data, 2);                    
    valor = (data[1]<<8) | (data[0]);
    if (valor > 0x7FFF){ valor -= 0x10000;}
    pr_info("BMP280: DIG_0x%X %d\n", registro, valor);
 
    return valor;
}
/*
static int readADC(unsigned char registro)
{
    char reg[1] = {0};
    char data[3] = {0};
    float valor;
    int ret;
    reg[0] = registro;            
    ret = I2C_Write(reg, 1);                    
    ret = I2C_Read(data, 3);                    
    valor = (((data[0]<<8) | (data[1]))<<4) | ((data[2] >> 4) & 0x0F);
    if (valor > 0x7FFF){ valor -= 0x10000;}
    pr_info("BMP280: ADC_0x%X %d\n", registro, valor);
 
    return valor;
}
*/
/*
//This function getting called when the slave has been found
//Note : This will be called only once when we load the driver.
*/
static int etx_oled_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    int ret;
    char reg[1] = {0};
    char data[2] = {0};
    char config[2] = {0};

    config[0] = BMP280_REGISTER_SOFTRESET;      //Le escribo al registro de reset
	config[1] = MODE_SOFT_RESET_CODE;           //Le escribo el comando RST
    ret = I2C_Write(config, 2);
    pr_info("BMP280: write ret %d = 2?\n",ret);

    reg[0] = BMP280_REGISTER_CHIPID;            //Le indico que registro quiero leer
    ret = I2C_Write(reg, 1);                    //Cual es el chip_ID?
    ret = I2C_Read(data, 1);                    //Espero la respuesta
    pr_info("BMP280: chip_ID = 0x58? 0x%X\n",data[0]);

    reg[0] = BMP280_REGISTER_STATUS;            //Le indico que registro quiero leer
    ret = I2C_Write(reg, 1);                    //Status?
    ret = I2C_Read(data, 1);                    //Espero la respuesta
    pr_info("BMP280: Reg de Status 0x%X\n",data[0]); //bit3 si es uno esta conviertiendo, bit 0 si es uno esta copiando a la memoria no volatil
    
    bmp280.dig_T1 = readCoefficient(BMP280_REGISTER_DIG_T1);
    bmp280.dig_T2 = readCoefficient(BMP280_REGISTER_DIG_T2);
    bmp280.dig_T3 = readCoefficient(BMP280_REGISTER_DIG_T3);

    bmp280.dig_P1 = readCoefficient(BMP280_REGISTER_DIG_P1);
    bmp280.dig_P2 = readCoefficient(BMP280_REGISTER_DIG_P2);
    bmp280.dig_P3 = readCoefficient(BMP280_REGISTER_DIG_P3);
    bmp280.dig_P4 = readCoefficient(BMP280_REGISTER_DIG_P4);
    bmp280.dig_P5 = readCoefficient(BMP280_REGISTER_DIG_P5);
    bmp280.dig_P6 = readCoefficient(BMP280_REGISTER_DIG_P6);
    bmp280.dig_P7 = readCoefficient(BMP280_REGISTER_DIG_P7);
    bmp280.dig_P8 = readCoefficient(BMP280_REGISTER_DIG_P8);
    bmp280.dig_P9 = readCoefficient(BMP280_REGISTER_DIG_P9);

    reg[0] = 0x88;
    ret = I2C_Write(reg, 1);
    ret = I2C_Read(data, 24);
    pr_info("BMP280: read ret %d = 24?\n",ret);

    config[0] = 0xF4;
	config[1] = 0x27;
    ret = I2C_Write(config, 2);
    //pr_info("BMP280: write ret %d\n",ret);
    config[0] = 0xF5;
	config[1] = 0xA0;
    ret = I2C_Write(config, 2);
    //pr_info("BMP280: write ret %d\n",ret);

    reg[0] = 0xF7;
    ret = I2C_Write(reg, 1);
    //pr_info("BMP280: write ret %d\n",ret);

    ret = I2C_Read(data, 8);
    pr_info("BMP280: read ret %d = 8?\n",ret);

    /*
    for(i=0; i< 8; i++){
        pr_info("BMP280: data[%d] %d\n",i,data[i]);
    }
    */

    return 0;
}
 
/*
//This function getting called when the slave has been removed
//Note : This will be called only once when we unload the driver.
*/
static int etx_oled_remove(struct i2c_client *client)
{     
    int ret;
    char config[2] = {0};;

    config[0] = BMP280_REGISTER_SOFTRESET;      //Le escribo al registro de reset
	config[1] = MODE_SOFT_RESET_CODE;           //Le escribo el comando RST
    ret = I2C_Write(config, 2);

    pr_info("BMP280: Reset and Remove\n");
    return 0;
}
 
/*
//Structure that has slave device id
*/
static const struct i2c_device_id etx_oled_id[] = {
        { SLAVE_DEVICE_NAME, 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, etx_oled_id);
 
/*
//I2C driver Structure that has to be added to linux
*/
static struct i2c_driver etx_sensor_driver = {
        .driver = {
            .name   = SLAVE_DEVICE_NAME,
            .owner  = THIS_MODULE,
        },
        .probe          = etx_oled_probe,
        .remove         = etx_oled_remove,
        .id_table       = etx_oled_id,
};
 
/*
//I2C Board Info strucutre
*/
static struct i2c_board_info sensor_i2c_board_info = {
        I2C_BOARD_INFO(SLAVE_DEVICE_NAME, SSD1306_SLAVE_ADDR)
    };
 
/*
//Module Init function
*/
static int __init etx_driver_init(void)
{
    int ret = -1;
    etx_i2c_adapter     = i2c_get_adapter(I2C_BUS_AVAILABLE);
    
    if( etx_i2c_adapter != NULL )
    {
        etx_i2c_client_sensor = i2c_new_device(etx_i2c_adapter, &sensor_i2c_board_info);
        
        if( etx_i2c_client_sensor != NULL )
        {
            i2c_add_driver(&etx_sensor_driver);
            ret = 0;
        }
        
        i2c_put_adapter(etx_i2c_adapter);
    }
    
    pr_info("Driver Added!!!\n");
    return ret;
}
 
/*
//Module Exit function
*/
static void __exit etx_driver_exit(void)
{
    i2c_unregister_device(etx_i2c_client_sensor);
    i2c_del_driver(&etx_sensor_driver);
    pr_info("Driver Removed!!!\n");
}
 
module_init(etx_driver_init);
module_exit(etx_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("Simple I2C driver explanation (SSD_1306 OLED Display Interface)");
MODULE_VERSION("1.34");