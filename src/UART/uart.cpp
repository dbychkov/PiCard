#include "uart.h"
#include <string.h>
#include <Arduino.h>

#if NO_CTS
static int chan=-1;

//задать размер буфера
#define FIFO_SIZE_BITS 8
#define FIFO_SIZE (1 << FIFO_SIZE_BITS)
static uint32_t last_read_ptr = 0;
static uint8_t dma_fifo[FIFO_SIZE] __attribute__((aligned(FIFO_SIZE))) ;
#endif

void init_HW_UART()
{
      // Инициализация UART и пинов
    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(PIN_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART_RX, GPIO_FUNC_UART);
       // uart_set_fifo_enabled(UART_ID, false);//test
#if NO_CTS
     // Отключаем аппаратное FIFO самого UART (опционально, 
    // но с DMA лучше работать побайтово через DREQ)
    uart_set_fifo_enabled(UART_ID, false);

    // 2. Настройка DMA
    chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(chan);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false); // Читаем всегда из одного регистра UART
    channel_config_set_write_increment(&c, true);  // Пишем в память по кругу
    
    // Включаем кольцевой режим для записи (размер 2^8 = 256)
    channel_config_set_ring(&c, false, FIFO_SIZE_BITS);

    // Синхронизация: DMA ждет, пока в UART появятся данные
    channel_config_set_dreq(&c, uart_get_dreq(UART_ID, false));

    dma_channel_configure(
        chan,
        &c,
        dma_fifo,                   // Куда: наш буфер в RAM
        &(uart_get_hw(UART_ID)->dr),  // Откуда: регистр данных UART
        0,                 // Количество 
        false                        // Старт отложен
    );
     uint32_t endless_val = (1u << 28) | FIFO_SIZE;
    dma_hw->ch[chan].al1_transfer_count_trig = endless_val;
//        (DMA_CH0_TRANS_COUNT_MODE_VALUE_ENDLESS << DMA_CH0_TRANS_COUNT_MODE_LSB);
    dma_channel_start(chan);

    //test
    gpio_init(PIN_UART_CTS);
    gpio_set_dir(PIN_UART_CTS, GPIO_OUT);
    gpio_put(PIN_UART_CTS,0);

#else
    gpio_init(PIN_UART_CTS);
    gpio_set_dir(PIN_UART_CTS, GPIO_OUT);
    gpio_put(PIN_UART_CTS,1);
#endif
};
#if NO_CTS
uint8_t __not_in_flash_func(read_uart)()
    {
            uint32_t current_write_ptr = (uint32_t)dma_hw->ch[chan].write_addr % FIFO_SIZE;
            gpio_put(PIN_UART_CTS,(last_read_ptr != current_write_ptr));//test
            if  (last_read_ptr != current_write_ptr) 
                {
                    uint8_t data_rd=dma_fifo[last_read_ptr];
                    // Сдвигаем указатель чтения
                    last_read_ptr = (last_read_ptr + 1) % FIFO_SIZE;
                    return data_rd;
                }
                
            else return dma_fifo[last_read_ptr];
    };
#else
uint8_t __not_in_flash_func(read_uart)()
    {
        uint8_t data=uart_get_hw(UART_ID)->dr;
        gpio_put(PIN_UART_CTS,(uart_is_readable(UART_ID)));  
        return data;
    };
            
#endif
void __not_in_flash_func(RST_HW_UART)(){};
uint8_t __not_in_flash_func(get_UART_status)()
{
#if NO_CTS
        uint32_t current_write_ptr = (uint32_t)dma_hw->ch[chan].write_addr % FIFO_SIZE;
        #define RXC  (last_read_ptr != current_write_ptr)
        
        uart_hw_t *hw = uart_get_hw(UART_ID);


        #define TXC  ((hw->fr & UART_UARTFR_TXFE_BITS)==0)
        #define UDRE (uart_is_writable(UART_ID))
        gpio_put(PIN_UART_CTS,RXC); //test
#else
        uart_hw_t *hw = uart_get_hw(UART_ID);


        #define TXC  ((hw->fr & UART_UARTFR_TXFE_BITS)==0)
        #define UDRE (uart_is_writable(UART_ID))        
//        #define TXC  (!UDRE)//        
//        #define UDRE (!TXC)
        #define RXC  (uart_is_readable(UART_ID))
        gpio_put(PIN_UART_CTS,RXC);
#endif
//        return 0;//test
       return ((RXC)<<7)|((TXC)<<6)|(UDRE<<5)|0x00;        

//        return ((RXC)<<7)|((TXC)<<6)|0x0;
};
