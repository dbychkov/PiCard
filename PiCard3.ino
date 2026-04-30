#include <Arduino.h>
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/spi.h"
#include "hardware/timer.h"



#include "src/PIO/pio_data_z80_bus.h"



#include "config.h"

#include "src/i2c/i2c_joy.h"

#include "src/i2s/i2s.h"

#if HW_UART_EN
  #include "src/UART/uart.h"
  static bool is_status_HW_UART_reg=false;
  static bool is_data_HW_UART_reg=true;
  
  #define ZXUNO_ADDR  0xFC3B
  #define ZXUNO_REG   0xFD3B
#endif

#if TEST_ROM_EN 

  #include "ROMS/testrom.h"
#endif

#if KB_PS2_EN
  #include "src/ps2/PS2_PIO.h"
  #include "src/zx_util/zx_kb.h"
  static zx_kb_state_t zx_kb_state;
#endif

#if TS_EN
  #include "src/TS/ts.h"
#endif


#if DIVMMC_EN
  #include "ROMS/ROM_DIVMMC.h"
  static uint8_t divMMC_RAM[16][8192];

#endif

#if GS_EN
  #include "src/GS/GS.h"
#endif

static void  in_init(uint pin)
{

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_set_pulls(pin,true,false);
    // gpio_set_input_hysteresis_enabled(pin, true);
    // gpio_pull_down(pin);     
};

void out_init(uint pin,bool val=0)
{

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT); 
    gpio_put(pin,val);    
};

static inline uint8_t READ_SD_BYTE()
{
    uint8_t dataSPI=spi_get_hw(SD_SPI)->dr;
    spi_get_hw(SD_SPI)->dr=0xff;  
    return   dataSPI;
}

static inline void  WRITE_SD_BYTE(uint8_t data) 
{
    volatile uint8_t dataSPI=spi_get_hw(SD_SPI)->dr;
    spi_get_hw(SD_SPI)->dr=data; 

}


#define ROM_LOCK gpio_put(PIN_ROM_BLK,0);
#define ROM_UNLOCK gpio_put(PIN_ROM_BLK,1);
static void reset_spectrum()
{
    ROM_UNLOCK;
    watchdog_update();
    out_init(PIN_RESET,1);
    for(int i=10;i--;)
    {
      watchdog_update();
      busy_wait_ms(50);
    }
        
#if TS_EN
    ts.reset() ;   
#endif
    out_init(PIN_RESET,0);

}

inline void NMI_press()
{
  if (gpio_get_dir(PIN_NMI)) return;

  gpio_put(PIN_NMI,0); 
  gpio_set_dir(PIN_NMI, GPIO_OUT); 
  watchdog_update();
  busy_wait_ms(50);
  gpio_set_dir(PIN_NMI, GPIO_IN); 




}

static bool divmmc_not_ZC_SD=true;
static uint8_t kempston_state=0xff;
static bool kempston_port_enable=true;  //флаг включения/отключения опроса порта 0х1F   TRUE - порт опрашивается

#if DENDY_JOY_3_PINS&&KEMPSTON_EN
static uint8_t nes_to_kempston_convert[256];
void inline DENDY_JOY_3_PINS_PROC()
{
    static int inx_clk=-6;
    static uint8_t data_out=0;
    inx_clk++;
    if (inx_clk>=0)
    {
        switch (inx_clk)
        {
        case 0:
            gpio_put(PIN_LATCH_DENDY,0);
            break;
        case 4 ... 19:
            {

              if ((inx_clk&1)==0)  {data_out<<=1; data_out|=gpio_get(PIN_DATA_DENDY);};
              gpio_put(PIN_CLK_DENDY,inx_clk&1);
              
            }
            break;
        case 20:
            gpio_put(PIN_CLK_DENDY,inx_clk&1); 
            kempston_state=nes_to_kempston_convert[data_out];
            break;
        case 22:
            inx_clk=-6;
            data_out=0;  
            gpio_put(PIN_LATCH_DENDY,1);
            break;
        default:
            break;
        }

    }
}                   
#endif

