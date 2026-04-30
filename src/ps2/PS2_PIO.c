#include "PS2_PIO.h"
// #include "kb_u_codes.h"

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"


#include "string.h"

static uint16_t pio_program_PS2_instructions[] = {
 
               
            //     .wrap                
    // 0xe02a, //  0: set    x, 10                       
    // 0x2000|(PIN_PS2_CLK-PIN_PS2_DELTA), //  1: wait   0 gpio, 0                  
    // 0x4001, //  2: in     pins, 1                    
    // 0x2080|(PIN_PS2_CLK-PIN_PS2_DELTA), //  3: wait   1 gpio, 0                  
    // 0x0041, //  4: jmp    x--, 1   
    
    0xe02a, //  0: set    x, 10                       
    0x2060, //  1: wait   0 gpio, 0                  
    0x4001, //  2: in     pins, 1                    
    0x20e0, //  3: wait   1 gpio, 0                  
    0x0041, //  4: jmp    x--, 1                    

    // 0x4075, //  5: in     null, 21                   
    // 0x8020, //  6: push   block                      
       
               

};



static const struct pio_program pio_program_PS2 = {
    .instructions = pio_program_PS2_instructions,
    .length = 5,
    .origin = -1,
};

#define LEN_PS2_BUF (100)
static uint32_t PS_2_BUS[LEN_PS2_BUF];

static uint32_t inx_rd_buf=0;

//static PIO pio_ps2=pio0;
static int sm_ps2=-1;
static kb_state_t kb_st_ps2;

bool parity8(uint8_t data)
{
    bool out=1;
    for(int i=8;i--;)
    {
      out^=(data&1);
      data>>=1;
    }
    return out;
}


uint8_t __not_in_flash_func(get_scan_code)(void)

//static inline uint8_t (get_scan_code)(void)

{
    if (PS_2_BUS[inx_rd_buf]==0) {
       // printf("SCAN_CODES=0");//test
        return 0;
        }

    uint32_t val=PS_2_BUS[inx_rd_buf];

#ifndef UART_PS2
    val>>=21;
     //printf("sc=0x%08x\n",val);//test
    if (((val&0x401)!=0x400)||((val>>9)&1)!=(parity8((val>>1)&0xff))) 
        {  //если ошибка данных ps/2
           //очищаем данные в буфере 
          
            // while(PS_2_BUS[inx_rd_buf])
            // { 
            //   sleep_ms(2);
            //   PS_2_BUS[inx_rd_buf]=0;
            //   inx_rd_buf=(inx_rd_buf+1)%LEN_PS2_BUF;
            // }
            PS_2_BUS[inx_rd_buf]=0;
            inx_rd_buf=(inx_rd_buf+1)%LEN_PS2_BUF;
           // перезапускаем SM и очищаем состояние клавиатуры

            //printf("Error PS/2. Restart SM\n");
            pio_sm_restart(pio_PS2, sm_ps2);
            memset(&kb_st_ps2,0,sizeof(kb_st_ps2));           
            return 0;
        }
#endif
    PS_2_BUS[inx_rd_buf]=0;
    inx_rd_buf=(inx_rd_buf+1)%LEN_PS2_BUF;


#ifdef UART_PS2
     return (uint8_t)(val);
#else 
     return (val>>1)&0xff;
#endif

}

uint32_t get_inx_ps2(){return inx_rd_buf;};


static void  inInit(uint gpio)
{
    gpio_init(gpio);
    gpio_set_dir(gpio,GPIO_IN);
    gpio_pull_up(gpio);

}


void init_PS2()
{
//   init_PS2_PIO(pio_PS2);
// };

// void init_PS2_PIO(PIO pio)
// {


#ifdef UART_PS2

    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_PS2_PIN_RX, GPIO_FUNC_UART);

    int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, true);
#else
    // PIO pio=pio_PS2;
    // pio_ps2=pio;
    sm_ps2=pio_claim_unused_sm(pio_PS2, true);;
    pio_set_gpio_base(pio_PS2, PIN_PS2_DELTA);
    

    uint offset = pio_add_program(pio_PS2, &pio_program_PS2);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + (pio_program_PS2.length-1)); 
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    
    // sm_config_set_in_pin_base(&c, 16);

    sm_config_set_jmp_pin(&c,PIN_PS2_CLK);


