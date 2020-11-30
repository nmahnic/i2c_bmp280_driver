#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#define CALIBRACION 	24
#define MEDICION		8

// sudo chmod 666 /dev/NM_td3_i2c_dev

int  main(void)
{
	int file;
	int i;
	char *bus = "/dev/Temperatura_I2C";
	char reg[1] = {0};
	char data[24] = {0};
	if((file = open(bus, O_RDWR)) < 0) 
	{
		printf("Driver I2C: Error al abrir el bus. \n");
		exit(1);
	}

	// Leo 1 byte de chequeo del CHIP ID
	reg[0] = 0xD0;
	printf("Escritura 1: 0x%X envio: %d bytes \n", reg[0],write(file, reg, 1));

	read(file, data, 1);
	printf("Lectura 1: 0x%X == 0x58 \n", data[0]);

	close(file);

	return 0;
}

/*
	if(read(file, data, 1) != 1)
	{
		printf("Driver I2C: Error en el read \n");
		exit(1);
	}
	printf("Lectura 1: 0x%X == 0x58 \n", data[0]);

	// Leo 24 bytes de datos de la direccion (0x88)
	reg[0] = 0x88;
	printf("Escritura 1: 0x%X envio: %d bytes \n", reg[0],write(file, reg, 1));

	if(read(file, data, 24) != 24)
	{
		printf("Driver I2C: Error en el read \n");
		exit(1);
	}

	//for(i=0;i<24;i++){
	//	printf("Lectura %d: 0x%X \n",i, reg[i]);
	//}
	
	// Convierto los valores de data a coeficientes int de temperatura
	int dig_T1 = data[1] * 256 + data[0];
	int dig_T2 = data[3] * 256 + data[2];
	if(dig_T2 > 32767)
	{
		dig_T2 -= 65536;
	}
	int dig_T3 = data[5] * 256 + data[4];
	if(dig_T3 > 32767)
	{
		dig_T3 -= 65536;
	}
	// Coeficientes de presion
	int dig_P1 = data[7] * 256 + data[6];
	int dig_P2  = data[9] * 256 + data[8];
	if(dig_P2 > 32767)
	{
		dig_P2 -= 65536;
	}
	int dig_P3 = data[11]* 256 + data[10];
	if(dig_P3 > 32767)
	{
		dig_P3 -= 65536;
	}
	int dig_P4 = data[13]* 256 + data[12];
	if(dig_P4 > 32767)
	{
		dig_P4 -= 65536;
	}
	int dig_P5 = data[15]* 256 + data[14];
	if(dig_P5 > 32767)
	{
		dig_P5 -= 65536;
	}
	int dig_P6 = data[17]* 256 + data[16];
	if(dig_P6 > 32767)
	{
		dig_P6 -= 65536;
	}
	int dig_P7 = data[19]* 256 + data[18];
	if(dig_P7 > 32767)
	{
		dig_P7 -= 65536;
	}
	int dig_P8 = data[21]* 256 + data[20];
	if(dig_P8 > 32767)
	{
		dig_P8 -= 65536;
	}
	int dig_P9 = data[23]* 256 + data[22];
	if(dig_P9 > 32767)
	{
		dig_P9 -= 65536;
	}
 
	// Selecciono el registro de control para mediciones (0xF4)
	// Opciones; - Normal mode, temp and pressure over sampling rate = 1(0x27)
	char config[2] = {0};
	config[0] = 0xF4;
	config[1] = 0x27;
	write(file, config, 2);

	// Selecciono el registro de control para poner el tiempo entre mediciones (0xF5)
	// Stand_by time = 1000 ms(0xA0)
	config[0] = 0xF5;
	config[1] = 0xA0;
	write(file, config, 2);

	sleep(1);
 
	// Leo 8 bytes de datos de la direccion (0xF7)
	// pressure msb1, pressure msb, pressure lsb, temp msb1, temp msb, temp lsb, humidity lsb, humidity msb
	reg[0] = 0xF7;
	write(file, reg, 1);

  	if(read(file, data, 8) != 8)
	{
		printf("Driver I2C: Error en el read \n");
		exit(1);
	}
    
	// Convierto la presion y temperatura a 19 bits
	long adc_p = (((long)data[0] * 65536) + ((long)data[1] * 256) + (long)(data[2] & 0xF0)) / 16;
	long adc_t = (((long)data[3] * 65536) + ((long)data[4] * 256) + (long)(data[5] & 0xF0)) / 16;
 
	// Calculo de offset de la temperatura (del manual)
	double var1 = (((double)adc_t) / 16384.0 - ((double)dig_T1) / 1024.0) * ((double)dig_T2);
	double var2 = ((((double)adc_t) / 131072.0 - ((double)dig_T1) / 8192.0) *(((double)adc_t)/131072.0 - ((double)dig_T1)/8192.0)) * ((double)dig_T3);
	double t_fine = (long)(var1 + var2);
	double cTemp = (var1 + var2) / 5120.0;
	double fTemp = cTemp * 1.8 + 32;
 
	// Calculo de offset de la presion (del manual)
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
	double pressure = (p + (var1 + var2 + ((double)dig_P7)) / 16.0) / 100;
 
  // Cierro el archivo
  	close(file);

	printf("Pressure : %.2f hPa \n", pressure);
	printf("Temperature in Celsius : %.2f C \n", cTemp);

	return 0;
}
*/