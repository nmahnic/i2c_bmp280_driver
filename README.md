# i2c_bmp280_driver

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

## BMP280 
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