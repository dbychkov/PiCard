#include "ts.h"
#include <string.h>
#include <Arduino.h>

static uint8_t maskAY[16] = {0xff,0x0f,0xff,0x0f,0xff,0x0f ,0x1f, 0xff, 0x1f,0x1f,0x1f, 0xff , 0xff, 0x0f,0xff,0xff};

//static uint8_t ampls_AY[]={0,1,1,1,2,2,3,5,6,9,13,17,22,29,36,45};/*AY_TABLE*///
//static uint8_t ampls_AY[]={0,1,2,3,4,5,6,7,8,9,13,17,22,31,42,55};/*MY_ AY_TABLE*/
//static uint8_t ampls_AY[]={0,1,2,3,4,5,6,8,10,12,16,19,23,28,34,40};/*снятя с китайского чипа*/
//#define MUL_AMPL (450)
//
//static uint8_t ampls_AY[]={0,3,5,7,10,15,21,34,41,65,91,114,144,181,215,255};
//#define MUL_AMPL (95)
//

static const int16_t ampls_AY[16] = {    0,     146,   216,   322,   475,   713,   1073,  1578,   1879,  2807,  4171,  6173,   9194,  13624, 20271, 21845};
#define MUL_AMPL (1)

static uint16_t Envelopes[16][32];

static const uint8_t Envelopes_const[16][32] =
{
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*0*/
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*1*/
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*2*/
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*3*/
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*4*/
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*5*/
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*6*/
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*7*/
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },/*8*/
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },/*9*/
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 }, /*10 0x0a */
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },/*11  0x0b */
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },/*12  0x0c*/
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },/*13  0x0d*/
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },/*14  0x0e*/
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }/*15 0x0d*/
};  

#define DELTA (2)

static void  in_init(uint pin)
{

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_set_pulls(pin,true,false);
    // gpio_set_input_hysteresis_enabled(pin, true);
    // gpio_pull_down(pin);     
};

TS::TS()
{
    reset();

    for (uint8_t j = 0; j < 16; j++)
        {
            for (uint8_t i = 0; i < 32; i++)
            {
              Envelopes[j][i] = ampls_AY[Envelopes_const[j][i]];
            }
        };
};

void __not_in_flash_func(TS::reset)()
{
    memset(reg[0], 0, 16);
    reg[0][14] = 0xff;
    memset(reg[1], 0, 16);
    reg[1][14] = 0xff;
    
#if AY_UART_EN
    in_init(PIN_UART_RX);    
    in_init(PIN_UART_TX);    
    in_init(PIN_UART_CTS);
    gpio_put(PIN_UART_TX,1);    
    gpio_put(PIN_UART_CTS,1);
    
    gpio_set_dir(PIN_UART_TX,GPIO_OUT);    
    gpio_set_dir(PIN_UART_CTS,GPIO_OUT);
#endif    

};

void __not_in_flash_func(TS::select_reg)(uint8_t N_reg)
{
    if (N_reg == 0xff) {i_chip = 0;return;} 
    if (N_reg == 0xfe) {i_chip = 1;return;}
    if (N_reg>15) return;

    
    i_reg[i_chip]=N_reg;

};

uint8_t __not_in_flash_func(TS::get_reg)()
{

#if AY_UART_EN
    if ((i_reg[i_chip])==14) 
        {
            return (gpio_get(PIN_UART_RX)<<7)|(portA&0x7f);
        };
#endif
    return reg[i_chip][i_reg[i_chip]];
};

void __not_in_flash_func(TS::set_reg)(uint8_t val)
{
#if AY_UART_EN
       if ((i_reg[i_chip])==14) 
        {
            portA=val;
            gpio_put(PIN_UART_TX,(portA&0x08));    
            gpio_put(PIN_UART_CTS,(portA&0x04));
            return;
        };
#endif
     reg[i_chip][i_reg[i_chip]]=val&maskAY[i_reg[i_chip]];

        

      switch (i_reg[i_chip])
        {
        case 0:
        case 1:
                period[i_chip][0]=((reg[i_chip][1]<<8)|reg[i_chip][0]);
            return; 
        case 2:
        case 3:
                period[i_chip][1]=((reg[i_chip][3]<<8)|reg[i_chip][2]);
            return; 
        case 4:
        case 5:
                period[i_chip][2]=((reg[i_chip][5]<<8)|reg[i_chip][4]);
            return;        

        case 6:  // частота генератора шума 	0 - 31
            noise_ay0_count[i_chip] = 0;
            return;
#if AY_UART_EN
        case 7:
            DIR_PORTA=(val>>6)&1;
      

            return;
#endif
        case 13://Выбор формы волнового пакета 	0 - 15
            is_envelope_begin[i_chip] = true;
            return;
        }
};

sound_LR __not_in_flash_func(TS::get_LR_sample)(){return sample;};

bool __not_in_flash_func(get_random)()
{
    static uint32_t Cur_Seed = 0xffff;
    Cur_Seed = (Cur_Seed * 2 + 1) ^
               (((Cur_Seed >> 16) ^ (Cur_Seed >> 13)) & 1);
    if ((Cur_Seed >> 16) & 1)
        return true;
    return false;
};

