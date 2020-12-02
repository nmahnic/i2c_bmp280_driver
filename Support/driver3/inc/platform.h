#include "define.h"

/* 
    Variables
*/

static const struct of_device_id i2c_of_device_ids [] = {
	{ .compatible = COMPATIBLE },
    { },
};

MODULE_DEVICE_TABLE(of, i2c_of_device_ids);
static struct platform_driver i2c_pd = {
	.probe = i2c_probe,
	.remove = i2c_remove,
	.driver = {
		.name = COMPATIBLE,
		.of_match_table = of_match_ptr(i2c_of_device_ids)
	},
};



/* 
Funciones
*/


static int i2c_probe(struct platform_device * i2c_pd) {
	uint32_t reg;

	td3_i2c_base = of_iomap(i2c_pd->dev.of_node, 0);
	
	// Mapeo el registro CM_PER
	if((cm_per_base = ioremap(CM_PER_BASE, CM_PER_LENGTH)) == NULL)	{
		 printk(KERN_ERR "I2C probe: no se pudo remapear en CM_PER\n");
		 iounmap(td3_i2c_base);
		 return 1;}
	
    reg = ioread32(cm_per_base + CM_PER_I2C2_CLKCTRL);
    reg |= MODULEMODE; //MODULEMODE valor para que quede siempre habilitado
    iowrite32(reg, cm_per_base + CM_PER_I2C2_CLKCTRL);//Modulo habilitado
	
	msleep(10);// Esperamos 10 ms ....

	// Mapeo el registro CONTROL MODULE
	if((control_module_base = ioremap(CONTROL_MODULE_BASE, CONTROL_MODULE_LENGTH)) == NULL) {
		 printk(KERN_ERR "I2C probe: no se pudo remapear en CONTROL MODULE\n");
		 iounmap(td3_i2c_base);
		 iounmap(cm_per_base);
		 return 1;}	
	
	virq = platform_get_irq(i2c_pd, 0);// Obtengo una IRQ virtual
	
	if(virq < 0) {
		printk(KERN_ERR "I2C probe: No se pudo obtener el número de irq\n");
		iounmap(td3_i2c_base);
 		iounmap(cm_per_base);
 		iounmap(control_module_base);
		return 1;}

	// Lo asigno a mi handler
	if(request_irq(virq, (irq_handler_t) i2c_irq_handler, IRQF_TRIGGER_RISING, COMPATIBLE, NULL)) {
		printk(KERN_ERR "I2C probe: No se puede bindiar la irq con el handler\n");
		iounmap(td3_i2c_base);
		iounmap(cm_per_base);
		iounmap(control_module_base);
		return 1;}

	pr_info("%s: PROBE Pasado!\n", DEVICE_NAME);
	
	return 0;
}

static int i2c_remove(struct platform_device * i2c_pd) {
	free_irq(virq, NULL);
	iounmap(td3_i2c_base);
	iounmap(cm_per_base);
	iounmap(control_module_base);
	pr_info("________________________________________________________________________\n");
    pr_alert("%s: Todo removido!\n", DEVICE_NAME);
	return 0;
}

irqreturn_t i2c_irq_handler(int irq, void *dev_id, struct pt_regs *regs) {
    uint32_t irq_status;
    uint32_t i = 0, j = 0;

    irq_status = ioread32(td3_i2c_base + I2C_IRQSTATUS); //Chequeo el estado de la IRQ

    if(irq_status & I2C_IRQSTATUS_RRDY)
    {
        i = ioread32(td3_i2c_base + I2C_CNT);
        datos[i] = ioread32(td3_i2c_base + I2C_DATA); //leo
        condicion = 1;
        wake_up_interruptible(&wq);}

    if(irq_status & I2C_IRQSTATUS_XRDY)
    {
        j = cant;
        iowrite32(datos_tx[j], td3_i2c_base + I2C_DATA); //Escribo
        cant++;
		condicion = 1;
        wake_up_interruptible(&wq);}

    irq_status = ioread32(td3_i2c_base + I2C_IRQSTATUS);
    irq_status |= IRQ_BORRA_FLAG;
    iowrite32(irq_status, td3_i2c_base + I2C_IRQSTATUS);//borro el flag
	
	return IRQ_HANDLED;
}

