#include "zx_kb.h"

static zx_kb_state_t zx_kb_state;

static void inline set_code(zx_kb_state_t* zx_keys_matrix,uint8_t code)
{
    switch (code)
            {
            case KEY_A: zx_keys_matrix->a[1]|=(1<<0); break;
            case KEY_B: zx_keys_matrix->a[7]|=(1<<4); break;
            case KEY_C: zx_keys_matrix->a[0]|=(1<<3); break;
            case KEY_D: zx_keys_matrix->a[1]|=(1<<2); break;
            case KEY_E: zx_keys_matrix->a[2]|=(1<<2); break;
            case KEY_F: zx_keys_matrix->a[1]|=(1<<3); break;
            case KEY_G: zx_keys_matrix->a[1]|=(1<<4); break;
            case KEY_H: zx_keys_matrix->a[6]|=(1<<4); break;
            case KEY_I: zx_keys_matrix->a[5]|=(1<<2); break;
            case KEY_J: zx_keys_matrix->a[6]|=(1<<3); break;
            case KEY_K: zx_keys_matrix->a[6]|=(1<<2); break;
            case KEY_L: zx_keys_matrix->a[6]|=(1<<1); break;
            case KEY_M: zx_keys_matrix->a[7]|=(1<<2); break;
            case KEY_N: zx_keys_matrix->a[7]|=(1<<3); break;
            case KEY_O: zx_keys_matrix->a[5]|=(1<<1); break;
            case KEY_P: zx_keys_matrix->a[5]|=(1<<0); break;
            case KEY_Q: zx_keys_matrix->a[2]|=(1<<0); break;
            case KEY_R: zx_keys_matrix->a[2]|=(1<<3); break;
            case KEY_S: zx_keys_matrix->a[1]|=(1<<1); break;
            case KEY_T: zx_keys_matrix->a[2]|=(1<<4); break;
            case KEY_U: zx_keys_matrix->a[5]|=(1<<3); break;
            case KEY_V: zx_keys_matrix->a[0]|=(1<<4); break;
            case KEY_W: zx_keys_matrix->a[2]|=(1<<1); break;
            case KEY_X: zx_keys_matrix->a[0]|=(1<<2); break;
            case KEY_Y: zx_keys_matrix->a[5]|=(1<<4); break;
            case KEY_Z: zx_keys_matrix->a[0]|=(1<<1); break;
            case KEY_1: zx_keys_matrix->a[3]|=(1<<0); break;
            case KEY_2: zx_keys_matrix->a[3]|=(1<<1); break;
            case KEY_3: zx_keys_matrix->a[3]|=(1<<2); break;
            case KEY_4: zx_keys_matrix->a[3]|=(1<<3); break;
            case KEY_5: zx_keys_matrix->a[3]|=(1<<4); break;
            case KEY_6: zx_keys_matrix->a[4]|=(1<<4); break;
            case KEY_7: zx_keys_matrix->a[4]|=(1<<3); break;
            case KEY_8: zx_keys_matrix->a[4]|=(1<<2); break;
            case KEY_9: zx_keys_matrix->a[4]|=(1<<1); break;
            case KEY_0: zx_keys_matrix->a[4]|=(1<<0); break;
            case KEY_ENTER: zx_keys_matrix->a[6]|=(1<<0); break;
            case KEY_SPACE: zx_keys_matrix->a[7]|=(1<<0); break;

            


            case KEY_R_SHIFT: zx_keys_matrix->a[0]|=(1<<0); break;//CS
            case KEY_L_SHIFT: zx_keys_matrix->a[0]|=(1<<0); break;
            case KEY_R_CTRL: zx_keys_matrix->a[7]|=(1<<1); break; //SS
            case KEY_L_CTRL: zx_keys_matrix->a[7]|=(1<<1); break;
            
            case KEY_DELETE: zx_keys_matrix->a[0]|=(1<<0);zx_keys_matrix->a[4]|=(1<<0); break;
            case KEY_BACK_SPACE: zx_keys_matrix->a[0]|=(1<<0);zx_keys_matrix->a[4]|=(1<<0); break;

            case KEY_CAPS_LOCK: zx_keys_matrix->a[0]|=(1<<0);zx_keys_matrix->a[3]|=(1<<1); break;
            case KEY_ESC:       zx_keys_matrix->a[0]|=(1<<0);zx_keys_matrix->a[7]|=(1<<0);  break;
            case KEY_TAB:       zx_keys_matrix->a[0]|=(1<<0);zx_keys_matrix->a[3]|=(1<<0); break;

          //  case KEY_QUOTE:   zx_keys_matrix->a[7]|=(1<<1);  break;  
            case KEY_QUOTE:   zx_keys_matrix->a[7]|=(1<<1); zx_keys_matrix->a[5]|=(1<<0);  break;  
            case KEY_COMMA:   zx_keys_matrix->a[7]|=(1<<1);zx_keys_matrix->a[7]|=(1<<3);   break;  
            case KEY_PERIOD:  zx_keys_matrix->a[7]|=(1<<1);zx_keys_matrix->a[7]|=(1<<2);  break;  
            case KEY_SLASH:   zx_keys_matrix->a[7]|=(1<<1);zx_keys_matrix->a[0]|=(1<<4);  break;  
            case KEY_SEMICOLON:   zx_keys_matrix->a[7]|=(1<<1); zx_keys_matrix->a[5]|=(1<<1); break;  
            case KEY_MINUS:   zx_keys_matrix->a[7]|=(1<<1);zx_keys_matrix->a[6]|=(1<<3);  break;  
            case KEY_EQUALS:   zx_keys_matrix->a[7]|=(1<<1);zx_keys_matrix->a[6]|=(1<<1);  break; 




            case KEY_UP:zx_keys_matrix->a[0]|=(1<<0); zx_keys_matrix->a[4]|=(1<<3);break;
            case KEY_DOWN:zx_keys_matrix->a[0]|=(1<<0); zx_keys_matrix->a[4]|=(1<<4);break;
            case KEY_LEFT:zx_keys_matrix->a[0]|=(1<<0); zx_keys_matrix->a[3]|=(1<<4);break;
            case KEY_RIGHT:zx_keys_matrix->a[0]|=(1<<0); zx_keys_matrix->a[4]|=(1<<2);break;
            default:
                break;
            }

};

// void FAST_FUNC(set_zx_kb_state2)(zx_kb_state_t* zx_keys_matrix, uint8_t* press_codes)
// {
//         uint8_t* p_code=press_codes;
        
//         uint32_t* p_zx_keys= (uint32_t*)zx_keys_matrix;
//         for(int i=sizeof(*zx_keys_matrix)/4;i--;){*p_zx_keys++=0;}
        
//         while (*p_code)
//         {
//             set_code(zx_keys_matrix,*p_code);
            
//             p_code++;
//         }
        

// };

void FAST_FUNC(set_zx_kb_state)(zx_kb_state_t* zx_keys_matrix, kb_state_t* kb_state)
{
      

    zx_keys_matrix->u[0]=0;
    zx_keys_matrix->u[1]=0;
    
    uint32_t* new_st=(uint32_t*)kb_state;
    int codes_sh=0;
    for(int i=sizeof(*kb_state)/4;i--;)
    {
        uint32_t press_mask_data=(*new_st);
        if (press_mask_data) 
            for(int k=0;k<32;k++)
                {
                   if (press_mask_data&1)
                    {
                        uint8_t code=codes_sh+k;
                        set_code(zx_keys_matrix,code);
                    };
                    press_mask_data>>=1;
                }
        new_st++;
        codes_sh+=32;
    }
        

};
