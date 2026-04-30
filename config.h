#pragma once
#include "inttypes.h"


#define TEST_ROM_EN (0)
#define GS_EN (1)
#define ZC_SD_EN (0)
#define DIVMMC_EN (1)

#define KEMPSTON_EN (1)
#define DENDY_JOY_3_PINS (0)
#define JOY_I2C (1)

#define BEEPER_EN (1)
#define SND_DRIVE_EN (1)
#define COVOX_EN (1)

#define TS_EN (1)

#define KB_PS2_EN (1)

#define PIN_PS2_CLK (38)
#define PIN_PS2_DATA (39)
#define pio_PS2 pio0

#define PIN_UART_RX (37)
#define PIN_UART_TX (36)
#define PIN_UART_CTS (34)

//9600
#define AY_UART_EN (0)
//115200
#define HW_UART_EN (1)

#define UART_ID uart1
#define UART_BAUD_RATE (115200)


#if DENDY_JOY_3_PINS
    #define PIN_DATA_DENDY (34)
    #define PIN_LATCH_DENDY (33)
    #define PIN_CLK_DENDY (32)
#elif JOY_I2C
    #define PIN_I2C_SDA (32)
    #define PIN_I2C_SCL (33)
    #define I2C_SPEED (400000)
    #define I2C_PORT (i2c0)
    #define PCF8575_ADDR (0x20)
    #define I2C_KEMPSTON (1)  
    #define NES_JOY_EN (1)
    #define SEGA_JOY_EN (0)
      
#endif

#if (DIVMMC_EN)&&(ZC_SD_EN)
#define DIV_OFF_PIN (37) 
#endif

#define MHz 1000000 
#define CPU_FREQ (351*MHz)

#define PIO_data_z80 (pio1)
#define PIO_I2S (pio2)

#define SFE_RP2350_XIP_CSI_PIN 0

#define I2S_DATA_PIN 44
#define I2S_CLK_BASE_PIN 45

#define PIN_A0 (8)
#define PIN_D0 (24)





#define PIN_M1 (3)
#define PIN_WR (4)
#define PIN_RD (5)
#define PIN_IORQ (6)
#define PIN_MREQ (7)

#define M1_Z80 (1<<PIN_M1)
#define WR_Z80 (1<<PIN_WR)
#define RD_Z80 (1<<PIN_RD)
#define IORQ_Z80 (1<<PIN_IORQ)
#define MREQ_Z80 (1<<PIN_MREQ)

#define PIN_DATA_CTRL (2)
#define PIN_ROM_BLK (1)
#define PIN_NMI (35)
#define PIN_RESET (47)

#define SD_CARD_EN (1)

#define SD_SPI_RX_PIN  (40)
#define SD_SPI_TX_PIN  (43)
#define SD_SPI_CLK_PIN (42)
#define SD_SPI_CS0_PIN  (41)
#define SD_SPI (spi1)
#define SD_SPI_FREQ_CLK (10*MHZ)

typedef struct{
    int16_t L;
    int16_t R;
}sound_LR;
