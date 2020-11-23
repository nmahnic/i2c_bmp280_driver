#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#define LARGO 48

// sudo chmod 666 /dev/NM_td3_i2c_dev

int  main(void)
{
	// Create I2C bus
	int file;
	int adapter_nr = 1;
	char filename[20];
	unsigned char data[LARGO];
	int i;
	//snprintf(filename, 19, "/dev/NM_td3_i2c_dev");

	if ((file = open("/dev/NM_td3_i2c_dev", O_RDWR)) < 0)
	{
		printf("Failed to open the Chardriver. \n");
		exit(1);
	}
	printf("El fileDescriptor es %d\n", file);

/*
	read(file, data, LARGO);
	for(i = 0; i < 24; ++i)
        printf("data[%d]=%d \n",i, data[i]);
	printf("\n");
	for( ; i < LARGO; ++i)
        printf("dato[%d]=%d \n",i-24, data[i]);
    printf("\n");


	int dig_T1 = data[1] * 256 + data[0];
	int dig_T2 = data[3] * 256 + data[2];
	if (dig_T2 > 0x7FFF)
	{
		dig_T2 -= 0x10000;
	}
	int dig_T3 = data[5] * 256 + data[4];
	if (dig_T3 > 0x7FFF)
	{
		dig_T3 -= 0x10000;
	}
	// pressure coefficents
	int dig_P1 = data[7] * 256 + data[6];
	int dig_P2 = data[9] * 256 + data[8];
	if (dig_P2 > 0x7FFF)
	{
		dig_P2 -= 0x10000;
	}
	int dig_P3 = data[11] * 256 + data[10];
	if (dig_P3 > 0x7FFF)
	{
		dig_P3 -= 0x10000;
	}
	int dig_P4 = data[13] * 256 + data[12];
	if (dig_P4 > 0x7FFF)
	{
		dig_P4 -= 0x10000;
	}
	int dig_P5 = data[15] * 256 + data[14];
	if (dig_P5 > 0x7FFF)
	{
		dig_P5 -= 0x10000;
	}
	int dig_P6 = data[17] * 256 + data[16];
	if (dig_P6 > 0x7FFF)
	{
		dig_P6 -= 0x10000;
	}
	int dig_P7 = data[19] * 256 + data[18];
	if (dig_P7 > 0x7FFF)
	{
		dig_P7 -= 0x10000;
	}
	int dig_P8 = data[21] * 256 + data[20];
	if (dig_P8 > 0x7FFF)
	{
		dig_P8 -= 0x10000;
	}
	int dig_P9 = data[23] * 256 + data[22];
	if (dig_P9 > 0x7FFF)
	{
		dig_P9 -= 0x10000;
	}


	long adc_p = (((long)data[24] * 0x10000) + ((long)data[25] * 256) + (long)(data[26] & 0xF0)) / 16;
	long adc_t = (((long)data[27] * 0x10000) + ((long)data[28] * 256) + (long)(data[28] & 0xF0)) / 16;
	// Temperature offset calculations
	double var1 = (((double)adc_t) / 16384.0 - ((double)dig_T1) / 1024.0) * ((double)dig_T2);
	double var2 = ((((double)adc_t) / 131072.0 - ((double)dig_T1) / 8192.0) * (((double)adc_t) / 131072.0 - ((double)dig_T1) / 8192.0)) * ((double)dig_T3);
	double t_fine = (long)(var1 + var2);
	double cTemp = (var1 + var2) / 5120.0;
	// Pressure offset calculations
	var1 = ((double)t_fine / 2.0) - 64000.0;
	var2 = var1 * var1 * ((double)dig_P6) / 32768.0;
	var2 = var2 + var1 * ((double)dig_P5) * 2.0;
	var2 = (var2 / 4.0) + (((double)dig_P4) * 0x10000);
	var1 = (((double)dig_P3) * var1 * var1 / 524288.0 + ((double)dig_P2) * var1) / 524288.0;
	var1 = (1.0 + var1 / 32768.0) * ((double)dig_P1);
	double p = 1048576.0 - (double)adc_p;
	p = (p - (var2 / 4096.0)) * 6250.0 / var1;
	var1 = ((double)dig_P9) * p * p / 2147483648.0;
	var2 = p * ((double)dig_P8) / 32768.0;
	double pressure = (p + (var1 + var2 + ((double)dig_P7)) / 16.0) / 100;
	usleep(500000);
	// Output data to screen
	printf("Pressure : %.2f hPa \n", pressure);
	printf("Temperature in Celsius : %.2f C \n", cTemp);
*/

	close(file);

	return 0;
}