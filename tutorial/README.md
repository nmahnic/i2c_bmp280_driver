NOTAS
Configuración de la BBB para compilación local
Para configurar la BBB para compilar en forma local en caso de tener que regenerar la imagen.

```
$ sudo apt update
$ sudo apt upgrade
$ sudo apt install build-essential linux-headers-$(uname -r)
$ sudo ln -s /usr/src/linux-headers-$(uname -r)/ /lib/modules/$(uname -r)/build
```

Como ver mi sensor con el Device Tree original
Sensor BMP280, para conocer sus direccion:
```
sudo i2cdetect -y -r 2
```

https://embetronicx.com/linux-device-driver-tutorials/
BMP280 puede operar en 3 modos:
+sleep mode -> no se realizan mediciones.
+normal mode -> El modo normal comprende un ciclo perpetuo automatizado entre un período de medición activo y un período de espera inactivo.
+forced mode -> Se realiza una sola medición y el sensor vuelve a sleep mode.

Tambien permite cambiar el tiempo de sobremuestreo entre 0 y 16. El fabricante define 6 modos mas comunes.
+Handlheld device low-power
+Handlheld device dynamic
+Weather monitoring
+Elevator / floor change detection
+Drop detection
+Indoor navigation

Si se usa como sensor de temperatura se aconseja apagar el sensor de presion, esto le da un bit mas de resolucion en el registro 0xF9. Lo mismo pero al reves ocurre si se usa unicamente como sensor de presion encontrandose el bit adicional de resolucion en el registro 0xFC.

Para habilitar el skipped de alguna variable usar registro de config 0x74.

bits [1:0] del 0xF4 (reg de configuracion) definen el modo
00 sleep mode
01 forced mode
10 forced mode
11 normal mode

bits [2:0] del 0xF5 (reg de configuracion) definen el modo
se determina el tiempo del standby del modo normal

Es recomendado realizar una leida de registros en rafaga (burst) y no leerlos de a uno.
registros de lectura 0xF7 al 0xFC.
Los datos estan en 20 bits para cada bariable

se puede leer de una memoria no volatin los valores de los registros 0x88 al 0xA1
las  palabras son de 16 bits, pero usan dos registros de 8, entonces dig_T1 esta en 0x88 LSB y en 0x89 el MSB, etc. Los mismo sucede para dig_P1 esta en 0x8E LSB y 0x8F el MSB de la presion
Salvo dig_T1 y dig_P1 el resto son short signados (dig_Tx y dig_Px)

Reg 0xF4      osrs_t osrs_p mode            |7|6|5|4|3|2|1|0|
+Handlheld device low-power                 |0|1|0|1|0|1|1|1| 0x57 87  
+Handlheld device dynamic                   |0|0|1|0|1|1|1|1| 0x2F 47
+Weather monitoring                         |0|0|1|0|0|1|0|1| 0x25 37
+Elevator / floor change detection          |0|0|1|0|1|1|1|1| 0x2F 47
+Drop detection                             |0|0|1|0|1|0|1|1| 0x2B 43
+Indoor navigation                          |0|1|0|1|0|1|1|1| 0x57 87

Reg 0xF5      t_sb filter spi3w_en          |7|6|5|4|3|2|1|0|
+Handlheld device low-power                 |0|0|1|0|0|0|0|0| 0x00 00  
+Handlheld device dynamic                   |0|0|0|0|0|0|0|0| 0x00 00  
+Weather monitoring                         |0|0|0|0|0|0|0|0| 0x00 00  
+Elevator / floor change detection          |0|1|0|0|0|0|0|0| 0x00 00  
+Drop detection                             |0|0|0|0|0|0|0|0| 0x00 00  
+Indoor navigation                          |0|0|0|0|0|0|0|0| 0x00 00  

default set
normal, Tsamplingx2, Psamplingx16, filterx16, sb500ms 

## How I2C bus driver works
+ I2C client driver initiates transfer using a function like i2c_transfer, i2c_master_send etc.
+ It comes to the master_xfer function in the bus driver (drivers/i2c/busses/*).
+ The bus driver splits the entire transaction into START, STOP, ADDRESS, READ with ACK, READ with NACK, etc. These conditions have to be created on the real i2c bus. The bus driver writes to the I2C hardware adaptor to generate these conditions on the I2C bus one by one, sleeping on a wait queue in between (basically giving the CPU to some other task to do some useful job rather than polling until hardware finishes).
Once the hardware has finished a transaction on the bus (for eg a START condition), interrupt will be generated and the ISR will wake up the sleeping master_xfer.
+ Once master_xfer wakes up, he will go and advise the hardware adaptor to send the second condition (for eg ADDRESS of the chip).
This continues till whole transactions are over and return back to the client driver.

The point to note here is sleep done by the thread in between each condition. This is why I2C transactions cannot be used in ISRs. For client driver, it is just a simple function like i2c_transfer,i2c_master_send. But it is implemented in the bus driver as explained above.

## INIT
* Malloc cdev
* Dynamically Allocating Major and Minor Number
* Automatically Creating Device File
    + Include the header file linux/device.h and linux/kdev_t.h
    + Create the struct Class
    + Create Device with the class which is created by the above step

## EXIT