//    inInit(PIN_PS2_CLK);
//    inInit(PIN_PS2_DATA);
     pio_gpio_init(pio_PS2, PIN_PS2_CLK);
     pio_gpio_init(pio_PS2, PIN_PS2_DATA);

    sm_config_set_in_shift(&c, true, true, 11);//??????  
    sm_config_set_in_pins(&c, PIN_PS2_DATA);

    pio_sm_init(pio_PS2, sm_ps2, offset, &c);
    pio_sm_set_enabled(pio_PS2, sm_ps2, true);
     
     uint32_t freq_sm=200000;
    float fdiv=(clock_get_hz(clk_sys)/freq_sm);//частота SM
    if (fdiv<1) fdiv=1;
    uint32_t fdiv32=(uint32_t) (fdiv * (1 << 16));
    fdiv32=fdiv32&0xfffff000;//округление делителя
    pio_PS2->sm[sm_ps2].clkdiv=fdiv32; //делитель для конкретной sm

#endif
    //инициализация DMA
    int dma_chan0 = dma_claim_unused_channel(true);
    int dma_chan1 = dma_claim_unused_channel(true);

    dma_channel_config c0 = dma_channel_get_default_config(dma_chan0);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
    channel_config_set_read_increment(&c0, false);
    channel_config_set_write_increment(&c0, true);

    uint dreq=DREQ_PIO1_RX0+sm_ps2;
    if (pio_PS2==pio0) dreq=DREQ_PIO0_RX0+sm_ps2;
    if (pio_PS2==pio1) dreq=DREQ_PIO1_RX0+sm_ps2;
    if (pio_PS2==pio2) dreq=DREQ_PIO2_RX0+sm_ps2;


    channel_config_set_dreq(&c0, dreq);


    
    channel_config_set_chain_to(&c0, dma_chan1);                      

    uint32_t* addr_read_dma= (uint32_t*)&pio_PS2->rxf[sm_ps2];
#ifdef UART_PS2
    channel_config_set_dreq(&c0, DREQ_UART0_RX);
    addr_read_dma=&(uart_get_hw(UART_ID)->dr);
    //uart_puts(UART_ID, "\nHello, uart interrupts\n");
#endif

    dma_channel_configure(
        dma_chan0,
        &c0,
        &PS_2_BUS[0], // Write address 
        addr_read_dma,             //  read address
        LEN_PS2_BUF, // 
        false            // Don't start yet
    );

    dma_channel_config c1 = dma_channel_get_default_config(dma_chan1);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    channel_config_set_read_increment(&c1, false);
    channel_config_set_write_increment(&c1, false);
    channel_config_set_chain_to(&c1, dma_chan0);                      

    static uint32_t addr_write_DMA0[1];
    addr_write_DMA0[0]=(uint32_t)&PS_2_BUS[0]; 
    dma_channel_configure(
        dma_chan1,
        &c1,
        &dma_hw->ch[dma_chan0].write_addr, // Write address 
        &addr_write_DMA0[0],             //  read address
        1, // 
        false            // Don't start yet
    );

    for(int i=0;i<LEN_PS2_BUF;i++) PS_2_BUS[i]=0;

    dma_start_channel_mask((1u << dma_chan0)) ;
    // dma_start_channel_mask((1u << dma_chan1)) ;



};

kb_state_t __not_in_flash_func(get_PS2_kb_state)(){ return kb_st_ps2; };

