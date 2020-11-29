/*
 * rda5807.h
 *
 *  Created on: Oct 11, 2020
 *      Author: pawel
 */

#ifndef INC_RDA5807_H_
#define INC_RDA5807_H_

#include "stm32f1xx_hal.h"

#define RDA5807_DIRECT_WRITE_REGISTER 0x10
#define RDA5807_SEQUENCE_WRITE_REGISTER 0x11

typedef enum {
  RDA5807_OK = 0,
  RDA5807_INIT_ERROR = (-1),
  RDA5807_NOT_FOUND = (-2),
  RDA5807_WRITE_ERROR = (-3),
  RDA5807_READ_ERROR = (-4),
} RDA5807_StatusTypeDef;

enum {
  SPACING_100KHZ = 0, SPACING_200KHZ = 1, SPACING_50KHZ = 2, SPACING_25KHZ = 3
};

enum {
  BAND_US_EUROPE = 0, BAND_JAPAN = 1, BAND_WORLD_WIDE = 2, BAND_EAST_EUROPE = 3
};

enum { MONO = 0, STEREO = 1};


// 0x00
typedef union {
  struct {
    uint8_t CHIPID :8; // Chip ifdef
    uint8_t DUMMY :8;
  } refined;
  uint16_t raw;
} rda_reg00;

// 0x01
typedef union {
  struct {
    uint8_t lowByte;
    uint8_t highByte;
  } refined;
  uint16_t raw;
} rda_reg01;

// 0x02
typedef union {
  struct {
    uint8_t ENABLE :1; // Power Up Enable; 0 = Disabled; 1 = Enabled
    uint8_t SOFT_RESET :1; // Soft reset; If 0, not reset; If 1, reset.
    uint8_t NEW_METHOD :1; // New Demodulate Method Enable, can improve 0 the
    // receive sensitivity about 1dB.
    uint8_t RDS_EN :1; // RDS/RBDS enable; If 1, rds/rbds enable
    uint8_t CLK_MODE :3; // See table above
    uint8_t SKMODE :1; // Seek Mode; 0 = wrap at the upper or lower band limit
    // and continue seeking; 1 = stop seeking at the upper
    // or lower band limit
    uint8_t SEEK :1; // Seek; 0 = Disable stop seek; 1 = Enable;
    uint8_t SEEKUP :1; // Seek Up; 0 = Seek down; 1 = Seek up
    uint8_t RCLK_DIRECT_IN :1; // RCLK clock use the directly input mode. 1 =
    // enable
    uint8_t NON_CALIBRATE :1; // 0=RCLK clock is always supply; 1=RCLK clock is
    // not always supply when FM work
    uint8_t BASS :1; // Bass Boost; 0 = Disabled; 1 = Bass boost enabled
    uint8_t MONO :1; // Mono Select; 0 = Stereo; 1 = Force mono
    uint8_t DMUTE :1; // Mute Disable; 0 = Mute; 1 = Normal operation
    uint8_t DHIZ :1; // Audio Output High-Z Disable; 0 = High impedance; 1 =
  // Normal operation
  } refined;
  uint16_t raw;
} rda_reg02;

//0x03
typedef union {
  struct {
    uint16_t SPACE :2;        //!< See Channel space table above
    uint16_t BAND :2;         //!< Seet band table above
    uint16_t TUNE :1;        //!< Tune; 0 = Disable; 1 = Enable
    uint16_t DIRECT_MODE :1; //!< Directly Control Mode, Only used when test
    uint16_t CHAN :10;       //!< Channel Select.
  } refined;
  uint16_t raw;
} rda_reg03;

