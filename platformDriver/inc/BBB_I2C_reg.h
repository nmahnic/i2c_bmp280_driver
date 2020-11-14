// Clock Module Peripheral (page 179) length = 1K -> 0x400
#define CM_PER                                  0x44E00000 
#define CM_PER_LEN                              0x00000400 // 1K
// I2C2 clocks manager (page 1270)
#define CM_PER_I2C2_CLKCTRL_OFFSET              0x00000044
#define CM_PER_I2C2_CLKCTRL_MASK                0x00030003 
#define CM_PER_I2C2_CLKCTRL_VALUE               0x00000002

// Config de Control Module (page??)
#define CTRL_MODULE_BASE                        0x44E10000
#define CTRL_MODULE_LEN                         0x00002000 // 8K
#define CTRL_MODULE_UART1_CTSN_OFFSET           0x00000978
#define CTRL_MODULE_UART1_RTSN_OFFSET           0x0000097C
#define CTRL_MODULE_UART1_MASK                  0x0000000E
//Habilita capacidad de pines (page 1515) 
//Fast | Reciever Enable | Pullup | Pullup/pulldown disabled | I2C2_SCL
#define CTRL_MODULE_UART1_I2C_VALUE             0x0000003B // 00111011b 


// Registros especificos de I2C
#define I2C_CON_START                           0x00000001 // Page 4636
#define I2C_CON_STOP                            0x00000002 // Page 4636
#define I2C_IRQSTATUS_RRDY                      0x00000008 // Page 4613
#define I2C_IRQSTATUS_XRDY                      0x00000010 // Page 4613
#define I2C_IRQSTATUS_RAW                       0x00000024 // Page 4606
#define I2C_IRQSTATUS                           0x00000028 // Page 4612
#define I2C_IRQENABLE_SET                       0x0000002C // Page 4614
#define I2C_IRQENABLE_CLR                       0x00000030 // Page 4616
#define I2C_CNT                                 0x00000098 // Page 4632
#define I2C_DATA                                0x0000009C // Page 4633
#define I2C_CON                                 0x000000A4 // Page 4634 
#define I2C_OA                                  0x000000A8 // Page 4637
#define I2C_SA                                  0x000000AC // Page 4638
#define I2C_PSC                                 0x000000B0 // Page 4639
#define I2C_SCLL                                0x000000B4 // Page 4640
#define I2C_SCLH                                0x000000B8 // Page 4641