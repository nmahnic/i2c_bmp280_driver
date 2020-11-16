#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

// sudo chmod 666 /dev/NM_td3_i2c_dev

int  main(void)
{
	// Create I2C bus
	int file;
	int adapter_nr = 1;
	char filename[20];
	//snprintf(filename, 19, "/dev/NM_td3_i2c_dev");

	if ((file = open("/dev/NM_td3_i2c_dev", O_RDWR)) < 0)
	{
		printf("Failed to open the Chardriver. \n");
		exit(1);
	}
	printf("El fileDescriptor es %d\n", file);
	sleep(5);
	close(file);

	return 0;
}