// 0x04
typedef union {
  struct {
    uint8_t GPIO1 :2; //!< General Purpose I/O 1. when gpio_sel=01; 00 = High impedance; 01 = Reserved; 10 = Low; 11 = High
    uint8_t GPIO2 :2; //!< General Purpose I/O 2. when gpio_sel=01; 00 = High impedance; 01 = Reserved; 10 = Low; 11 = High
    uint8_t GPIO3 :2; //!< General Purpose I/O 1. when gpio_sel=01; 00 = High impedance; 01 = Mono/Stereo indicator (ST); 10 = Low; 11 = High
    uint8_t I2S_ENABLE :1;   //!< I2S enable; 0 = disabled; 1 = enabled.
    uint8_t RSVD1 :1;
    uint8_t AFCD :1;   //!< AFC disable; If 0, afc work; If 1, afc disabled.
    uint8_t SOFTMUTE_EN :1; //!< If 1, softmute enable.
    uint8_t RDS_FIFO_CLR :1; //!< 1 = clear RDS fifo
    uint8_t DE :1;           //!< De-emphasis; 0 = 75 μs; 1 = 50 μs
    uint8_t RDS_FIFO_EN :1;  //!< 1 = RDS fifo mode enable.
    uint8_t RBDS :1;         //!< 1 = RBDS mode enable; 0 = RDS mode only
    uint8_t STCIEN :1; //!< Seek/Tune Complete Interrupt Enable; 0 = Disable Interrupt; 1 = Enable Interrupt;
    uint8_t RSVD2 :1;
  } refined;
  uint16_t raw;
} rda_reg04;

//0x05
typedef union {
  struct {
    uint8_t VOLUME :4; //!< DAC Gain Control Bits (Volume); 0000 = min volume; 1111 = max volume.
    uint8_t LNA_ICSEL_BIT :2; //!< Lna working current bit: 00=1.8mA; 01=2.1mA; 10=2.5mA; 11=3.0mA.
    uint8_t LNA_PORT_SEL :2; //!< LNA input port selection bit: 00: no input; 01: LNAN; 10: LNAP; 11: dual port input
    uint8_t SEEKTH :4;         //!< Seek SNR Threshold value
    uint8_t RSVD2 :1;
    uint8_t SEEK_MODE :2; //!< Default value is 00; When = 10, will add the RSSI seek mode
    uint8_t INT_MODE :1; //!< If 0, generate 5ms interrupt; If 1, interrupt last until read reg0CH action occurs.
  } refined;
  uint16_t raw;
} rda_reg05;

//0x06
typedef union {
  struct {
    uint8_t R_DELY :1;       //!< If 1, R channel data delay 1T.
    uint8_t L_DELY :1;       //!< If 1, L channel data delay 1T.
    uint8_t SCLK_O_EDGE :1;  //!< If 1, invert sclk output when as master.
    uint8_t SW_O_EDGE :1;    //!< If 1, invert ws output when as master.
    uint8_t I2S_SW_CNT :4;   //!< Only valid in master mode. See table above
    uint8_t WS_I_EDGE :1; //!< If 0, use normal ws internally; If 1, inverte ws internally.
    uint8_t DATA_SIGNED :1; //!< If 0, I2S output unsigned 16-bit audio data. If 1, I2S output signed 16-bit audio data.
    uint8_t SCLK_I_EDGE :1; //!< If 0, use normal sclk internally;If 1, inverte sclk internally.
    uint8_t WS_LR :1; //!< Ws relation to l/r channel; If 0, ws=0 ->r, ws=1 ->l; If 1, ws=0 ->l, ws=1 ->r.
    uint8_t SLAVE_MASTER :1; //!< I2S slave or master; 1 = slave; 0 = master.
    uint8_t OPEN_MODE :2; //!< Open reserved register mode;  11=open behind registers writing function others: only open behind registers reading function.
    uint8_t RSVD :1;
  } refined;
  uint16_t raw;
} rda_reg06;

//0x07
typedef union {
  struct {
    uint8_t FREQ_MODE :1; //!< If 1, then freq setting changed. Freq = 76000(or 87000) kHz + freq_direct (08H) kHz.
    uint8_t SOFTBLEND_EN :1; //!< If 1, Softblend enable
    uint8_t SEEK_TH_OLD :6; //!< Seek threshold for old seek mode, Valid when Seek_Mode=001
    uint8_t RSVD1 :1;
    uint8_t MODE_50_65 :1;   //!< 1 = 65~76 MHz;  0 = 50~76MHz
    uint8_t TH_SOFRBLEND :5; //!< Threshold for noise soft blend setting, unit 2dB (default 0b10000).
    uint8_t RSVD2 :1;
  } refined;
  uint16_t raw;
} rda_reg07;

