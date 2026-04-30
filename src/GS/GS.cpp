#include "GS.h"
#include <Arduino.h>

#if GS_EN

#include <hardware/structs/qmi.h>
#include <hardware/structs/xip.h>

#include "gs105b.rom.h"
//#include "gs105b.32K.rom.h"
//#include "gs_test.rom.h"

#define printf ;


#define PSRAM_LOCATION (0x11000000)  
#define PSRAM_SIZE (8 * 1024 * 1024)

 static uint8_t RAM_BANK1[ram_page-2][0x4000] PSRAM;
 static uint8_t RAM_BANK2[ram_page-2][0x4000] PSRAM;
//
// static uint8_t** RAM_BANK1=(uint8_t**)PSRAM_LOCATION;
// static uint8_t** RAM_BANK2=(uint8_t**)(PSRAM_LOCATION+(0x4000*(ram_page-2)));

//uint8_t RAM[0x4000] = {0};

//
//uint8_t RAM_BANK1[ram_page-2][0x4000] ;
//uint8_t RAM_BANK2[ram_page-2][0x4000] ;
uint8_t RAM_BANK1_1[2][0x4000] = { 0 };  //is PICO RAM
uint8_t RAM_BANK1_2[2][0x4000] = { 0 };


//uint8_t *ram_slot0 = RAM_BANK1_1[0]; //the first half of the zero ram page 0x0000-0x3FFF 0x4000
uint8_t *ram_slot1 = RAM_BANK1_2[1]; //the second half of the first ram page 0x4000-0xbFFF 0x4000
uint8_t *ram_slot2 = RAM_BANK1_1[0]; //custom ram page 0xc000-0x7fff is full page 0x8000-0xffff
uint8_t *ram_slot3 = RAM_BANK1_1[0]; //custom ram page 0x8000-0xFFFF 

uint8_t page = 0;
uint8_t slot0_is_ram = 0;
uint8_t slot23_is_ram = 0;

volatile uint8_t ZXDATWR;
volatile uint8_t GSCOM;
volatile uint8_t GSDAT;
volatile uint8_t GSCTR;
volatile uint8_t GSCFG0; //0F
volatile uint8_t GSSTAT;
volatile uint8_t ZXSTAT;
volatile uint8_t ZXCMD;
volatile uint8_t  volume1;
volatile uint8_t  volume2;
volatile uint8_t  volume3;
volatile uint8_t  volume4;
volatile uint8_t channel1;
volatile uint8_t channel2;
volatile uint8_t channel3;
volatile uint8_t channel4;


/*
Область адресного пространства Z80 Страницы физической памяти
$0000-$3FFF первая половина нулевой страницы
$4000-$7FFF вторая половина первой страницы
$8000-$FFFF произвольная страница памяти

Номер страницы памяти в области $8000-$FFFF задаётся в порту MPAG числом 
от 0 до 63.
При установленном бите B_NOROM в порту GSCFG0 во всех вышеперечисленных 
областях адресного пространства включено ОЗУ и речь идёт о страницах ОЗУ. Если же бит 
B_NOROM сброшен, то в областях $0000-$3FFF и $8000-$FFFF включены страницы ПЗУ, в то 
время как в области $4000-$7FFF остаётся вышеуказанная страница OЗУ.  При тёплом сбросе 
бит B_NOROM очищается, таким образом процессор стартует из ПЗУ.
При установке бита B_RAMRO в порту GSCFG0 нулевая страница ОЗУ защищается от 
записи в неё. Данный бит не влияет на возможность записи в нулевую страницу ПЗУ. 

B_NOROM		equ	0 ; =0 - there is ROM everywhere except 4000-7FFF, =1 - the RAM is all around
M_NOROM		equ	1

B_RAMRO		equ	1 ; =1 - ram absolute addresses 0000-7FFF (zeroth big page) are write-protected
M_RAMRO		equ	2
*/


