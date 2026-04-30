#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "../kbd/kb_u_codes.h"

typedef struct zx_kb_state_t
{
   union 
   {
    uint32_t u[2];
    uint8_t  a[8];
   };
   

}zx_kb_state_t;

//void FAST_FUNC(set_zx_kb_state2)(zx_kb_state_t* zx_keys_matrix, uint8_t* press_codes);
void FAST_FUNC(set_zx_kb_state)(zx_kb_state_t* zx_keys_matrix, kb_state_t* kb_state);


#ifdef __cplusplus
}
#endif