void __not_in_flash_func(translate_scancode)(uint8_t code,bool is_press, bool is_e0,bool is_e1)
  {
    if (is_e1)
    {
      if (code==0x14) {if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_PAUSE_BREAK); else CLR_STATE_KEY(kb_st_ps2,KEY_PAUSE_BREAK);}
      return;
    }

    if (!is_e0)
      switch (code)
      {
//0
      case 0x1C: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_A); else CLR_STATE_KEY(kb_st_ps2,KEY_A); break;
      case 0x32: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_B); else CLR_STATE_KEY(kb_st_ps2,KEY_B); break;
      case 0x21: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_C); else CLR_STATE_KEY(kb_st_ps2,KEY_C); break;
      case 0x23: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_D); else CLR_STATE_KEY(kb_st_ps2,KEY_D); break;
      case 0x24: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_E); else CLR_STATE_KEY(kb_st_ps2,KEY_E); break;
      case 0x2B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F); else CLR_STATE_KEY(kb_st_ps2,KEY_F); break;
      case 0x34: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_G); else CLR_STATE_KEY(kb_st_ps2,KEY_G); break;
      case 0x33: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_H); else CLR_STATE_KEY(kb_st_ps2,KEY_H); break;
      case 0x43: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_I); else CLR_STATE_KEY(kb_st_ps2,KEY_I); break;
      case 0x3B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_J); else CLR_STATE_KEY(kb_st_ps2,KEY_J); break;

      case 0x42: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_K); else CLR_STATE_KEY(kb_st_ps2,KEY_K); break;
      case 0x4B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_L); else CLR_STATE_KEY(kb_st_ps2,KEY_L); break;
      case 0x3A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_M); else CLR_STATE_KEY(kb_st_ps2,KEY_M); break;
      case 0x31: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_N); else CLR_STATE_KEY(kb_st_ps2,KEY_N); break;
      case 0x44: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_O); else CLR_STATE_KEY(kb_st_ps2,KEY_O); break;
      case 0x4D: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_P); else CLR_STATE_KEY(kb_st_ps2,KEY_P); break;
      case 0x15: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_Q); else CLR_STATE_KEY(kb_st_ps2,KEY_Q); break;
      case 0x2D: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_R); else CLR_STATE_KEY(kb_st_ps2,KEY_R); break;
      case 0x1B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_S); else CLR_STATE_KEY(kb_st_ps2,KEY_S); break;
      case 0x2C: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_T); else CLR_STATE_KEY(kb_st_ps2,KEY_T); break;

      case 0x3C: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_U); else CLR_STATE_KEY(kb_st_ps2,KEY_U); break;
      case 0x2A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_V); else CLR_STATE_KEY(kb_st_ps2,KEY_V); break;
      case 0x1D: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_W); else CLR_STATE_KEY(kb_st_ps2,KEY_W); break;
      case 0x22: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_X); else CLR_STATE_KEY(kb_st_ps2,KEY_X); break;
      case 0x35: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_Y); else CLR_STATE_KEY(kb_st_ps2,KEY_Y); break;
      case 0x1A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_Z); else CLR_STATE_KEY(kb_st_ps2,KEY_Z); break;

      case 0x54: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_LEFT_BR); else CLR_STATE_KEY(kb_st_ps2,KEY_LEFT_BR); break;
      case 0x5B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_RIGHT_BR); else CLR_STATE_KEY(kb_st_ps2,KEY_RIGHT_BR); break;
      case 0x4C: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_SEMICOLON); else CLR_STATE_KEY(kb_st_ps2,KEY_SEMICOLON); break;
      case 0x52: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_QUOTE); else CLR_STATE_KEY(kb_st_ps2,KEY_QUOTE); break;
      case 0x41: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_COMMA); else CLR_STATE_KEY(kb_st_ps2,KEY_COMMA); break;
      case 0x49: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_PERIOD); else CLR_STATE_KEY(kb_st_ps2,KEY_PERIOD); break;

