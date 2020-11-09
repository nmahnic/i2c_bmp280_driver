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

static struct sensor {
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