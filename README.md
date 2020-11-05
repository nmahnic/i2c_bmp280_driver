# i2c_bmp280_driver

NOTAS
Configuración de la BBB para compilación local
Para configurar la BBB para compilar en forma local en caso de tener que regenerar la imagen.

```
$ sudo apt update
$ sudo apt upgrade
$ sudo apt install build-essential linux-header-$(uname -r)
$ sudo ln -s /usr/src/linux-headers-$(uname -r)/ /lib/modules/$(uname -r)/build
```

Como ver mi sensor con el Device Tree original
Sensor BMP280, para conocer sus direccion:
```
sudo i2cdetect -y -r 2
```