void __time_critical_func(WrZ80)(register word address, const register byte value) {
    
    //if ((address < 0x4000) && (slot0_is_ram == 1)) {  
    //    ram_slot0[address]=value;
    //    return;
    //}
    if ((address > 0x3fff) && (address < 0x8000)) {
        //printf("Ram 4000-7fff write %x %x\n", address, value);
        ram_slot1[address&0x3fff]=value;
        return;
    }
    if ((address > 0x7fff) && (slot23_is_ram != 1)) {
        //printf("Rom 8000-ffff write block\n");  
        return;
    }
    else if ((address > 0x7fff) && (address < 0xc000)) {
        //printf("Ram slot 2 8000-bfff write 0x%X offset 0x%X value %x page %x\n", address, address&0x3fff, value, page-1);
        ram_slot2[address&0x3fff]=value;
        return;
    }
    else if ((address > 0xbfff) && (address <= 0xffff) ) {
        //printf("Ram slot 3 c000-ffff write 0x%X offset 0x%X value %x page %x\n", address, address&0x3fff, value, page-1);
        ram_slot3[address&0x3fff]=value;
        return;

    }
}
/*
Данные в каналы заносятся при чтении процессором ОЗУ по адресам  #6000
- #7FFF автоматически.
Данные для каналов должны находиться по следующим адресам:
╔═════════════╤═══════════════╗
║разряд адреса│  для канала   ║   #6000 - #60FF  - данные канала 1
║             ├───┬───┬───┬───╢   #6100 - #61FF  - данные канала 2
║             │ 1 │ 2 │ 3 │ 4 ║   #6200 - #62FF  - данные канала 3
╟─────────────┼───┼───┼───┼───╢   #6300 - #63FF  - данные канала 4
║  A0 - A7    │ X │ X │ X │ X ║   #6400 - #64FF  - данные канала 1
║             │   │   │   │   ║                  .
║     A8      │ 0 │ 1 │ 0 │ 1 ║                  .
║             │   │   │   │   ║                  .
║     A9      │ 0 │ 0 │ 1 │ 1 ║                  .
║             │   │   │   │   ║                  .
║  A10-A12    │ X │ X │ X │ X ║                  .
║             │   │   │   │   ║                  .
║  A13,A14    │ 1 │ 1 │ 1 │ 1 ║                  .
║             │   │   │   │   ║                  .
║    A15      │ 0 │ 0 │ 0 │ 0 ║   #7D00 - #7DFF  - данные канала 2
║             │   │   │   │   ║   #7E00 - #7EFF  - данные канала 3
╚═════════════╧═══╧═══╧═══╧═══╝   #7F00 - #7FFF  - данные канала 4

*/
byte __time_critical_func(RdZ80)(const register word address) {
    static int mix=1;
    if ((address < 0x4000) && (slot0_is_ram != 1)) {
        ///printf("Ram read 0000-4000 %x value %x \n", address, ram_slot2[address]);
        return gs105b_rom[address]; //gs105b_romgs_test_rom
    }
    //else if (address < 0x4000)
    //{   
    //    return ram_slot0[address];
    // }
    
    if ((address > 0x3fff) && (address < 0x8000)) {
        
        switch (address&0x6300){
            case 0x6300: 
                channel4 = (ram_slot1[address&0x3fff]);
                //channel4 = (int16_t)(((ram_slot1[address&0x3fff]-128) * volume4)>>8);
                //printf("GS set channel4 %d\n", channel4);
                //channel4=(int16_t)(ram_slot1[address&0x3fff]);
                //mixer_buffer[i] += ((source->samples[i+source->pos]-128) * source->volume) >> 8;
                
                //channel4 = (int16_t)((channel4) * volume4 + 32768);
                //printf("Convert channel4 %0d\n", channel4);
                //printf("Add volume %f to channel4 %d\n",volume4, channel4);
                break;
            case 0x6200: 
                channel3 = (ram_slot1[address&0x3fff]);
                //channel3 = (int16_t)(((ram_slot1[address&0x3fff]-128) * volume3)>>8);
                //printf("GS set channel3 %01x, %f\n", channel3, volume3);
                //printf("Convert channel3 %01x\n", channel3);
                //channel3 = channel3*volume1;
                //printf("Add volume to channel3 %01x\n", channel3);
                break;
            case 0x6100: 
                channel2 = (ram_slot1[address&0x3fff]);
                //channel2 = (int16_t)(((ram_slot1[address&0x3fff]-128) * volume2)>>8);
                //printf("GS set channel2 %01x, %f\n", channel2, volume2);
                //printf("Convert channel2 %01x\n", channel2);
                //channel2 = channel2*volume1;
                //printf("Add volume to channel2 %01x\n", channel2);
                break;
            case 0x6000: 
                channel1 = (ram_slot1[address&0x3fff]);
                //channel1 = (int16_t)(((ram_slot1[address&0x3fff]-128) * volume1)>>8);
                //printf("GS set channel1 %01x, %f\n", channel1, volume1);
                //printf("Convert channel1 %01x\n", channel1);
                //channel1 = channel1*volume1;
                //printf("Add volume to channel1 %01x\n", channel1);
                break;
            //channel3 = (int16_t)(channel3 - 0x80) << 8;
            //channel4 = (int16_t) (channel4 * volume4)/4
            //channel4 = (((2*(mix+(ram_slot1[address&0x3fff])))-((mix*(ram_slot1[address&0x3fff]))/128)-128))
            //(int16_t)(((2*(volume4+ram_slot1[address&0x3fff]))-((volume4*ram_slot1[address&0x3fff])/128)-128));
            //(int16_t)(ram_slot1[address&0x3fff])*2;
            //int16_t res = (int16_t)((int32_t) vol*(int32_t)data);
            //(((2*(mix+channel4))-((mix*channel4)/128)-128))
            //channel4 = (4*(volume4+channel4))-((volume4*channel4)/128);
        }   
        return ram_slot1[address&0x3fff];;
    }

    if ((address > 0x7fff) && (slot23_is_ram != 1)) {
        //if (address > 0xbfff) printf("Rom read 8000-ffff %x ROM adress %x\n", address, address%0x8000);   
        return gs105b_rom[address&0x7fff]; //gs105b_romgs_test_rom
    }
    else if ((address > 0x7fff) && (address < 0xc000)) {
        //printf("Ram read 8000-c000 %x %x value  %x \n", address, address%0x8000, ram_slot2[address%0x8000]);
        return ram_slot2[address&0x3fff];
    }
    else if ((address > 0xbfff) && (address <= 0xffff) ) {
        //printf("Ram read c000-ffff  %x %x value %x \n", address, address%0xc000, ram_slot3[address%0xc000]);
        return ram_slot3[address&0x3fff];
    }
    return 0;
}
/*
MPAG	equ	#00 ; write-only, Memory PAGe port (big pages at 8000-FFFF or small at 8000-BFFF)
MPAGEX	equ	#10 ; write-only, Memory PAGe EXtended (only small pages at C000-FFFF)

ZXCMD	equ	#01 ; read-only, ZX CoMmanD port: here is the byte written by ZX into GSCOM

ZXDATRD	equ	#02 ; read-only, ZX DATa ReaD: a byte written by ZX into GSDAT appears here;
		    ; upon reading this port, data bit is cleared

ZXDATWR	equ	#03 ; write-only, ZX DATa WRite: a byte written here is available for ZX in GSDAT;
		    ; upon writing here, data bit is set

ZXSTAT	equ	#04 ; read-only, read ZX STATus: command and data bits. positions are defined by *_CBIT and *_DBIT above

CLRCBIT	equ	#05 ; read-write, upon either reading or writing this port, the Command BIT is CLeaRed

B_CBIT	equ	0 ;Command position
M_CBIT	equ	1 ;  BIT and mask

B_DBIT	equ	7   ;Data position
M_DBIT	equ	#80 ; BIT and mask

VOL1	equ	#06 ; write-only, volumes for sound channels 1-8
VOL2	equ	#07
VOL3	equ	#08
VOL4	equ	#09
VOL5	equ	#16
VOL6	equ	#17
VOL7	equ	#18
VOL8	equ	#19

порт А
устанавливает бит D7 слова состояния не равным биту D0 порта 0

порт B
устанавливает бит D0 слова состояния равным биту D5 порта 6

*/