//1 -----------
      case 0x45: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_0); else CLR_STATE_KEY(kb_st_ps2,KEY_0); break;
      case 0x16: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_1); else CLR_STATE_KEY(kb_st_ps2,KEY_1); break;
      case 0x1E: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_2); else CLR_STATE_KEY(kb_st_ps2,KEY_2); break;
      case 0x26: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_3); else CLR_STATE_KEY(kb_st_ps2,KEY_3); break;
      case 0x25: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_4); else CLR_STATE_KEY(kb_st_ps2,KEY_4); break;
      case 0x2E: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_5); else CLR_STATE_KEY(kb_st_ps2,KEY_5); break;
      case 0x36: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_6); else CLR_STATE_KEY(kb_st_ps2,KEY_6); break;
      case 0x3D: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_7); else CLR_STATE_KEY(kb_st_ps2,KEY_7); break;
      case 0x3E: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_8); else CLR_STATE_KEY(kb_st_ps2,KEY_8); break;
      case 0x46: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_9); else CLR_STATE_KEY(kb_st_ps2,KEY_9); break;

      case 0x4E: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_MINUS); else CLR_STATE_KEY(kb_st_ps2,KEY_MINUS); break;
      case 0x55: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_EQUALS); else CLR_STATE_KEY(kb_st_ps2,KEY_EQUALS); break;
      case 0x5D: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_BACKSLASH); else CLR_STATE_KEY(kb_st_ps2,KEY_BACKSLASH); break;
      case 0x66: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_BACK_SPACE); else CLR_STATE_KEY(kb_st_ps2,KEY_BACK_SPACE); break;
      case 0x5A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_ENTER); else CLR_STATE_KEY(kb_st_ps2,KEY_ENTER); break;
      case 0x4A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_SLASH); else CLR_STATE_KEY(kb_st_ps2,KEY_SLASH); break;
      case 0x0E: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_TILDE); else CLR_STATE_KEY(kb_st_ps2,KEY_TILDE); break;
      case 0x0D: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_TAB); else CLR_STATE_KEY(kb_st_ps2,KEY_TAB); break;
      case 0x58: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_CAPS_LOCK); else CLR_STATE_KEY(kb_st_ps2,KEY_CAPS_LOCK); break;
      case 0x76: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_ESC); else CLR_STATE_KEY(kb_st_ps2,KEY_ESC); break;
      
      case 0x12: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_L_SHIFT); else CLR_STATE_KEY(kb_st_ps2,KEY_L_SHIFT); break;
      case 0x14: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_L_CTRL); else CLR_STATE_KEY(kb_st_ps2,KEY_L_CTRL); break;
      case 0x11: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_L_ALT); else CLR_STATE_KEY(kb_st_ps2,KEY_L_ALT); break;
      case 0x59: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_R_SHIFT); else CLR_STATE_KEY(kb_st_ps2,KEY_R_SHIFT); break;
      
      case 0x29: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_SPACE); else CLR_STATE_KEY(kb_st_ps2,KEY_SPACE); break;
//2 -----------
      case 0x70: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_0); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_0); break;
      case 0x69: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_1); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_1); break;
      case 0x72: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_2); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_2); break;
      case 0x7A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_3); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_3); break;
      case 0x6B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_4); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_4); break;
      case 0x73: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_5); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_5); break;
      case 0x74: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_6); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_6); break;
      case 0x6C: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_7); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_7); break;
      case 0x75: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_8); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_8); break;
      case 0x7D: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_9); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_9); break;

      case 0x77: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_LOCK); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_LOCK); break;
      case 0x7C: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_MULT); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_MULT); break;
      case 0x7B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_MINUS); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_MINUS); break;
      case 0x79: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_PLUS); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_PLUS); break;
      case 0x71: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_PERIOD); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_PERIOD); break;
      case 0x7E: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_SCROLL_LOCK); else CLR_STATE_KEY(kb_st_ps2,KEY_SCROLL_LOCK); break;
