#pragma once
#include <pico/stdlib.h>


#include "z80/Z80.h"
#include "inttypes.h"
#include "../../config.h"

#if GS_EN

#define ram_page 64

#define CommandBIT 0x01;
#define DataBIT 0x80;

void GS_init();
sound_LR GS_get_sound_LR_sample();
extern volatile uint8_t GSCTR;
extern volatile uint8_t GSSTAT;
extern volatile uint8_t GSDAT;
extern volatile uint8_t GSCOM;
extern volatile uint8_t ZXDATWR;
extern Z80 cpu;

#endif
