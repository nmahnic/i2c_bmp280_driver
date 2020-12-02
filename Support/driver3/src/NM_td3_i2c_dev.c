#include "../inc/define.h"
#include "../inc/char_dev.h"
#include "../inc/platform.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pablo Vecchio R5054");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Sometimes it seems to me");

static int __init i2c_init(void){
	int status = 0;
	pr_info("________________________________________________________________________\n");
    pr_info("%s: Modulo i2c_td3 para TP n°2 de TD3\n",DEVICE_NAME);
    char_dev.myi2c_cdev = cdev_alloc();    

    if ((status= alloc_chrdev_region(&char_dev.myi2c,MENOR,CANT_DISP,DEVICE_NAME))<0){
        pr_alert("%s: No es posible asignar el número Mayor\n",DEVICE_NAME);
        return status;
    }

    pr_info("%s: Número Mayor asignado %d \n",DEVICE_NAME,MAJOR(char_dev.myi2c));
    cdev_init(char_dev.myi2c_cdev,&i2c_ops);

    if((status = cdev_add(char_dev.myi2c_cdev,char_dev.myi2c,CANT_DISP)) < 0){
        unregister_chrdev_region(char_dev.myi2c,CANT_DISP);
        pr_alert("%s: No es posible registrar el dispositivo\n",DEVICE_NAME);
        return status;}

	if ((char_dev.myi2c_class = class_create(THIS_MODULE,CLASS_NAME)) == NULL){
		pr_alert("No se puede crear la Clase para el dispositivo %s\n",DEVICE_NAME);
		unregister_chrdev_region(char_dev.myi2c,CANT_DISP);
		return EFAULT;}
	
	char_dev.myi2c_class->dev_uevent = cambio_permisos;
	
	if ((device_create (char_dev.myi2c_class, NULL, char_dev.myi2c,NULL, DEVICE_NAME)) == NULL){
		pr_alert("No se puede crear el dispositivo %s\n",DEVICE_NAME);
		class_destroy(char_dev.myi2c_class);
		unregister_chrdev_region(char_dev.myi2c,CANT_DISP);
		return EFAULT;}

	status = platform_driver_register(&i2c_pd);

	if((status) <0){
		pr_alert("%s: No se pudo registrar el platform Device\n", DEVICE_NAME);
		return status;}

	pr_info("%s: Todo Inicializado!\n", DEVICE_NAME);
	pr_info("________________________________________________________________________\n");
	return 0;
}

static void __exit i2c_exit(void){
	cdev_del(char_dev.myi2c_cdev);
	device_destroy(char_dev.myi2c_class, char_dev.myi2c);
	class_destroy(char_dev.myi2c_class);
    unregister_chrdev_region(char_dev.myi2c,CANT_DISP);
	platform_driver_unregister(&i2c_pd);
	printk(KERN_INFO "%s: Chau!\n", DEVICE_NAME);
	pr_info("________________________________________________________________________\n");
}


static int cambio_permisos(struct device *dev, struct kobj_uevent_env *env){
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

module_init(i2c_init);
module_exit(i2c_exit);
