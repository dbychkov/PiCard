#include "pio_data_z80_bus.h"
#include "hardware/pio.h"
#include "../../config.h"

static const uint16_t pio_dataz80_program_instructions[] = {
         //для 245 буфера(2350)

        0xb063, //0: mov pindirs, null  side 1  
        0x90a0, //1: pull   block       side 1
        0x10c0, //2: jmp    pin, 0      side 1 

        0x7008, //4: out    pins, 8     side 1   
        0xa06b,  //3: mov pindirs, ~null side 0  
        //0x6008, //4: out    pins, 8     side 0   
        0x00c0, // 5: jmp    pin, 0          side 0             
       // 0x00c7, // 5: jmp    pin, 7          side 0     
        0x0005, // 6: jmp     5              side 0  
        //0xa442, // 7: nop                    side 0 [4]   

        //для 245 буфера(2040)
        // 0xb0e3, //  0: mov    osr, null       side 1     
        // 0x7088, //  1: out    pindirs, 8      side 1     
        // 0x90a0, //  2: pull   block           side 1     
        // 0x10c0, //  3: jmp    pin, 0          side 1     
        // 0xa027, //  4: mov    x, osr          side 0     
        // 0xa0eb, //  5: mov    osr, !null      side 0     
        // 0x6088, //  6: out    pindirs, 8      side 0     
        // 0xa0e1, //  7: mov    osr, x          side 0     
        // 0x6008, //  8: out    pins, 8         side 0     
        // 0x00c0, //  9: jmp    pin, 0          side 0     
        // 0x0009, // 10: jmp     9              side 0     
        
//         begin:
//     mov    osr, null       side 1
//     out    pindirs, 8      side 1        
// loop:
//     pull   block side 1
//     jmp pin begin side 1
//     mov    x, osr          side 0     
//     mov    osr, !null      side 0     
//     out    pindirs, 8      side 0     
//     mov    osr, x          side 0     
//     out    pins, 8         side 0     
    
// l1: jmp pin begin side 0
//     jmp  l1 side 0

    };
    
    
    static const struct pio_program pio_dataz80_program = {
        .instructions = pio_dataz80_program_instructions,
        .length = sizeof(pio_dataz80_program_instructions)/(sizeof(uint16_t)),
        .origin = -1,
    };


int sm_data_z80=-1;
void init_PIO_DATAZ80()
    {
        sm_data_z80=pio_claim_unused_sm(PIO_data_z80, true);;
    
        uint offset = pio_add_program(PIO_data_z80, &pio_dataz80_program);
    
        pio_sm_config c = pio_get_default_sm_config();
        sm_config_set_wrap(&c, offset, offset + (pio_dataz80_program.length-1)); 
        
        // sm_config_set_fifo_join(&c,PIO_FIFO_JOIN_TXPUT); 
        sm_config_set_in_shift(&c, false, false, 32); 
        sm_config_set_out_shift(&c, false, false, 32);
        sm_config_set_jmp_pin(&c,PIN_RD);
       

        //выходные пины data 
    
        for(int i=0;i<8;i++)
        {
            pio_gpio_init(PIO_data_z80, PIN_D0+i);
            gpio_set_drive_strength(PIN_D0+i,GPIO_DRIVE_STRENGTH_4MA);
            gpio_set_slew_rate(PIN_D0+i,GPIO_SLEW_RATE_FAST);
        }
    
        
       
        pio_sm_set_consecutive_pindirs(PIO_data_z80, sm_data_z80, PIN_D0, 8, true);//конфигурация пинов на выход
    
        sm_config_set_out_pin_base(&c, PIN_D0);
        sm_config_set_out_pin_count(&c,8);
    
        //side set пины
        pio_gpio_init(PIO_data_z80, PIN_DATA_CTRL);
    
        pio_sm_set_pindirs_with_mask(PIO_data_z80, sm_data_z80, (1<<(PIN_DATA_CTRL))|(0xff<<(PIN_D0)),(1<<(PIN_DATA_CTRL))|(0xff<<(PIN_D0)) );
    
        sm_config_set_sideset_pins(&c,PIN_DATA_CTRL);
        sm_config_set_sideset(&c,1,false,false);
    
       
        
    
    
    
    
    
    
    
    
    
        pio_sm_init(PIO_data_z80, sm_data_z80, offset, &c);
        pio_sm_set_enabled(PIO_data_z80, sm_data_z80, true);
         
        // float fdiv=1;
        // uint32_t fdiv32=(uint32_t) (fdiv * (1 << 16));
        // fdiv32=fdiv32&0xfffff000;//округление делителя
        // PIO_data_z80->sm[sm_data_z80].clkdiv=fdiv32; //делитель для конкретной sm
    
        PIO_data_z80->txf[sm_data_z80]=0x00ffffff;//запуск данных 
      
    
    }
    

    
    void __no_inline_not_in_flash_func(put_dataZ80)(uint8_t data)
    {
        PIO_data_z80->txf[sm_data_z80]=0x00ffffff|(data<<24);//запуск данных 
    };
    
    
