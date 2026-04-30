#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "inttypes.h"
#include "hardware/pio.h"
#include "../kbd/kb_u_codes.h"
#include "../../config.h"

// #define PIO_PS2
//#define UART_PS2

#ifdef UART_PS2
    #define UART_ID uart0
    #define BAUD_RATE 12500
    #define DATA_BITS 8
    #define STOP_BITS 1
    #define PARITY    UART_PARITY_NONE
    #define UART_PS2_PIN_RX  (1)

#endif

// #define PIN_PS2_DATA (39)
// #define PIN_PS2_CLK (38)
// #define pio_PS2 pio2

#if (PIN_PS2_CLK>=30)
 #define PIN_PS2_DELTA (16)
#else
 #define PIN_PS2_DELTA (0)
#endif
// #define PIN_PS2_DATA (7)
// #define PIN_PS2_CLK (6)

// #define pio_PS2 pio0

// void init_PS2_PIO(PIO pio);

void init_PS2();

//uint8_t (get_scan_code)(void);
uint32_t get_inx_ps2();
bool decode_PS2();

kb_state_t get_PS2_kb_state();

#ifdef __cplusplus
};
#endif
