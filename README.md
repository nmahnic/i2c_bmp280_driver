# i2c_bmp280_driver

## Luego de instalar la imagen del Kernel
Configuración de la BBB para compilación local de modulos de kernel.
```
$ sudo apt update
$ sudo apt upgrade
$ sudo apt install build-essential linux-headers-$(uname -r)
$ sudo ln -s /usr/src/linux-headers-$(uname -r)/ /lib/modules/$(uname -r)/build
```
NOTA: el **upgrade** tarda unos cuantos minutos.

## deviceDriver
Este driver no utiliza la secuencia normal de usos de modulos de kernel. Modifica desde el deviceTree el dispositivo I2C con otra configuracion, de manera que las APIs de kernel que hacen uso del /dev/i2c-2 ya no encuentran al dispositivo.

Se implementa un platform driver donde se manejara el dispositivo I2C modificado en el devTree y se desarrollan systemCall propias para ser usadas por aplicaciones.
estas systemCall no seran genericas para cualquier dispositivo que utilice I2C ya que al platform device se le incluira un char device nativo de la plataforma, de manera que las APIs desarrolladas solo serviran para el sensor **bmp280**

## BMP280 
El bmp280 es un sensor de presion y temperatura desarrollado por Bosch.

Puede operar en 3 modos:
+ sleep mode -> no se realizan mediciones.
+ normal mode -> El modo normal comprende un ciclo perpetuo automatizado entre un período de medición activo y un período de espera inactivo.
+ forced mode -> Se realiza una sola medición y el sensor vuelve a sleep mode.

Dado que se puede cambiar el tiempo de sobremuestreo y la aplicacion de diferentes filtros digitales internos. El fabricante define 6 combinaciones de parámetros de configuración más comunes.
+ Handlheld device low-power
+ Handlheld device dynamic
+ Weather monitoring
+ Elevator / floor change detection
+ Drop detection
+ Indoor navigation