void __not_in_flash_func(TS::step)()
{
    sound_LR sample_out={-32000,-32000};
    static bool bools[2][4]; 
    static uint32_t main_ay_count_env[2]={0,0};
    static uint32_t envelope_ay_count[2]={0,0};
    static uint16_t ampl_ENV[2]={0,0};
for (int _i_chip=0;_i_chip<2; _i_chip++)
    {


    #define chA_bit (bools[_i_chip][0])
    #define chB_bit (bools[_i_chip][1])
    #define chC_bit (bools[_i_chip][2])
    #define noise_bit (bools[_i_chip][3])
  
   

    //отдельные счётчики
    static uint32_t chA_count[2]={0,0};
    static uint32_t chB_count[2]={0,0};
    static uint32_t chC_count[2]={0,0};

    //копирование прошлого значения каналов для более быстрой работы

    bool chA_bitOut=chA_bit;
    bool chB_bitOut=chB_bit;
    bool chC_bitOut=chC_bit;

    uint16_t ay0_A_period =((reg[_i_chip][1]<<8)|reg[_i_chip][0]);
    uint16_t ay0_B_period =((reg[_i_chip][3]<<8)|reg[_i_chip][2]);
    uint16_t ay0_C_period =((reg[_i_chip][5]<<8)|reg[_i_chip][4]);
    uint16_t ay0_R12_R11=  ((reg[_i_chip][12]<<8)|reg[_i_chip][11]);
    
//    #define ay0_A_period (period[_i_chip][0])    
//    #define ay0_B_period (period[_i_chip][1])
//    #define ay0_C_period (period[_i_chip][2])
    uint8_t R7=reg[_i_chip][7];
    uint8_t nR7 (~R7);
    //nR7 - инвертированый R7 для прямой логики - 1 Вкл, 0 - Выкл

    uint32_t dA=0,dB=0,dC=0; 

//    if ((nR7&0x1)&&(ay0_A_period>=DELTA)) {chA_count[_i_chip]+=DELTA;if (chA_count[_i_chip]>= ay0_A_period) {chA_bit^=1;chA_count[_i_chip]-=ay0_A_period;dA=chA_count[_i_chip]%DELTA;}} else {chA_bitOut=1;chA_count[_i_chip]=0;}; /*Тон A*/
//    if ((nR7&0x2)&&(ay0_B_period>=DELTA)) {chB_count[_i_chip]+=DELTA;if (chB_count[_i_chip]>= ay0_B_period) {chB_bit^=1;chB_count[_i_chip]-=ay0_B_period;dB=chB_count[_i_chip]%DELTA;}} else {chB_bitOut=1;chB_count[_i_chip]=0;}; /*Тон B*/
//    if ((nR7&0x4)&&(ay0_C_period>=DELTA)) {chC_count[_i_chip]+=DELTA;if (chC_count[_i_chip]>= ay0_C_period) {chC_bit^=1;chC_count[_i_chip]-=ay0_C_period;dC=chC_count[_i_chip]%DELTA;}} else {chC_bitOut=1;chC_count[_i_chip]=0;}; /*Тон C*/


//точнее частота, но подзванивает при больших DELTA
    if (nR7&0x1) {chA_count[_i_chip]+=DELTA;if (chA_count[_i_chip]>= ay0_A_period) {chA_bit^=1;dA=chA_count[_i_chip]%DELTA;chA_count[_i_chip]-=ay0_A_period;}} else {chA_bitOut=1;chA_count[_i_chip]=0;}; /*Тон A*/
    if (nR7&0x2) {chB_count[_i_chip]+=DELTA;if (chB_count[_i_chip]>= ay0_B_period) {chB_bit^=1;dB=chB_count[_i_chip]%DELTA;chB_count[_i_chip]-=ay0_B_period;}} else {chB_bitOut=1;chB_count[_i_chip]=0;}; /*Тон B*/
    if (nR7&0x4) {chC_count[_i_chip]+=DELTA;if (chC_count[_i_chip]>= ay0_C_period) {chC_bit^=1;dC=chC_count[_i_chip]%DELTA;chC_count[_i_chip]-=ay0_C_period;}} else {chC_bitOut=1;chC_count[_i_chip]=0;}; /*Тон C*/

//период отбрасывает не кратные DELTA значения
//    if (nR7&0x1) {chA_count[_i_chip]+=DELTA;if (chA_count[_i_chip]>= ay0_A_period) {chA_bit^=1;chA_count[_i_chip]=0;dA=0;}} else {chA_bitOut=1;chA_count[_i_chip]=0;}; /*Тон A*/
//    if (nR7&0x2) {chB_count[_i_chip]+=DELTA;if (chB_count[_i_chip]>= ay0_B_period) {chB_bit^=1;chB_count[_i_chip]=0;dB=0;}} else {chB_bitOut=1;chB_count[_i_chip]=0;}; /*Тон B*/
//    if (nR7&0x4) {chC_count[_i_chip]+=DELTA;if (chC_count[_i_chip]>= ay0_C_period) {chC_bit^=1;chC_count[_i_chip]=0;dC=0;}} else {chC_bitOut=1;chC_count[_i_chip]=0;}; /*Тон C*/
//
//    //проверка запрещения тона в каналах
//    if (R7&0x1) chA_bitOut=1; 
//    if (R7&0x2) chB_bitOut=1;
//    if (R7&0x4) chC_bitOut=1;

    //добавление шума, если разрешён шумовой канал
    if (nR7&0x38)//есть шум хоть в одном канале
        {
 
          noise_ay0_count[_i_chip]+=DELTA;
          if (noise_ay0_count[_i_chip]>=(reg[_i_chip][6]<<1)) {noise_bit=get_random(); noise_ay0_count[_i_chip]=0;}//отдельный счётчик для шумового
                                // R6 - частота шума
            
               if(!noise_bit)//если бит шума ==1, то он не меняет состояние каналов

                {            
                    if ((chA_bitOut)&&(nR7&0x08)) chA_bitOut=0;//шум в канале A
                    if ((chB_bitOut)&&(nR7&0x10)) chB_bitOut=0;//шум в канале B
                    if ((chC_bitOut)&&(nR7&0x20)) chC_bitOut=0;//шум в канале C

                };
           
        }

       // амплитуды огибающей
        if ((reg[_i_chip][8] & 0x10) | (reg[_i_chip][9] & 0x10) | (reg[_i_chip][10] & 0x10)) // отключение огибающей
        {   
            main_ay_count_env[_i_chip] += DELTA;
            if (is_envelope_begin[_i_chip])
            {
                envelope_ay_count[_i_chip] = 0;
                main_ay_count_env[_i_chip] = 0;
                is_envelope_begin[_i_chip] = false;
            };

            if (((main_ay_count_env[_i_chip]) >= (ay0_R12_R11 << 1))) // без операции деления
            {
                   main_ay_count_env[_i_chip] -= ay0_R12_R11 << 1;
  
             if (envelope_ay_count[_i_chip] >= 32)
             {
              //  if ((reg[_i_chip][13]==0x08)| (reg[_i_chip][13]==0x0a) | (reg[_i_chip][13]==0x0c) | (reg[_i_chip][13]==0x0e))
              if ((reg[_i_chip][13]&0x08) && ((~(reg[_i_chip][13]))&0x01))
                 envelope_ay_count[_i_chip] = 0;  // loop 
                 else envelope_ay_count[_i_chip] = 31;

             }

                ampl_ENV[_i_chip] =  Envelopes[reg[_i_chip][13]][envelope_ay_count[_i_chip]]  ; // из оперативки
                envelope_ay_count[_i_chip]++;
            }
        } //end  амплитуды огибающей 


//static uint16_t outs[3];
//
//#define outA (outs[0])
//#define outB (outs[1])
//#define outC (outs[2])

        static uint32_t out_old[2][3];
        #define OUT_A_OLD (out_old[_i_chip][0])        
        #define OUT_B_OLD (out_old[_i_chip][1])
        #define OUT_C_OLD (out_old[_i_chip][2])

        uint32_t _outA = chA_bitOut ? ((reg[_i_chip][8] & 0xf0) ? (ampl_ENV[_i_chip]) : ampls_AY [reg[_i_chip][8]]) : 0;
        uint32_t _outB = chB_bitOut ? ((reg[_i_chip][9] & 0xf0) ? (ampl_ENV[_i_chip]) : ampls_AY [reg[_i_chip][9]]) >>1: 0;
        uint32_t _outC = chC_bitOut ? ((reg[_i_chip][10] & 0xf0)? (ampl_ENV[_i_chip]) : ampls_AY [reg[_i_chip][10]]) : 0;

        uint32_t outA=(_outA*(DELTA-dA)+OUT_A_OLD*dA)/DELTA;        
        uint32_t outB=(_outB*(DELTA-dB)+OUT_B_OLD*dB)/DELTA;        
        uint32_t outC=(_outC*(DELTA-dC)+OUT_C_OLD*dC)/DELTA;


        OUT_A_OLD=_outA;
        OUT_B_OLD=_outB;
        OUT_C_OLD=_outC;

        

//        uint8_t outA = chA_bitOut ?  ampls_AY[reg[_i_chip][8]] : 0;
//        uint8_t outB = chB_bitOut ?  ampls_AY[reg[_i_chip][9]] >>1: 0;
//        uint8_t outC = chC_bitOut ?  ampls_AY[reg[_i_chip][10]] : 0;
        
        sample_out.R+=((outA+outB+outB/2+0*outC/8)*MUL_AMPL);        
        sample_out.L+=((outC+outB+outB/2+0*outA/8)*MUL_AMPL);
//        sample_out.R-=6000;
//        sample_out.L-=6000;
    }

    sample=sample_out;
    
    

}; 


TS ts;







