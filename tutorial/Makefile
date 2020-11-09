KERNEL_SOURCE = /lib/modules/$(shell uname -r)/build
MOD_NAME = driver

# Converts a module object in LKM
obj-m += src/$(MOD_NAME).o

#compila el modulo haciendo una llamada al Makefile que esta en '/lib/modules/$(shell uname -r)/build'
all: softclean build insmod

build:
	make -C ${KERNEL_SOURCE} M=${PWD} modules

#limpia todos los archivos objetos
softclean:
	make -C ${KERNEL_SOURCE} M=${PWD} clean

clean: rmmod softclean

#revisa si el modulo esta instalado
cat:
	cat /proc/modules | grep $(MOD_NAME)

#instala el modulo
insmod:
	sudo insmod src/$(MOD_NAME).ko

#desinstala el modulo
rmmod:
	sudo rmmod src/$(MOD_NAME).ko

#muestra los mensajes (dmesg), en el ejemplo todos los printk imprimen el nombre del archivo
dmesg:
	dmesg | grep $(MOD_NAME) 

awk:
	cat /proc/devices | grep $(MOD_NAME)

tail:
	tail -f /var/log/syslog

info:
	modinfo src/$(MOD_NAME).ko