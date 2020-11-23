#define    SAMPLING_NONE        0x00
#define    SAMPLING_X1          0x01
#define    SAMPLING_X2          0x02
#define    SAMPLING_X4          0x03
#define    SAMPLING_X8          0x04
#define    SAMPLING_X16         0x05

#define    MODE_SLEEP           0x00
#define    MODE_FORCED          0x01
#define    MODE_NORMAL          0x03
#define    MODE_SOFT_RESET_CODE 0xB6

#define    FILTER_OFF           0x00
#define    FILTER_X2            0x01
#define    FILTER_X4            0x02
#define    FILTER_X8            0x03
#define    FILTER_X16           0x04

#define    STANDBY_MS_1         0x00
#define    STANDBY_MS_62_5      0x01
#define    STANDBY_MS_125       0x02
#define    STANDBY_MS_250       0x03
#define    STANDBY_MS_500       0x04
#define    STANDBY_MS_1000      0x05
#define    STANDBY_MS_2000      0x06
#define    STANDBY_MS_4000      0x07

#define    BMP280_CHIPID        0x58 /**< Default chip ID. */
#define    BMP280_ADDRESS       0x77 /**< The default I2C address for the sensor. */
#define    BMP280_ADDRESS_ALT   0x76 /**< Alternative I2C address for the sensor. */

#define MENOR          0
#define CANT_DISP      1
#define DEVICE_NAME    "NM_td3_i2c_dev"
#define CLASS_NAME     "NM_td3_i2c_class"
#define COMPATIBLE	   "NM_td3_i2c_dev"

enum {
  BMP280_REGISTER_DIG_T1 = 0x88,
  BMP280_REGISTER_DIG_T2 = 0x8A,
  BMP280_REGISTER_DIG_T3 = 0x8C,
  BMP280_REGISTER_DIG_P1 = 0x8E,
  BMP280_REGISTER_DIG_P2 = 0x90,
  BMP280_REGISTER_DIG_P3 = 0x92,
  BMP280_REGISTER_DIG_P4 = 0x94,
  BMP280_REGISTER_DIG_P5 = 0x96,
  BMP280_REGISTER_DIG_P6 = 0x98,
  BMP280_REGISTER_DIG_P7 = 0x9A,
  BMP280_REGISTER_DIG_P8 = 0x9C,
  BMP280_REGISTER_DIG_P9 = 0x9E,
  BMP280_REGISTER_CHIPID = 0xD0,
  BMP280_REGISTER_VERSION = 0xD1,
  BMP280_REGISTER_SOFTRESET = 0xE0,
  BMP280_REGISTER_CAL26 = 0xE1, /**< R calibration = 0xE1-0xF0 */
  BMP280_REGISTER_STATUS = 0xF3,
  BMP280_REGISTER_CONTROL = 0xF4,
  BMP280_REGISTER_CONFIG = 0xF5,
  BMP280_REGISTER_PRESSUREDATA = 0xF7,
  BMP280_REGISTER_TEMPDATA = 0xFA,
};

struct sensor {
  char dig_T1;
  char dig_T2;
  char dig_T3;
  char dig_P1;
  char dig_P2;
  char dig_P3;
  char dig_P4;
  char dig_P5;
  char dig_P6;
  char dig_P7;
  char dig_P8;
  char dig_P9;
}; 

static int change_permission_cdev(struct device *dev, struct kobj_uevent_env *env);
static int NMopen(struct inode *inode, struct file *file);
static int NMrelease(struct inode *inode, struct file *file);
static ssize_t NMread (struct file * device_descriptor, char __user * user_buffer, size_t read_len, loff_t * my_loff_t);
static ssize_t NMwrite (struct file * device_descriptor, const char __user * user_buffer, size_t write_len, loff_t * my_loff_t);

struct file_operations i2c_ops = {
	.owner = THIS_MODULE,
	.open = NMopen,
	.read = NMread,
	.write = NMwrite,
	.release = NMrelease,
	//.ioctl = NMioctl,
};

static struct {
	int pos_rx;
	int buff_rx_len;
	int data_rx;

	int pos_tx;
	int buff_tx_len;
	int data_tx;

	char * buff_rx;
	char * buff_tx;
} data_i2c;

static struct {
	dev_t myi2c;

	struct cdev * myi2c_cdev;
	struct device * myi2c_device;
	struct class * myi2c_class;
} state;