void __time_critical_func(OutZ80)(register word port, register byte value) {
    //printf("Z80 out port %02x value %02x\n", port & 0xff, value);
    //static int mix=1;
    switch (port & 0xff) {
        case 0x00:
            if ((value>1)&&(value<=(ram_page))){ 
//            if ((value>1)){               
                ram_slot2 = &RAM_BANK1[value-2][0];
                ram_slot3 = &RAM_BANK2[value-2][0];
                slot23_is_ram=1;


//                uint8_t temp1 = ram_slot3[10];
//                uint8_t temp2 = ram_slot2[10];
//                asm volatile("nop \n nop \n nop \n nop \n nop \n nop   ");


                //asm volatile("nop \n nop \n nop \n nop \n nop \n nop   ");
                //asm volatile("nop \n nop \n nop \n nop \n nop \n nop   ");
                //printf("Set page NR %02x\n", value);
                //printf("Set ramslot2 %02x ramslot3 %02x \n", &RAM_BANK1[value-1][0], &RAM_BANK2[value-1][0]);    
            } else {
                ram_slot2 = &RAM_BANK1_1[value&1][0];
                ram_slot3 = &RAM_BANK1_2[value&1][0];
                if (value==0) slot23_is_ram=0;
                    else slot23_is_ram=1;
                //printf("Set page NR %02x\n", value);
                //printf("Set ramslot2 %02x ramslot3 %02x \n", &RAM_BANK1[value-1][0], &RAM_BANK2[value-1][0]); 
            }
            break;
        case 0x03:      // ZXDATWR	equ	#03 ; write-only, ZX DATa WRite: a byte written here is available for ZX in GSDAT;
                        // upon writing here, data bit is set           
            //ZXDATRD=value;
            ZXDATWR=value; 
            //ZXSTAT |= DataBIT;
            GSSTAT |= DataBIT;
            //printf("GS set ZXDATWR %01x\n", ZXDATWR);           
            break;
        case 0x05: //CLRCBIT	equ	#05 ; read-write, upon either reading or writing this port, the Command BIT is CLeaRed
            //printf("GS write clear CommandBIT %01x\n", GSSTAT);
            
            GSSTAT &= ~CommandBIT;
            
            break; 
        case 0x06:  //Volume1
            volume1=value<<2;
            //mix=1;
			//volume1 = (2*(mix+value))-((mix*value)/128);
            //printf("GS set volume1 %01x, %01x, %01x\n", value, channel1, volume1);
            break; 
        case 0x07:  //Volume2
            volume2=value<<2;
            //channel2=(int16_t)channel2;//*value;
            //volume2 = (2*(mix+value))-((mix*value)/128);
            //printf("GS set volume2 %01x, %01x\n", value, channel2);
            break; 
        case 0x08:  //Volume3
            volume3=value<<2;
            //channel3=(int16_t)channel3;//*value;
            //volume3 = (2*(mix+value))-((mix*value)/128);
            //printf("GS set volume3 %01x, %01x\n", value, channel3);
            break; 
        case 0x09:  //Volume4
            volume4=value<<2;
            //channel4=(int16_t)channel4;//*value;
            //volume4 = (2*(mix+value))-((mix*value)/128);
            //printf("GS set volume1 %x, %f\n", value, volume4);
            break; 
            //case 0x0F: 
        //    printf("GS set GSCFG0 %01x\n", value); 
        //    
        //    GSSTAT &= ~CommandBIT;
        //    break; 
        case 0x10: 
            printf("GS set MPAGEX %01x\n", value); 

            break;
        case 0x0A: 
            printf("GS set 0A %01x\n", value); 

            break;
        case 0x0B: 
            printf("GS set 0B %01x\n", value); 

            break;

        default:
            break;
    }
}