// 0x08
typedef union {
  struct {
    uint8_t lowByte;
    uint8_t highByte;
  } refined;
  uint16_t raw;
} rda_reg08;

// 0x0a
typedef union {
  struct {
    uint16_t READCHAN :10; //See Channel table . See table above
    uint16_t ST :1;        //Stereo Indicator; 0 = Mono; 1 = Stereo
    uint16_t BLK_E :1; //When RDS enable: 1 = Block E has been found; 0 = no
    //Block E has been found
    uint16_t RDSS :1;  //RDS Synchronization; 0 = RDS decoder not
    //synchronized(default); 1 = RDS decoder synchronized;
    //Available only in RDS Verbose mode
    uint16_t SF :1;    //Seek Fail. 0 = Seek successful; 1 = Seek failure;
    uint16_t STC :1;   //Seek/Tune Complete. 0 = Not complete; 1 = Complete;
    uint16_t RDSR :1; //RDS ready; 0 = No RDS/RBDS group ready(default); 1 =
  //New RDS/RBDS group ready.
  } refined;
  uint16_t raw;
} rda_reg0a;

//0x0b
typedef union {
  struct {
    uint8_t BLERB :2;      //!< Block Errors Level of RDS_DATA_1
    uint8_t BLERA :2;      //!< Block Errors Level of RDS_DATA_0
    uint8_t ABCD_E :1; //!< 1 = the block id of register 0cH,0dH,0eH,0fH is E;  0 = the block id of register 0cH, 0dH, 0eH,0fH is A, B, C, D
    uint8_t RSVD1 :2;
    uint8_t FM_READY :1;   //!< 1=ready; 0=not ready.
    uint8_t FM_TRUE :1; //!< 1 = the current channel is a station; 0 = the current channel is not a station.
    uint8_t RSSI :7; //!< RSSI; 000000 = min; 111111 = max; RSSI scale is logarithmic.
  } refined;
  uint16_t raw;
} rda_reg0b;

//0x0c
typedef union {
  struct {
    uint8_t lowByte;
    uint8_t highByte;
  } refined;
  uint16_t RDSA; //!< BLOCK A ( in RDS mode) or BLOCK E (in RBDS mode when ABCD_E flag is 1)
} rda_reg0c;

//0x0d
typedef union {
  struct {
    uint8_t lowByte;
    uint8_t highByte;
  } refined;
  uint16_t RDSB;
} rda_reg0d;

//0x0e
typedef union {
  struct {
    uint8_t lowByte;
    uint8_t highByte;
  } refined;
  uint16_t RDSC;
} rda_reg0e;

//0x0f
typedef union {
  struct {
    uint8_t lowByte;
    uint8_t highByte;
  } refined;
  uint16_t RDSD;
} rda_reg0f;

typedef struct {
  rda_reg00 reg00;
  rda_reg01 reg01;
  rda_reg02 reg02;
  rda_reg03 reg03;
  rda_reg04 reg04;
  rda_reg05 reg05;
  rda_reg06 reg06;
  rda_reg07 reg07;
  rda_reg08 reg08;
} rda5807_config_t;

typedef struct {
  rda_reg0a reg0a;
  rda_reg0b reg0b;
  rda_reg0c reg0c;
  rda_reg0d reg0d;
  rda_reg0e reg0e;
  rda_reg0f reg0f;
} rda5807_status_t;

int8_t rda5807_init(I2C_HandleTypeDef *i2c_h);
int8_t rda5807_check_is_connected(I2C_HandleTypeDef *i2c);
int8_t rda5807_write_register(uint8_t reg, uint16_t val);
void rda5807_read_status(void);
void rda5807_read_status_ex(void);

int rda5807_get_frequency(void);
void rda5807_set_frequency(int freq);
int rda5807_get_rssi(void);
int rda5807_get_stereo(void);

#endif /* INC_RDA5807_H_ */