static sound_LR snd_out_GS;
static sound_LR snd_out_TS;
static sound_LR snd_out_BEEPER;
static sound_LR snd_out_SND_DRIVE1;
static sound_LR snd_out_SND_DRIVE2;
// #define SIGN(x) (((x)==0)?0:(((x)>0)?1:-1))
#define SIGN_F(x) (((x) > 0) - ((x) < 0))
static void __no_inline_not_in_flash_func(mix_sound_out)()
{
    

    static int32_t snd_L_old=0,snd_R_old=0;
    static int32_t snd_L_out=0,snd_R_out=0;
    static int32_t snd_L_in=0,snd_R_in=0;
    static int32_t snd_L_C=0,snd_R_C=0;
    
    volatile int32_t snd_L=0, snd_R=0;
    //микшер 


  #if GS_EN

    snd_L+=snd_out_GS.L;
    snd_R+=snd_out_GS.R;
  #endif
  
  #if TS_EN

    snd_L+=snd_out_TS.L;
    snd_R+=snd_out_TS.R;
  #endif

  #if BEEPER_EN
    snd_L+=snd_out_BEEPER.L;
    snd_R+=snd_out_BEEPER.R;
  #endif

  #if COVOX_EN || SND_DRIVE_EN
    snd_L+=snd_out_SND_DRIVE1.L;
    snd_R+=snd_out_SND_DRIVE1.R;
  #endif

  #if SND_DRIVE_EN
    snd_L+=snd_out_SND_DRIVE2.L;
    snd_R+=snd_out_SND_DRIVE2.R;
  #endif

    //DC блокер
    // const int32_t kf=1;
    
    // int32_t dL=(snd_L-snd_L_old);
    // int32_t dR=(snd_R-snd_R_old);
    

    // snd_L_in+=dL/1+0*dL/4;//делитель определяет частоту среза ФНЧ
    // snd_R_in+=dR/1+0*dR/4;//если >1, то ФВЧ

 

    // snd_L_old=snd_L_in;
    // snd_R_old=snd_R_in;
    
    snd_L_in=snd_L;
    snd_R_in=snd_R;

    

    snd_L_C+=SIGN_F(snd_L_in-snd_L_C);
    snd_R_C+=SIGN_F(snd_R_in-snd_R_C);

    snd_L_out=(snd_L_in-snd_L_C);
    snd_R_out=(snd_R_in-snd_R_C);




    //  //ограничитель
    int16_t snd_L_out_lim;
    int16_t snd_R_out_lim;



//простой ограничитель
    // snd_L_out_lim=(snd_L_out>32767)?32767:snd_L_out;
    // snd_L_out_lim=(snd_L_out<-32768)?-32768:snd_L_out;


    // snd_R_out_lim=(snd_R_out>32767)?32767:snd_R_out;
    // snd_R_out_lim=(snd_R_out<-32768)?-32768:snd_R_out;


//"Мягкий" ограничитель
  //точка перегиба
   #define P_LIN (20000) 

   //максимум
   #define P_MAX (32760)
  
   if (snd_L_out>P_LIN) snd_L_out_lim=(snd_L_out*P_MAX-P_LIN*P_LIN)/(snd_L_out-2*P_LIN+P_MAX);
   else if  (snd_L_out<-P_LIN) snd_L_out_lim=(-snd_L_out*P_MAX-P_LIN*P_LIN)/(snd_L_out+2*P_LIN-P_MAX);
        else  snd_L_out_lim=snd_L_out;


   if (snd_R_out>P_LIN) snd_R_out_lim=(snd_R_out*P_MAX-P_LIN*P_LIN)/(snd_R_out-2*P_LIN+P_MAX);
   else if  (snd_R_out<-P_LIN) snd_R_out_lim=(-snd_R_out*P_MAX-P_LIN*P_LIN)/(snd_R_out+2*P_LIN-P_MAX);
        else  snd_R_out_lim=snd_R_out;
         //ограничитель
    

    // snd_L_out=(snd_L_out>32767)?32767:snd_L_out;
    // snd_L_out=(snd_L_out<-32768)?-32768:snd_L_out;


    // snd_R_out=(snd_R_out>32767)?32767:snd_R_out;
    // snd_R_out=(snd_R_out<-32768)?-32768:snd_R_out;

    // int16_t snd_L_out_lim=snd_L_out;
    // int16_t snd_R_out_lim=snd_R_out;

    //вывод данных
    i2s_out((int16_t)snd_L_out_lim,(int16_t)snd_R_out_lim);
    

    // snd_L_out*=0.9995;
    // snd_R_out*=0.9995;
    
    //удержание возле 0
    // static int inx=0;
    // if (inx++<10) return;
    //const int k=16000;
    // snd_L_out-=SIGN(snd_L_out);
    // snd_R_out-=SIGN(snd_R_out);


    // snd_L_out-=(snd_L_out>0)?(1):0;
    // snd_R_out-=(snd_R_out>0)?(1):0;

    // snd_L_out+=(snd_L_out<0)?(1):0;
    // snd_R_out+=(snd_R_out<0)?(1):0;

    //  inx=0;

//     const int maxN_off_out=100000;
// #define EQ_SND(x,y) (((x.L-y.L)==0)&&((x.R-y.R)==0))



    
//     int32_t snd_L,snd_R;
//     snd_L=0;snd_R=0;

//   #if GS_EN
//     static sound_LR snd_out_GS_next;
//     static uint32_t count_EQ_GS_out=0;

//     if EQ_SND(snd_out_GS_next,snd_out_GS) count_EQ_GS_out++; else count_EQ_GS_out=0; 
//     if (count_EQ_GS_out<maxN_off_out) {snd_L+=snd_out_GS.L;snd_R+=snd_out_GS.R;} 
//     snd_out_GS_next=snd_out_GS;

//   #endif

//   #if BEEPER_EN
//     static sound_LR snd_out_BEEPER_next;
//     static uint32_t count_EQ_BEEPER_out=0;

//     if EQ_SND(snd_out_BEEPER_next,snd_out_BEEPER) count_EQ_BEEPER_out++; else count_EQ_BEEPER_out=0; 
//     if (count_EQ_BEEPER_out<maxN_off_out) {snd_L+=snd_out_BEEPER.L;snd_R+=snd_out_BEEPER.R;}; 
//     snd_out_BEEPER_next=snd_out_BEEPER;

//   #endif

//   #if COVOX_EN || SND_DRIVE_EN
//     static sound_LR snd_out_SND_DRIVE1_next;
//     static uint32_t count_EQ_SND_DRIVE_out=0;

//     if EQ_SND(snd_out_SND_DRIVE1_next,snd_out_SND_DRIVE1) count_EQ_SND_DRIVE_out++; else count_EQ_SND_DRIVE_out=0; 
//     if (count_EQ_SND_DRIVE_out<maxN_off_out) {snd_L+=snd_out_SND_DRIVE1.L;snd_R+=snd_out_SND_DRIVE1.R;}; 
//     snd_out_SND_DRIVE1_next=snd_out_SND_DRIVE1;

//   #endif

//   #if SND_DRIVE_EN

//     static sound_LR snd_out_SND_DRIVE2_next;
//     // static uint32_t count_EQ_SND_DRIVE2_out=0;

//     if EQ_SND(snd_out_SND_DRIVE2_next,snd_out_SND_DRIVE1) count_EQ_SND_DRIVE_out++; else count_EQ_SND_DRIVE_out=0; 
//     if (count_EQ_SND_DRIVE_out<maxN_off_out) {snd_L+=snd_out_SND_DRIVE2.L;snd_R+=snd_out_SND_DRIVE2.R;}; 
//     snd_out_SND_DRIVE2_next=snd_out_SND_DRIVE2;

//   #endif
    
//     //ограничитель
//     snd_L=(snd_L>32767)?32767:snd_L;
//     snd_L=(snd_L<-32768)?-32768:snd_L;


//     snd_R=(snd_R>32767)?32767:snd_R;
//     snd_R=(snd_R<-32768)?-32768:snd_R;


//     //вывод данных
//     i2s_out(snd_L,snd_R);

} 
void __no_inline_not_in_flash_func(ZX_bus_task)()
{
#if SD_CARD_EN
    spi_init(SD_SPI, SD_SPI_FREQ_CLK);
    gpio_set_function(SD_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_pulls(SD_SPI_TX_PIN,true,false);
    gpio_set_pulls(SD_SPI_RX_PIN,true,false);
    gpio_set_function(SD_SPI_CLK_PIN, GPIO_FUNC_SPI);
    out_init(SD_SPI_CS0_PIN,1);
    bool is_SD_active=false;
    divmmc_not_ZC_SD=!(ZC_SD_EN);
    divmmc_not_ZC_SD=DIVMMC_EN;
#endif
  #if DENDY_JOY_3_PINS&&KEMPSTON_EN
      for(int nes_keys=0;nes_keys<256;nes_keys++)
      {
              uint8_t data_kmpst=(nes_keys&0x0f)|((nes_keys>>2)&0x30)|((nes_keys<<3)&0x80)|((nes_keys<<1)&0x40);
              data_kmpst^=0xff;
              nes_to_kempston_convert[nes_keys]=data_kmpst;
      }
      in_init(PIN_DATA_DENDY);
      out_init(PIN_LATCH_DENDY,0);
      out_init(PIN_CLK_DENDY,0);
  
  
  #endif

#if DIVMMC_EN
     //переменные дивммс
     uint8_t* divMMC_pageROM=(uint8_t*)&ROM_DIVMMC[0];
     uint8_t* divMMC_pageRAM=(uint8_t*)divMMC_RAM[0];
    //  uint32_t div_pageRAM_inx=0;
     bool divMMC_ON=false;
     bool divMMC_ON_PORT=false;
     bool divMMC_SW=false;



#endif 
#if (DIVMMC_EN)&&(ZC_SD_EN)
   in_init(DIV_OFF_PIN);
#endif



    for(int pin=PIN_A0;pin<(16+PIN_A0);pin++) in_init(pin);
    in_init(PIN_M1);
    in_init(PIN_RD);
    in_init(PIN_WR);
    in_init(PIN_IORQ);
    in_init(PIN_MREQ);



    out_init(PIN_NMI,1);
    // out_init(PIN_RESET,1);
    out_init(PIN_ROM_BLK, 1);
  #if (DIVMMC_EN)
    memset((uint8_t*)divMMC_RAM[0],0,16*8192);
  #endif
    // busy_wait_ms(300);    
    init_PIO_DATAZ80();
    // gpio_put(PIN_RESET,0);  
    ROM_UNLOCK;


    bool is_new_cmd=true;
    for(;;)
      {
          uint32_t datain32=gpio_get_all();

        //ждём устаканивания данных на шине
          // uint32_t inx=2;
          // while(inx--)
          // {
          //   if  (datain32!=gpio_get_all()) {datain32=gpio_get_all();inx=2;};
            

          // }
          

         if ((datain32&(IORQ_Z80|MREQ_Z80))==(IORQ_Z80|MREQ_Z80)) { is_new_cmd=true;continue;};
         if ((datain32&(RD_Z80|WR_Z80))==(RD_Z80|WR_Z80)) {  is_new_cmd=true;continue;}; //ждём сигнала чтения или записи на шине
         if (!is_new_cmd) continue;





         uint32_t  io_mrq_rd_wr=(((~datain32))&(MREQ_Z80|IORQ_Z80|RD_Z80|WR_Z80));



         switch (io_mrq_rd_wr)
          {
            //запись в порт
            case (IORQ_Z80|WR_Z80):
              {
              if (!is_new_cmd) break; is_new_cmd=false;
              uint16_t addr16=(datain32>>PIN_A0);
              uint8_t addr8=addr16&0xff;

              // uint8_t d8=(uint32_t)datain32>>PIN_D0;  
              uint8_t d8=gpio_get_all()>>PIN_D0;

              

              switch (addr8)
                {
                 #if GS_EN
                  case 0x33: // GSCTR   EQU #33 ; write-only, control register for NGS: constants available given immediately below
                        /*
                        C_GRST	equ	#80 ; reset constant to be written into GSCTR
                        C_GNMI	equ	#40 ; NMI constant to be written into GSCTR
                        C_GLED	equ	#20 ; LED toggle constant
                        */
                        GSCTR = d8;
                        break;
                    
                   

                  case 0xB3: // GSDAT   EQU #B3 ; read-write, data transfer register for NGS
                         GSDAT = d8;
                         //printf("ZX set GSDAT %01X\n", GSDAT);
                         //printf("GSSTAT is %01X\n", GSSTAT);
                         GSSTAT |= DataBIT;
                         break;
                  case 0xBB: // GSCOM   EQU #BB ;write-only, command for NGS
                         GSCOM = d8;
                         //printf("ZX set GSCOM %01X\n", GSCOM);
                         //printf("GSSTAT is %01X\n", GSSTAT);
                         GSSTAT |= CommandBIT;
                         break;
                #endif

                #if ZC_SD_EN
                  case 0x57://данные SD карты
                        if (divmmc_not_ZC_SD) break;
                        if (is_SD_active)
                         {
                            WRITE_SD_BYTE(d8);
                         }
                         
                        break;
                  case 0x77://управление SD
                        if (divmmc_not_ZC_SD) break;
                        is_SD_active=((d8&1)==1);
                        gpio_put(SD_SPI_CS0_PIN,d8&2);
                        break;
                #endif


                #if DIVMMC_EN

                  case 0xe3:
                        if (!divmmc_not_ZC_SD) break; 
                        divMMC_ON_PORT=((d8&0x80)==0x80);
                        gpio_put(PIN_ROM_BLK,!((divMMC_ON)||(divMMC_ON_PORT))); 
                        //управление NMI
                        if ((divMMC_ON)||(divMMC_ON_PORT))  {gpio_put(PIN_NMI,1); gpio_set_dir(PIN_NMI, GPIO_OUT); }   else   gpio_set_dir(PIN_NMI, GPIO_IN);
                        divMMC_pageRAM=(uint8_t*)divMMC_RAM[(d8&0xf)];
                        break;
                    
                  case 0xe7://  
                        if (!divmmc_not_ZC_SD) break; 
                        is_SD_active=(d8&1)==0;
                        gpio_put(SD_SPI_CS0_PIN,d8&1);
                        break;
                         
                  case 0xeb://данные SD карты
                        if (!divmmc_not_ZC_SD) break; 
                        if (is_SD_active)
                         {
                            WRITE_SD_BYTE(d8);
                         }
                         
                        break;
                #endif

                // #if HW_UART_EN
                //       case 0xc6: WRITE_UART(d8); break;
                // #endif

                #if COVOX_EN
                  case 0xfb:
                    snd_out_SND_DRIVE1.L=((d8*128)-16000);
                    snd_out_SND_DRIVE1.R=snd_out_SND_DRIVE1.L;
                      mix_sound_out();
                  break;
                #endif

                #if SND_DRIVE_EN
                  case 0x0f:
                    snd_out_SND_DRIVE1.L=((d8*64)-8000);
                    mix_sound_out();
                  break;

                  case 0x1f:
                    snd_out_SND_DRIVE2.L=((d8*64)-8000);
                    mix_sound_out();
                  break;
                  
                  case 0x4f:
                    snd_out_SND_DRIVE1.R=((d8*64)-8000);
                    mix_sound_out();
                  break;
                  
                  case 0x5f:
                    snd_out_SND_DRIVE2.R=((d8*64)-8000);
                    mix_sound_out();
                  break;
                #endif

                };
                
                // WRITE_UART(addr16&0xff);//test
                // WRITE_UART((addr16>>8)&0xff);//test


                

                #if BEEPER_EN
                  if ((addr16&1)==0)
                  {
                    snd_out_BEEPER.L=(((d8&0x18)*1024)-8000)/2;
                    snd_out_BEEPER.R=snd_out_BEEPER.L;
                    mix_sound_out();
                    break;
                  }
                #endif

                #if TS_EN
                  if((addr16&0xc003)==(0xfffd&0xc003)) ts.select_reg(d8);
                  if((addr16&0xc003)==(0xbffd&0xc003)) ts.set_reg(d8);
                #endif

                #if HW_UART_EN
                    // if (((divMMC_ON)||(divMMC_ON_PORT))==false)
                    {
                    if((addr16)==ZXUNO_ADDR)
                      {
                        //выбор регистра
                        is_status_HW_UART_reg=false; is_data_HW_UART_reg=false;
                        
                        if (d8==0xc7)  { is_status_HW_UART_reg=true; is_data_HW_UART_reg=false;};
                        if (d8==0xc6)  { is_data_HW_UART_reg=true; is_status_HW_UART_reg=false; };
                        
                        break;
                        
                      };
                    if((addr16)==ZXUNO_REG)
                      {
                          if (is_data_HW_UART_reg==true) WRITE_UART(d8); 
                          break; 
                        
                      };
                    };
                    // if((addr16&0xC1FF)==(0xF8EF&0xC1FF)) WRITE_UART(d8);    
              #endif


              };
              break;
            //чтение  порта
            case (IORQ_Z80|RD_Z80):
              {
              if (!is_new_cmd) break; is_new_cmd=false;
              uint16_t addr16=(datain32>>PIN_A0);
              uint8_t addr8=addr16&0xff;

              

              switch (addr8)
                {

                  #if ZC_SD_EN
                    case 0x57://данные SD карты
                        if (divmmc_not_ZC_SD) break;
                        if (is_SD_active) put_dataZ80(READ_SD_BYTE()); 
                        break;
                    case 0x77://управление SD
                        if (divmmc_not_ZC_SD) break;
                        if (is_SD_active) put_dataZ80(0xfc);
                        break;
                  #endif

                  #if GS_EN
                    case 0xb3:
                            put_dataZ80(ZXDATWR);
                            GSSTAT &= ~DataBIT; 

                        break;
                    case 0xbb:
                            put_dataZ80(GSSTAT);
                        break;
                  #endif

                  #if DIVMMC_EN     
                    case 0xeb://данные SD карты
                          if (!divmmc_not_ZC_SD) break; 
                          if (is_SD_active)
                          {
                             put_dataZ80(READ_SD_BYTE()); 
                          }
                         
                        break;
                  #endif


                   

                    #if DENDY_JOY_3_PINS&&KEMPSTON_EN
                      case 0x1f: //кемпстон джой
                            if ((kempston_port_enable)&(((divMMC_ON)||(divMMC_ON_PORT))==false)) put_dataZ80(kempston_state); 
                            break;
                    #endif
                    #if JOY_I2C&&KEMPSTON_EN
                      case 0x1f: //кемпстон джой
                            if ((kempston_port_enable)&(((divMMC_ON)||(divMMC_ON_PORT))==false)) put_dataZ80(get_joy_data()); 
                            break;
                    
                    
                    #endif

                    // #if HW_UART_EN
                    //     case 0xc6: put_dataZ80(READ_UART()); break;
                    //     case 0xc7: put_dataZ80(get_UART_status());break;
                    // #endif
                  



                };
                
                #if KB_PS2_EN
                  if((addr16&1)==(0))
                  {
                    if ((zx_kb_state.u[0])||(zx_kb_state.u[1]))
                    {
                        uint8_t* kb_data=(uint8_t*)(zx_kb_state.u);
                        uint8_t addrh=addr16>>8;
                        uint8_t d8=0;

                        if ((addrh&0x01)==0) d8|=kb_data[0];
                        if ((addrh&0x02)==0) d8|=kb_data[1];
                        if ((addrh&0x04)==0) d8|=kb_data[2];
                        if ((addrh&0x08)==0) d8|=kb_data[3];
                        if ((addrh&0x10)==0) d8|=kb_data[4];
                        if ((addrh&0x20)==0) d8|=kb_data[5];
                        if ((addrh&0x40)==0) d8|=kb_data[6];
                        if ((addrh&0x80)==0) d8|=kb_data[7];
                        d8=~d8;
                        put_dataZ80(d8);


                        break;
                    }
                        
                  }
                #endif
                #if TS_EN
                  if((addr16&0xc003)==(0xfffd&0xc003)) {put_dataZ80(ts.get_reg());break;};
                #endif

                #if HW_UART_EN
                    
                    // if (((divMMC_ON)||(divMMC_ON_PORT))==false)
                    if((addr16)==ZXUNO_REG)
                      {
                          if (is_status_HW_UART_reg==true) put_dataZ80(get_UART_status());
                          if (is_data_HW_UART_reg==true)  put_dataZ80(READ_UART());
                          break;
                      }
                    // if((addr16&0xC1FF)==(0xF8EF&0xC1FF)) put_dataZ80(READ_UART());
                    // if((addr16&0xC1FF)==(0xF8EF&0xC1FF)) put_dataZ80(get_UART_status());
                    
              #endif
                

              };
              break;
            //запись в память
            case (MREQ_Z80|WR_Z80):
              {
              if (!is_new_cmd) break; is_new_cmd=false;
              
              #if TEST_ROM_EN
                break;
              #endif

              // uint8_t d8=(uint32_t)datain32>>PIN_D0;
              uint8_t d8=gpio_get_all()>>PIN_D0;

              uint16_t addr16=((uint32_t)datain32>>PIN_A0);

              #if DIVMMC_EN     
  
                    if (((addr16&0xe000)==0x2000)&&((divMMC_ON)||(divMMC_ON_PORT))&&(divmmc_not_ZC_SD)) 
                
                    { 
                        divMMC_pageRAM[(addr16&0x1fff)]=d8;  
                    }
              
              #endif

              };
              break;

            //чтение памяти
            case (MREQ_Z80|RD_Z80):
              {
              if (!is_new_cmd) break; is_new_cmd=false;
              uint16_t addr16=(datain32>>PIN_A0);

                if(addr16<0x3FFF)
                kempston_port_enable=false;
                else
                kempston_port_enable=true;


              #if TEST_ROM_EN
                    ROM_LOCK;
                    if ((addr16&0xc000)!=0) continue;
                    put_dataZ80(TEST_ROM[addr16]); 
                    break;
              #endif

              #if DIVMMC_EN


                if (((addr16&0xc000)!=0)) continue;
                if (!divmmc_not_ZC_SD) {ROM_UNLOCK;continue;};
                gpio_put(PIN_ROM_BLK,!((divMMC_ON)||(divMMC_ON_PORT)));
                if ((datain32&M1_Z80)==0) 
                      {
                          switch (addr16)
                          {
                              case 0x0000:
                              case 0x0008:
                              case 0x0038:
                              case 0x0066:
                              case 0x04c6:
                              case 0x0562:
                                  if (!divMMC_ON) divMMC_SW=true;//точки входа
                                  break;
                              case 0x3d00 ... 0x3dff:
                                  divMMC_ON=true; 
//                                  kempston_port_enable=false;//точки входа BDI
                                  break;
                              case 0x1ff8 ... 0x1fff: 
                                  if (divMMC_ON) divMMC_SW=true;//точки выхода
                                  break;
                              default:
 //                                 kempston_port_enable=true;
                                  break;
                          }

                      }
                if ((divMMC_ON)||(divMMC_ON_PORT))
                    {
                    
                        ROM_LOCK;
                        uint8_t data_out=(addr16&0x2000)?divMMC_pageRAM[addr16&0x1fff]:divMMC_pageROM[addr16];  
                        put_dataZ80(data_out);                
                    } 

                    
                if (divMMC_SW) {divMMC_SW=false; divMMC_ON=!divMMC_ON;}   ;

                //управление NMI
                if ((divMMC_ON)||(divMMC_ON_PORT))  {gpio_put(PIN_NMI,1); gpio_set_dir(PIN_NMI, GPIO_OUT); }   else   gpio_set_dir(PIN_NMI, GPIO_IN);

                
                

              #endif


              };
              break;
            default:
              is_new_cmd=true;
              break;

             
          }

          
      }

}


static inline void check_div_status()
{ 
  #if (DIVMMC_EN)&&(ZC_SD_EN)
  static bool div_off=false;
  if (div_off!=gpio_get(DIV_OFF_PIN))
  {    
      ROM_UNLOCK;
  
      div_off=gpio_get(DIV_OFF_PIN);

      divmmc_not_ZC_SD=!div_off;
      reset_spectrum();
  };
  #else
      #if (DIVMMC_EN) 
        divmmc_not_ZC_SD=true;
      #endif

      #if (ZC_SD_EN) 
        divmmc_not_ZC_SD=false;
      #endif

  #endif
};



int16_t sintable[256];



int __no_inline_not_in_flash_func(main)() {

  out_init(PIN_NMI,1);
  out_init(PIN_RESET,1);

  watchdog_enable(500, 1);
  vreg_disable_voltage_limit();
  vreg_set_voltage(VREG_VOLTAGE_1_60);
  busy_wait_ms(10);
  set_sys_clock_hz(CPU_FREQ, 1);
  busy_wait_ms(10);

  for(int i=0;i<256;i++)
  {
    sintable[i]=32700*sin(i*2*PI/256.0);
  }




#if JOY_I2C&&KEMPSTON_EN
    init_i2c_joy();
#endif
  busy_wait_ms(10);




#if GS_EN
  GS_init();
  ResetZ80(&cpu);
#endif
#if HW_UART_EN
  init_HW_UART();
#endif

  multicore_launch_core1(ZX_bus_task); 
  
  watchdog_update();

  // reset_z80();
  busy_wait_ms(100);

  divmmc_not_ZC_SD=true;
  
  i2s_init();
  uint64_t int_tick=time_us_64()+26;




#if KB_PS2_EN
  init_PS2();
#endif
gpio_put(PIN_RESET,0);
  uint32_t tst_inx=0;
  uint32_t odd_inx=0;
  while(1)
  {
    // continue;//test
    uint64_t tick_time = time_us_64();


    if (tick_time>=int_tick)
    {    
      //if ((tst_inx++)&(1<<7)) {snd_out.L=10000;snd_out.R=10000;} else {snd_out.L=-10000;snd_out.R=-10000;};

      //i2s_out(sintable[(tst_inx*3)%256], sintable[((tst_inx++)*7)%256]);
      // i2s_out(snd_out_GS.L,snd_out_GS.R);
  #if GS_EN|TS_EN
      mix_sound_out();
  #endif   
      // int_tick=tick_time+27;
      // int_tick+=27; //STEP TS = 6
      //  int_tick+=14;//STEP TS = 3

      int_tick+=9;//STEP TS = 2
  #if TS_EN
      ts.step();
      snd_out_TS=ts.get_LR_sample();
  #endif

  if ((++odd_inx%3)!=0) 
    { 
  #if KB_PS2_EN
    static uint32_t inx_ps2=0;
    static kb_u_state_t ps2_kb_state;
    static bool is_decode_ps2_old=false;
      if (odd_inx%3==1)
         if ((inx_ps2++&0x3f)==0)
        {                 
          
           bool is_decode_ps2=decode_PS2();
           bool newkey=(is_decode_ps2_old!=is_decode_ps2)&&(is_decode_ps2==false);

           is_decode_ps2_old=is_decode_ps2;
            
          if (newkey) 
            {
                 
              kb_state_t kb_state1=get_PS2_kb_state();
              ps2_kb_state.old_state=ps2_kb_state.new_state;
              ps2_kb_state.new_state=kb_state1;
              if ((IS_ANY_PRESS(&ps2_kb_state))||(IS_ANY_RELEASE(&ps2_kb_state)))
                {
                   set_zx_kb_state(&zx_kb_state,&kb_state1); 
                   if (IS_ANY_ACTIVE(&ps2_kb_state,KEY_R_CTRL,KEY_L_CTRL)&&IS_ANY_ACTIVE(&ps2_kb_state,KEY_R_ALT,KEY_L_ALT)&&IS_ANY_ACTIVE(&ps2_kb_state,KEY_DELETE)) reset_spectrum();
                   if IS_PRESS(&ps2_kb_state,KEY_INSERT) NMI_press();
                   if IS_PRESS(&ps2_kb_state,KEY_F12) while(1);;//reboot


                }
            }

        } 

  #endif
      continue;  
    }

      watchdog_update();
  


  #if GS_EN

      // IntZ80(&cpu, INT_IRQ);
      // ExecZ80(&cpu, 320);
      // snd_out=GS_get_sound_LR_sample();
      // i2s_out(snd_out.L,snd_out.R);
      
        snd_out_GS=GS_get_sound_LR_sample();
        IntZ80(&cpu, INT_IRQ);
      
      // ExecZ80(&cpu, 320);
  #endif

 

      check_div_status();


  
    #if DENDY_JOY_3_PINS&&KEMPSTON_EN
        DENDY_JOY_3_PINS_PROC();
    #endif        
    #if JOY_I2C&&KEMPSTON_EN
        joy_proc();
    #endif    
    }
    else
    {
  #if GS_EN
      int32_t dt=int_tick-tick_time;
      dt=(dt>0)?dt:1;
      ExecZ80(&cpu, dt*12);
      //  ExecZ80(&cpu, 12);
  #else 
    asm volatile ("nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n ");
  #endif

    ;

    }

  #if GS_EN 
   
        if (GSCTR & 0x40 == 0x40  ) {
            IntZ80(&cpu, INT_NMI);
            GSCTR &= ~0x40;
        } else if ( GSCTR & (1 << 7)) {
            ResetZ80(&cpu);
            GSCTR &= ~0x80;
        }
  #else 
  asm volatile ("nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n ");
  #endif 

  }
}
