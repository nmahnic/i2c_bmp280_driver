#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define CALIBRACION 	24
#define MEDICION		8

#define I2C_SLAVE					0x0703

// sudo chmod 666 /dev/NM_td3_i2c_dev

int  main(void)
{
        int file,i;
        char * bus = "/dev/NM_td3_i2c_dev";
        file = open(bus, O_RDWR);

        // Configurar el device I2C
        ioctl(file, I2C_SLAVE, 0x76);

        // Leyendo 24 bytes desde (0x88)
        char reg[1] = {0x88};
        write(file, reg, 1);
        char data[24] = {0};
        read(file, data, 24);
        int dig_T1 = data[1] * 256 + data[0];
        int dig_T2 = data[3] * 256 + data[2];
        if(dig_T2 > 32767){
            dig_T2 -= 65536;}
        int dig_T3 = data[5] * 256 + data[4];
        if(dig_T3 > 32767){
            dig_T3 -= 65536;}

        // Coeficientes de presión
        int dig_P1 = data[7] * 256 + data[6];
        int dig_P2  = data[9] * 256 + data[8];
        if(dig_P2 > 32767){
            dig_P2 -= 65536;}
        int dig_P3 = data[11]* 256 + data[10];
        if(dig_P3 > 32767){
            dig_P3 -= 65536;}
        int dig_P4 = data[13]* 256 + data[12];
        if(dig_P4 > 32767){
            dig_P4 -= 65536;}
        int dig_P5 = data[15]* 256 + data[14];
        if(dig_P5 > 32767){
            dig_P5 -= 65536;}
        int dig_P6 = data[17]* 256 + data[16];
        if(dig_P6 > 32767){
            dig_P6 -= 65536;}
        int dig_P7 = data[19]* 256 + data[18];
        if(dig_P7 > 32767){
            dig_P7 -= 65536;}
        int dig_P8 = data[21]* 256 + data[20];
        if(dig_P8 > 32767){
            dig_P8 -= 65536;}
        int dig_P9 = data[23]* 256 + data[22];
        if(dig_P9 > 32767){
            dig_P9 -= 65536;}
        //for (i=0;i<24;i++)
        //    printf("DATO[%d] = 0x%X\n",i,data[i]);
        //printf("\n\n\n");
        char config[2] = {0};
        config[0] = 0xF4;
        config[1] = 0x27;
        write(file, config, 2);
        
        
        config[0] = 0xF5;
        config[1] = 0xA0;
        write(file, config, 2);
        
        reg[0] = 0xF7;
        write(file, reg, 1);
        read(file, data, 8);
        
        // Conversión a 19-bits
        long adc_p = (((long)data[0] * 65536) + ((long)data[1] * 256) + (long)(data[2] & 0xF0)) / 16;
        long adc_t = (((long)data[3] * 65536) + ((long)data[4] * 256) + (long)(data[5] & 0xF0)) / 16;
            
        // Temperatura
        double var1 = (((double)adc_t) / 16384.0 - ((double)dig_T1) / 1024.0) * ((double)dig_T2);
        double var2 = ((((double)adc_t) / 131072.0 - ((double)dig_T1) / 8192.0) *(((double)adc_t)/131072.0 - ((double)dig_T1)/8192.0)) * ((double)dig_T3);
        double t_fine = (long)(var1 + var2);
        double TEMP = (var1 + var2) / 5120.0;
        
        // Presión
        var1 = ((double)t_fine / 2.0) - 64000.0;
        var2 = var1 * var1 * ((double)dig_P6) / 32768.0;
        var2 = var2 + var1 * ((double)dig_P5) * 2.0;
        var2 = (var2 / 4.0) + (((double)dig_P4) * 65536.0);
        var1 = (((double) dig_P3) * var1 * var1 / 524288.0 + ((double) dig_P2) * var1) / 524288.0;
        var1 = (1.0 + var1 / 32768.0) * ((double)dig_P1);
        double p = 1048576.0 - (double)adc_p;
        p = (p - (var2 / 4096.0)) * 6250.0 / var1;
        var1 = ((double) dig_P9) * p * p / 2147483648.0;
        var2 = p * ((double) dig_P8) / 32768.0;
        double PRESION = (p + (var1 + var2 + ((double)dig_P7)) / 16.0) / 100;

        //for (i=0;i<24;i++)
        //    printf("DATO[%d] = 0x%X\n",i,data[i]);
        printf("TEMP= %f           PRESION=%f\n\n",TEMP,PRESION);
        sleep(2);
        close(file);

	return 0;
}