//3 -----------
      case 0x05: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F1); else CLR_STATE_KEY(kb_st_ps2,KEY_F1); break;
      case 0x06: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F2); else CLR_STATE_KEY(kb_st_ps2,KEY_F2); break;
      case 0x04: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F3); else CLR_STATE_KEY(kb_st_ps2,KEY_F3); break;
      case 0x0C: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F4); else CLR_STATE_KEY(kb_st_ps2,KEY_F4); break;
      case 0x03: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F5); else CLR_STATE_KEY(kb_st_ps2,KEY_F5); break;
      case 0x0B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F6); else CLR_STATE_KEY(kb_st_ps2,KEY_F6); break;
      case 0x83: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F7); else CLR_STATE_KEY(kb_st_ps2,KEY_F7); break;
      case 0x0A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F8); else CLR_STATE_KEY(kb_st_ps2,KEY_F8); break;
      case 0x01: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F9); else CLR_STATE_KEY(kb_st_ps2,KEY_F9); break;
      case 0x09: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F10); else CLR_STATE_KEY(kb_st_ps2,KEY_F10); break;
     
      case 0x78: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F11); else CLR_STATE_KEY(kb_st_ps2,KEY_F11); break;
      case 0x07: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_F12); else CLR_STATE_KEY(kb_st_ps2,KEY_F12); break;



      default:
        break;
      }
    if (is_e0)
      switch (code)
      {
//1----------------
      case 0x1F: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_L_WIN); else CLR_STATE_KEY(kb_st_ps2,KEY_L_WIN); break;
      case 0x14: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_R_CTRL); else CLR_STATE_KEY(kb_st_ps2,KEY_R_CTRL); break;
      case 0x11: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_R_ALT); else CLR_STATE_KEY(kb_st_ps2,KEY_R_ALT); break;
      case 0x27: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_R_WIN); else CLR_STATE_KEY(kb_st_ps2,KEY_R_WIN); break;
      case 0x2F: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_MENU); else CLR_STATE_KEY(kb_st_ps2,KEY_MENU); break;
//2------------------
      //для принт скрин обработаем только 1 код
      case 0x12: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_PRT_SCR); else CLR_STATE_KEY(kb_st_ps2,KEY_PRT_SCR); break;


      case 0x4A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_SLASH); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_SLASH); break;
      case 0x5A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_NUM_ENTER); else CLR_STATE_KEY(kb_st_ps2,KEY_NUM_ENTER); break;
      case 0x75: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_UP); else CLR_STATE_KEY(kb_st_ps2,KEY_UP); break;
      case 0x72: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_DOWN); else CLR_STATE_KEY(kb_st_ps2,KEY_DOWN); break;
      case 0x74: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_RIGHT); else CLR_STATE_KEY(kb_st_ps2,KEY_RIGHT); break;
      case 0x6B: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_LEFT); else CLR_STATE_KEY(kb_st_ps2,KEY_LEFT); break;      
      case 0x71: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_DELETE); else CLR_STATE_KEY(kb_st_ps2,KEY_DELETE); break;
      case 0x69: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_END); else CLR_STATE_KEY(kb_st_ps2,KEY_END); break;
      case 0x7A: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_PAGE_DOWN); else CLR_STATE_KEY(kb_st_ps2,KEY_PAGE_DOWN); break;
      case 0x7D: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_PAGE_UP); else CLR_STATE_KEY(kb_st_ps2,KEY_PAGE_UP); break;
      
      case 0x6C: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_HOME); else CLR_STATE_KEY(kb_st_ps2,KEY_HOME); break;
      case 0x70: if (is_press) SET_STATE_KEY(kb_st_ps2,KEY_INSERT); else CLR_STATE_KEY(kb_st_ps2,KEY_INSERT); break;


      }
    
  

   
  }


 bool __not_in_flash_func(decode_PS2)()
{
  static bool is_e0=false;
  static bool is_e1=false;
  static bool is_f0=false;
  

    uint8_t scancode=get_scan_code();
    if (scancode==0xe0) {is_e0=true;return false;}
    if (scancode==0xe1) {is_e1=true;return false;}
    if (scancode==0xf0) {is_f0=true;return false;}  
    if (scancode)
                {
                    //сканкод 

                    //получение универсальных кодов из сканкодов PS/2
                    translate_scancode(scancode,!is_f0,is_e0,is_e1);
                    is_e0=false;
                    if (is_f0) is_e1=false;
                    is_f0=false;
                   //test
                    //  static char test_str[128];  
                    // keys_to_str(test_str,' ',kb_st_ps2);
                    //  printf("is_e0=%d, is_f0=%d, code=0x%02x\n",is_e0,is_f0,scancode);
                    //  printf(test_str);
                   
                   
                   
                    //преобразование из универсальных сканкодов в матрицу бытрого преобразования кодов для zx клавиатуры
                    //zx_kb_decode(zx_keyboard_state);

                      return true;//произошли изменения

                    }
  return false;
}
