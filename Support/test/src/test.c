#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#define CALIBRACION 	24
#define MEDICION		8

// sudo chmod 666 /dev/NM_td3_i2c_dev

int  main(void)
{
	int dispositivo;
    char aux;
    int i;
    char data[24];
    char Med[6];

    dispositivo = open("/dev/i2c_td3", O_RDWR);
    if(dispositivo < 0){
      printf("Falla al abrir dispositivo i2c\n");
      exit(0);
    }

/*
    aux = 0xF5;
    write(dispositivo, &aux,1);
    aux = 0xA0;
    write(dispositivo, &aux,1);

    aux = 0xF4;
    write(dispositivo, &aux,1);
    aux = 0x25;
    write(dispositivo, &aux,1);
*/

//    sleep(10/1000);

    aux = 0xE0;
    write(dispositivo, &aux,1);
    aux = 0xB6;
    write(dispositivo, &aux,1);

    sleep(10/1000);

    aux = 0xD0;
    write(dispositivo,&aux,1);
    read(dispositivo,&data[0],1);

    printf("CHIP_ID 0x%X\n", data[0]);

    aux = 0x88;
    write(dispositivo,&aux,1);
    read(dispositivo,&data[0],24);

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

    
    for(i=0;i<24;i++){
        printf("Calib%02d 0x%02X\n",i, data[i]);
    }

    aux = 0xF5;
    write(dispositivo, &aux,1);
    aux = 0x00;
    write(dispositivo, &aux,1);

    aux = 0xF4;
    write(dispositivo, &aux,1);
    aux = 0x00;
    write(dispositivo, &aux,1);

    aux = 0xF7;
    write(dispositivo,&aux,1);
    read(dispositivo,&data[0],6);


    long adc_p = (((long)data[0] * 65536) + ((long)data[1] * 256) + (long)(data[2] & 0xF0)) / 16;
	long adc_t = (((long)data[3] * 65536) + ((long)data[4] * 256) + (long)(data[5] & 0xF0)) / 16;
	// Temperatura
	double var1 = (((double)adc_t) / 16384.0 - ((double)dig_T1) / 1024.0) * ((double)dig_T2);
	double var2 = ((((double)adc_t) / 131072.0 - ((double)dig_T1) / 8192.0) *(((double)adc_t)/131072.0 - ((double)dig_T1)/8192.0)) * ((double)dig_T3);
	double t_fine = (long)(var1 + var2);
	float temp = (var1 + var2) / 5120.0;
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
	float pres = (p + (var1 + var2 + ((double)dig_P7)) / 16.0) / 100;

    
    for(i=0;i<6;i++){
        printf("T%02d 0x%02X\n",i, data[i]);
    }
    

    printf("temp %.2f\n", temp);
    printf("pres %.2f\n", pres);

    close(dispositivo);

	return 0;
}

