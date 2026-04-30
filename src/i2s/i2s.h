#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "inttypes.h"

#include "../../config.h"




void i2s_init();
void i2s_deinit();
void i2s_out(int16_t l_out,int16_t r_out);

#ifdef __cplusplus
}
#endif
