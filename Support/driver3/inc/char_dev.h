#include "define.h"

/*
    Funciones
*/

static int myopen (struct inode *inode, struct file *file){

    //Deshabilito I2C2
    iowrite32(I2C_CON_DISABLE, td3_i2c_base + I2C_CON);

	//Configuro Prescaler
    iowrite32(I2C_PSC_CONF, td3_i2c_base + I2C_PSC);

    //Configuro tiempos
    iowrite32(I2C_SCLL_CONF, td3_i2c_base + I2C_SCLL);
    iowrite32(I2C_SCLH_CONF, td3_i2c_base + I2C_SCLH);

    //Owner Adress
    iowrite32(I2C_OA_CONF, td3_i2c_base + I2C_OA);

	//Forzamos idle, no wake-up
    iowrite32(0, td3_i2c_base + I2C_SYSC);

    //Habilito I2C2
    iowrite32(I2C_CON_ENABLE, td3_i2c_base + I2C_CON);

	return 0;
}

static long myioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    if (cmd==I2C_SLAVE){
        iowrite32(I2C_CON_DISABLE, td3_i2c_base + I2C_CON);
        iowrite32(arg, td3_i2c_base + I2C_SA);
        iowrite32(I2C_CON_ENABLE, td3_i2c_base + I2C_CON);
    }
	return 0;
}

static int myread (struct file *filp, char *buffer, size_t len, loff_t *f_pos){
    if (!access_ok(VERIFY_WRITE, buffer, len))
        return -EINVAL;
    
    uint32_t status = 0, i = 0, j = 0, reg_value = 0;
    unsigned char data[24] = {0};
   
    //Ocupado?
    reg_value = ioread32(td3_i2c_base + I2C_IRQSTATUS_RAW);
    while((reg_value >> 12) & 1)
    {
        msleep(100);
        pr_err("%s: Dispositivo ocupado momentáneo\n",DEVICE_NAME);
        i++;
        if(i >= 5)
        {
            pr_err("%s: Dispositivo ocupado indefinidamente. CHAU!\n",DEVICE_NAME);
            return -EINVAL;}
    }

    //Tamaño de la lectura 
    iowrite32(len, td3_i2c_base + I2C_CNT);

    //Habilitamos la interrupción de la lectura
    iowrite32(I2C_IRQSTATUS_RRDY, td3_i2c_base + I2C_IRQENABLE_SET);

	//Habilito el IC2-2, Master, Lectura con bit de Inicio y Parada
    iowrite32(I2C_CON_EN_MA_RX_STT_STP, td3_i2c_base + I2C_CON);

    msleep(10);
    
    j = len-1;
    for(i=0; i<len; i++)
    {
        data[j] = datos[i];
        j--;}
		
    for(i=0; i<24; i++)
		datos[i] = 0;

    status = copy_to_user((void *) buffer, (void *) data, sizeof(data));
    if(status != 0)
    {
        pr_err("Error al copiar en el buffer del usuario de %s\n",DEVICE_NAME);
        return -1;
    }  

    //Cantidad en cero
    cant = 0;

    //Espero que termine la lectura
    if((status = wait_event_interruptible(wq, condicion > 0)) < 0)
    {
        condicion = 0;
        pr_err("Error al leer en %s\n",DEVICE_NAME);
        return status;}

    condicion = 0;

    return len;
}

static int mywrite (struct file *filp, const char *buffer, size_t len, loff_t *f_pos){
	uint32_t status = 0, i = 0, reg_value = 0;
    char data[2] = {0};

    //Ocupado?
    reg_value = ioread32(td3_i2c_base + I2C_IRQSTATUS_RAW);

    while((reg_value >> 12) & 1)
    {
        msleep(100);
        pr_err("%s: Dispositivo ocupado momentáneo\n",DEVICE_NAME);
        i++;
        if(i == 4){
            pr_err("%s: Dispositivo ocupado indefinidamente\n",DEVICE_NAME);         
            return -1;}
    }

    //Me traigo los datos para escribir
    status = copy_from_user((void *) data, (void *) buffer, sizeof(data));
    if(status != 0){
        pr_err("No se puede leer el buffer del usuario para %s\n",DEVICE_NAME);
        return -1;}
    
    //borro buffer
    for(i=0; i<2; i++)
        datos_tx[i] = 0;

    //Grabo
    for(i=0; i<len; i++)
        datos_tx[i] = data[i];

    //How much?
    iowrite32(len, td3_i2c_base + I2C_CNT);

    //Habilito la interrupción
    iowrite32(I2C_IRQSTATUS_XRDY, td3_i2c_base + I2C_IRQENABLE_SET);

    //Configure register -> ENABLE & MASTER & TX & STOP
    iowrite32(I2C_CON_EN_MA_TX_STT_STP, td3_i2c_base + I2C_CON);

    msleep(10);

    //Cantidad en cero
    cant = 0;

    //Esperanza!!
    if((status = wait_event_interruptible(wq, condicion > 0)) < 0){
        condicion = 0;
        pr_err("No se pudo escribir para %s!! Chau!\n",DEVICE_NAME);
        return -1;}

    condicion = 0;
 
           
    return len;

	return 0;
}

static int myclose (struct inode *inode, struct file *file){

        //limpieza
        iowrite32(I2C_CON_DISABLE, td3_i2c_base + I2C_CON);
        iowrite32(I2C_CON_DISABLE, td3_i2c_base + I2C_PSC);
        iowrite32(I2C_CON_DISABLE, td3_i2c_base + I2C_SCLL);
        iowrite32(I2C_CON_DISABLE, td3_i2c_base + I2C_SCLH);
        iowrite32(I2C_CON_DISABLE, td3_i2c_base + I2C_OA);
	
	return 0;
}