byte __time_critical_func(InZ80)(register word port) {
    //printf("Z80 port in %02x\n", port & 0xff);   
    switch (port & 0xff) {
        // gg input
        case 0x01: //ZXCMD	equ	#01 ; read-only, ZX CoMmanD port: here is the byte written by ZX into GSCOM
            //printf("GS read GSCOM %01X\n", GSCOM);
            
            return GSCOM;
        case 0x02:  //ZXDATRD	equ	#02 ; read-only, ZX DATa ReaD: a byte written by ZX into GSDAT appears here;
                    //; upon reading this port, data bit is cleared
            //printf("GS read GSDAT %01X\n", GSDAT);
            
            
            GSSTAT  &= ~DataBIT; 
            
            return GSDAT;     
        case 0x04: //ZXSTAT	equ	#04 ; read-only, read ZX STATus: command and data bits. positions are defined by *_CBIT and *_DBIT above

            //printf("GS read GSSTAT %01X\n", GSSTAT);

            return GSSTAT;
        case 0x05:
            //printf("GS read clear CommandBIT in port %02x\n", port & 0xff);
            
            GSSTAT &= ~CommandBIT;
            return GSSTAT;
        case 0x00:
            return 0xff;
        }
    return 0xff;
}

void __no_inline_not_in_flash_func(PatchZ80)(register Z80 *R) {
}

word __no_inline_not_in_flash_func(LoopZ80)(register Z80 *R) {
    return INT_NONE;
}



