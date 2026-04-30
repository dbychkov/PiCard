#pragma once

#include "../../config.h"

class TS
{
    int i_chip=0;
    uint8_t i_reg[2]={0,0};
    uint32_t noise_ay0_count[2]={0,0};
    bool is_envelope_begin[2]={false,false};
    uint8_t reg[2][16];
    sound_LR sample;
    uint16_t period[2][3]={{0,0,0},{0,0,0}};
    uint8_t portA=0xff;
    bool DIR_PORTA=true;

   public:
    TS();
    void reset();
    void select_reg(uint8_t N_reg);
    void set_reg(uint8_t val);
    uint8_t get_reg();
    sound_LR get_LR_sample();
    void step(); 
    
}; 

extern TS ts;
