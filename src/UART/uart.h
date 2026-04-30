#pragma once

#include "../../config.h"

void init_HW_UART();
uint8_t get_UART_status();
void RST_HW_UART();
//без CTS так и не заработало(, поэтому не включать!!!
#define NO_CTS (0)

#if NO_CTS
    uint8_t read_uart();
    #define READ_UART()   (read_uart())
    #define WRITE_UART(data)  (uart_get_hw(UART_ID)->dr=data)
#else
    uint8_t read_uart();
    #define READ_UART()   (read_uart())
//    #define READ_UART()   (uart_get_hw(UART_ID)->dr)
    #define WRITE_UART(data)  (uart_get_hw(UART_ID)->dr=data)

    //#define READ_UART()   uart_getc(UART_ID)
    //#define WRITE_UART(data)  uart_putc(UART_ID,data)

#endif