static void __no_inline_not_in_flash_func(psram_init)(uint cs_pin) {
     
    gpio_set_function(cs_pin, GPIO_FUNC_XIP_CS1); // CS for PSRAM

    // Set PSRAM timing for APS6404
    //
    // Using an rxdelay equal to the divisor isn't enough when running the APS6404 close to 133MHz.
    // So: don't allow running at divisor 1 above 100MHz (because delay of 2 would be too late),
    // and add an extra 1 to the rxdelay if the divided clock is > 100MHz (i.e. sys clock > 200MHz).
    const int max_psram_freq = 166000000;
    const int clock_hz = 400000000;//clock_get_hz(clk_sys);
    int divisor = (clock_hz + max_psram_freq - 1) / max_psram_freq;
    if (divisor == 1 && clock_hz > 100000000) {
        divisor = 2;
    }
    int rxdelay = divisor;
    if (clock_hz / divisor > 100000000) {
        rxdelay += 1;
    }

    // - Max select must be <= 8us.  The value is given in multiples of 64 system clocks.
    // - Min deselect must be >= 18ns.  The value is given in system clock cycles - ceil(divisor / 2).
    const int clock_period_fs = 1000000000000000ll / clock_hz;
    const int max_select = (125 * 1000000) / clock_period_fs;  // 125 = 8000ns / 64
    const int min_deselect = (18 * 1000000 + (clock_period_fs - 1)) / clock_period_fs - (divisor + 1) / 2;

    qmi_hw->m[1].timing = 1 << QMI_M1_TIMING_COOLDOWN_LSB |
                          QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB |
                          max_select << QMI_M1_TIMING_MAX_SELECT_LSB |
                          min_deselect << QMI_M1_TIMING_MIN_DESELECT_LSB |
                          rxdelay << QMI_M1_TIMING_RXDELAY_LSB |
                          divisor << QMI_M1_TIMING_CLKDIV_LSB;


    xip_ctrl_hw->ctrl|=XIP_CTRL_WRITABLE_M1_BITS;

}





   
void GS_init()
{
    GSDAT=0;
    GSSTAT=0x7e;
    ZXDATWR=0;
    ZXSTAT=0;
    volume1=0x3f;
    volume2=0x3f;
    volume3=0x3f;
    volume4=0x3f;
    channel1=0x80;
    channel2=0x80;
    channel3=0x80;
    channel4=0x80;
    psram_init(SFE_RP2350_XIP_CSI_PIN);

    memset(RAM_BANK1_1, 0, sizeof(RAM_BANK1_1));
    memset(RAM_BANK1_2, 0, sizeof(RAM_BANK1_2));

}
/*
int16_t static inline (mix_sample)(int16_t const sample1, int16_t const sample2)
{
    int32_t mixed = (int32_t)sample1 + (int32_t)sample2;
    if(  32767 < mixed ){ return  32767; }
    if( -32768 > mixed ){ return -32768; }
    return (int16_t)(mixed & (int32_t)0xFFFF);
};

sound_LR __no_inline_not_in_flash_func(GS_get_sound_LR_sample)()
{
    sound_LR snd;
    snd.L=mix_sample((int16_t)(volume3*(channel3-128)),(int16_t)(volume4*(channel4-128)));
    snd.R=mix_sample((int16_t)(volume1*(channel1-128)),(int16_t)(volume2*(channel2-128)));
    return snd;
}; 
*/
int16_t static inline (mix_sample)(int16_t const sample1, int16_t const sample2, int16_t const sample3, int16_t const sample4)
{
    int32_t mixed = (int32_t)sample1 + (int32_t)sample2 + (int32_t)sample3 + (int32_t)sample4;
    if(  32767 < mixed ){ return  32767; }
    if( -32768 > mixed ){ return -32768; }
    return (int16_t)(mixed & (int32_t)0xFFFF);
};

sound_LR __no_inline_not_in_flash_func(GS_get_sound_LR_sample)()
{
	int8_t div_mix = 3;
	int16_t vmax = 15;
    sound_LR snd;
    snd.L=mix_sample((int16_t) ((volume3*10/vmax)*(channel3-128)),
					 (int16_t) ((volume4*10/vmax)*(channel4-128)),
					 (int16_t)(((volume1*10/vmax)/div_mix)*(channel1-128)),
					 (int16_t)(((volume2*10/vmax)/div_mix)*(channel2-128)));
    snd.R=mix_sample((int16_t) ((volume1*10/vmax)*(channel1-128)),
					 (int16_t) ((volume2*10/vmax)*(channel2-128)),
					 (int16_t)(((volume3*10/vmax)/div_mix)*(channel3-128)),
					 (int16_t)(((volume4*10/vmax)/div_mix)*(channel4-128)));
    return snd;
};

Z80 cpu;
#endif
