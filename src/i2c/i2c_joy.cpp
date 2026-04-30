#include "i2c_joy.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "../../config.h"

#if JOY_I2C

typedef enum
{NES_MODE,SEGA_MODE}joy_mode_t;

static uint8_t joy_state=0x00;

static uint8_t nes_to_kempston_convert[256];
static uint8_t sega_to_kempston_convert[256];

#define NES_PIN_LATCH_MASK (1<<2)
#define NES_PIN_CLK_MASK (1<<3)
#define NES_PIN_DATA_MASK (1<<1)

#define SEGA_UP_MASK    (1<<0)
#define SEGA_DOWN_MASK  (1<<1)
#define SEGA_LEFT_MASK  (1<<2)
#define SEGA_RIGHT_MASK (1<<3)
#define SEGA_B_MASK     (1<<4)
#define SEGA_C_MASK     (1<<5)
#define SEGA_A_MASK     (1<<4)
#define SEGA_SEL_MASK   (1<<6)
#define SEGA_START_MASK   (1<<5)



void init_i2c_joy()
{
  i2c_deinit(I2C_PORT);
  i2c_init(I2C_PORT, I2C_SPEED);
  gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(PIN_I2C_SDA);
  gpio_pull_up(PIN_I2C_SCL);

   for(int nes_keys=0;nes_keys<256;nes_keys++)
      {
              uint8_t data_kmpst=(nes_keys&0x0f)|((nes_keys>>2)&0x30)|((nes_keys<<3)&0x80)|((nes_keys<<1)&0x40);
              data_kmpst^=0xff;
              nes_to_kempston_convert[nes_keys]=data_kmpst;
      }

   for(int sega_keys=0;sega_keys<256;sega_keys++)
      {
              //uint8_t data_kmpst=(sega_keys&0x80)|((sega_keys<<1)&0x60)|((sega_keys>>2)&0x10)|((sega_keys>>3)&0x01)|((sega_keys>>1)&0x02)|((sega_keys<<1)&0x04)|((sega_keys<<3)&0x08);              
              uint8_t data_kmpst=(sega_keys&0x90)|((sega_keys<<1)&0x40)|((sega_keys>>1)&0x20)|((sega_keys>>3)&0x01)|((sega_keys>>1)&0x02)|((sega_keys<<1)&0x04)|((sega_keys<<3)&0x08);
              
              data_kmpst^=0xff;
              sega_to_kempston_convert[sega_keys]=data_kmpst;
      }
  
  uint8_t wr_buf[2];
  wr_buf[0]=0xff;
  wr_buf[1]=0xff;
  i2c_write_blocking(I2C_PORT, PCF8575_ADDR, wr_buf, 2, false);

}

static bool  __no_inline_not_in_flash_func(i2c_transfer_non_blocking)(i2c_inst_t *i2c, uint8_t addr, uint8_t* wr_data,uint8_t* rd_data, int len) {
    
   
    if ((len>16) ||( i2c_get_write_available(i2c)<16)) return false;

    //чтение из fifo
    i2c_hw_t *hw = i2c_get_hw(i2c);
    for (int i=len;i--;) *rd_data++=(uint8_t)hw->data_cmd;

    //выставление адреса
    i2c_get_hw(i2c)->enable = 0;
    i2c_get_hw(i2c)->tar = addr;
    i2c_get_hw(i2c)->enable = 1;
    


    //запуск чтения i2c
    for(int i=0;i<len;i++)
    {
       if (i!=(len-1))   i2c->hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS;
      else  i2c->hw->data_cmd = I2C_IC_DATA_CMD_CMD_BITS | I2C_IC_DATA_CMD_STOP_BITS;
    }
   //запись i2c
    for(int i=0;i<len;i++)
    {
        
        // Отправляем данные напрямую в регистр данных
        // i2c->hw->data_cmd = data[i];
        if (i!=(len-1)) i2c->hw->data_cmd = wr_data[i];
        else i2c->hw->data_cmd = wr_data[i]|I2C_IC_DATA_CMD_STOP_BITS;
    }
    
    return true;


}

uint8_t __no_inline_not_in_flash_func(get_joy_data)(){return joy_state;};


static joy_mode_t joy_mode=SEGA_MODE;


void __no_inline_not_in_flash_func(joy_proc)()
{


    static int inx_clk=-6;
    static uint8_t data_out_NES=0;
    static uint8_t data_out_SEGA=0;
    static uint8_t wr_data=0xff;


    if (joy_mode==NES_MODE)
    {
        inx_clk++;
        static int data_mask=0;
        if (inx_clk>=0)
        {
            switch (inx_clk)
            {
            case 0:
                // gpio_put(PIN_LATCH_DENDY,0);
                data_mask=0x80;
                wr_data&=(0xff^NES_PIN_LATCH_MASK);
               
                break;
            
            case 4 ... 19:
                {

                
                
                if ((inx_clk%2)) wr_data|=NES_PIN_CLK_MASK; else wr_data&=(0xff^NES_PIN_CLK_MASK);

                
                }
                break;
            case 20:
                
                data_mask=0;
                break;
            case 22:

                wr_data|=NES_PIN_LATCH_MASK;
        
                break;
            case 23:            
            
                inx_clk=-6;

                joy_mode=SEGA_MODE;

               
                break;
            default:
                break;
            }

            uint8_t read_buf[2];
            uint8_t wr_buf[2];
            wr_buf[0]=wr_data;
            bool rez=i2c_transfer_non_blocking(I2C_PORT, PCF8575_ADDR, wr_buf, read_buf, 2);

            if (!rez) inx_clk--;
            else
            {
                if (((inx_clk%2))&&(inx_clk<20)&&(inx_clk>3))
                {
                    data_out_NES|=(read_buf[0]&NES_PIN_DATA_MASK)?data_mask:0;
                    data_mask>>=1;
                }
            };


        }
    }
    else 
    if(joy_mode==SEGA_MODE)
    {
        inx_clk++;

        if (inx_clk<0) wr_data=0xff;
        
        switch (inx_clk)
        {
        
        case 0:
            wr_data=0xff&(SEGA_SEL_MASK^0xff);
            break;
        case 5:
                inx_clk=-6;

                data_out_SEGA=0;  
                data_out_NES=0;

                joy_mode=NES_MODE;

            break;

        }
        
        uint8_t read_buf[2];
        uint8_t wr_buf[2];
        wr_buf[0]=wr_data;
        bool rez=i2c_transfer_non_blocking(I2C_PORT, PCF8575_ADDR, wr_buf, read_buf, 2);
        if (!rez) inx_clk--;
        else
        {
            if (inx_clk==0) 
            {
                data_out_SEGA|=(read_buf[0]&(SEGA_UP_MASK|SEGA_DOWN_MASK|SEGA_LEFT_MASK|SEGA_RIGHT_MASK|SEGA_B_MASK|SEGA_C_MASK));
            }

            if (inx_clk==4) 
            {
                data_out_SEGA|=(read_buf[0]&(SEGA_A_MASK|SEGA_START_MASK))<<2;

                if ((read_buf[0]&(SEGA_LEFT_MASK|SEGA_RIGHT_MASK))==0) 
                    joy_state=sega_to_kempston_convert[data_out_SEGA];
                else
                    joy_state=nes_to_kempston_convert[data_out_NES];

            }

            

        };

        


    }
}

#endif

