## Paso1
Dentro de la carpeta /boot/dtbs/4.19.94-ti-r36/
```
cd /boot/dtbs/4.19.94-ti-r36/
```
## Paso2
Me copio el am335x-boneblack.dtb a una carpeta para decompilarlo
```
sudo cp am335x-boneblack.dtb /home/ubuntu/i2c_bmp280_driver/deviceTree/
```
## Paso3
Me voy a la carpeta i2c_bmp280/deviceTree/
```
cd
cd i2c_bmp280_driver/deviceTree
```
## Paso4
Decompilo
```
dtc -I dtb -O dts -o am335x.dts am335x-boneblack.dtb

```
## Paso 5
Modifico el .dts
```
/ {
	compatible = "ti,am335x-bone-black", "ti,am335x-bone", "ti,am33xx", "NM_td3_i2c_dev";
	interrupt-parent = < 0x01 >;
	#address-cells = < 0x01 >;
	#size-cells = < 0x01 >;
	model = "TI AM335x BeagleBone Black";
    ...
    ...
```
```
        i2c_td3@4819c000 {
            compatible = "NM_td3_i2c_dev";
            #address-cells = < 0x01 >;
            #size-cells = < 0x00 >;
            ti,hwmods = "i2c3";
            reg = < 0x4819c000 0x1000 >;
            interrupts = < 0x1e >;
            status = "okay";
            pinctrl-names = "default";
            pinctrl-0;
            clock-frequency = < 0x186a0 >;
            phandle = < 0xae >;
            ...
            ...
```
## Paso6
Compilo el .dts y lo copio al /boot/dtbs/4.19.94-ti-r36/
```
dtc -I dts -O dtb -o am335x-boneblack-td3.dtb am335x.dts
sudo cp am335x-boneblack-td3.dtb /boot/dtbs/4.19.94-ti-r36/
cd /boot/dtbs/4.19.94-ti-r36/
ll | grep am335x-boneblack-td3
```
## Paso7
Edito el archivo uEnv.txt
```
cd /boot/
sudo nano uEnv.txt
```
Descomentar la linea dtb
Agregar el .dtb que este en la carpeta /boot/dtbs/4.19.94-ti-r36/
```
dtb=am335x-boneblack-td3.dtb
```
## Paso8
Reinicio
```
sudo reboot
```
## Paso9
```
cd /proc/device-tree/ocp
ll | grep -i i2c_td3@4819c000
cd i2c_td3@4819c000/
cat name
cat compatible
```