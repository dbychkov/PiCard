#include "kb_u_codes.h"
#include "string.h"
#include "stdarg.h"
#include "stdio.h"
//#include "uprintf.h"


bool FAST_FUNC(_is_active)(kb_u_state_t* kb_state, ...)
{
    va_list factor;         //указатель va_list
   
    va_start(factor, kb_state);    // устанавливаем указатель
    
    
    int key_code=va_arg(factor, int); 
    if (key_code==NO_KEY)  
        {
            //проверка на любую клавишу    
            va_end(factor);
            uint32_t* new_st=(uint32_t*)&(kb_state->new_state);

            for(int i=sizeof(kb_state->new_state)/4;i--;)
                {
                    if ((*new_st)) return true;
                    new_st++;
                }
                return false;
        }; 
    
    va_end(factor);

   
    va_start(factor, kb_state);    // устанавливаем указатель

    for(;;)
    { 
        int key_code=va_arg(factor, int); 
       
        if (key_code==NO_KEY)  break; 
        
        if ((GET_STATE_KEY(kb_state->new_state,key_code)!=0)) continue;
        else { va_end(factor);return false;
        
        
        };; //если хоть 1 клавиша не нажата - выходим с результатом false


    }
    // завершаем обработку параметров

    return true;
};


bool FAST_FUNC(_is_any_active)(kb_u_state_t* kb_state, ...)
{
    va_list factor;         //указатель va_list
    va_start(factor, kb_state);    // устанавливаем указатель

    int key_code=va_arg(factor, int); 
    if (key_code==NO_KEY)  
        {
            //проверка на любую клавишу , если аргумент   только NO_KEY
            va_end(factor);
            uint32_t* new_st=(uint32_t*)&(kb_state->new_state);

            for(int i=sizeof(kb_state->new_state)/4;i--;)
                {
                    if ((*new_st)) return true;
                    new_st++;
                }
                return false;
        }; 
    
    va_end(factor);



    for(;;)
    { 
        int key_code=va_arg(factor, int); 
       
        if (key_code==NO_KEY)  break; 
        
        if ((GET_STATE_KEY(kb_state->new_state,key_code)!=0)) return true;
    }
    // завершаем обработку параметров
    va_end(factor);

    return false;
};


bool FAST_FUNC(_is_press)(kb_u_state_t* kb_state, ...)
{
    va_list factor;         //указатель va_list
    va_start(factor, kb_state);    // устанавливаем указатель

    for(;;)
    { 
        int key_code=va_arg(factor, int); 
       
        if (key_code==NO_KEY)  break; 
        
        if ((GET_STATE_KEY(kb_state->new_state,key_code)!=0)) continue;
        else { va_end(factor);return false;};; //если хоть 1 клавиша не нажата - выходим с результатом false
    }
    // завершаем обработку параметров
    va_end(factor);

    va_start(factor, kb_state);    // устанавливаем указатель

    for(;;)
    { 
        int key_code=va_arg(factor, int); 
       
        if (key_code==NO_KEY)  break; 
        
        if ((GET_STATE_KEY(kb_state->old_state,key_code)!=0)) continue;
        else { va_end(factor);return true;};; //если хоть 1 клавиша была не нажата - выходим с результатом true
    }
    // завершаем обработку параметров
    va_end(factor);


    return false;
};


bool FAST_FUNC(_is_release)(kb_u_state_t* kb_state, ...)
{
   va_list factor;         //указатель va_list
    va_start(factor, kb_state);    // устанавливаем указатель

    for(;;)
    { 
        int key_code=va_arg(factor, int); 
       
        if (key_code==NO_KEY)  break; 
        
        if ((GET_STATE_KEY(kb_state->old_state,key_code)!=0)) continue;
        else { va_end(factor);return false;};; //если хоть 1 клавиша из списка не была нажата - выходим с результатом false
    }
    // завершаем обработку параметров
    va_end(factor);

    va_start(factor, kb_state);    // устанавливаем указатель

    for(;;)
    { 
        int key_code=va_arg(factor, int); 
       
        if (key_code==NO_KEY)  break; 
        
        if ((GET_STATE_KEY(kb_state->new_state,key_code)!=0)) continue;
        else { va_end(factor);return true;};; //если хоть 1 клавиша  не нажата - выходим с результатом true
    }
    // завершаем обработку параметров
    va_end(factor);


    return false;
};

bool FAST_FUNC(_is_hold)(kb_u_state_t* kb_state, ...)
{
    va_list factor;         //указатель va_list
    va_start(factor, kb_state);    // устанавливаем указатель

    for(;;)
    { 
        int key_code=va_arg(factor, int); 
       // xprintf("%d\n\r",key_code);
        if (key_code==NO_KEY) { va_end(factor);return true;}; 
       //  xprintf("%d\n\r",kb_state->new_state.u[1]);
        if ((GET_STATE_KEY(kb_state->new_state,key_code)!=0)&&(GET_STATE_KEY(kb_state->old_state,key_code)!=0)) continue;
        else break;
    }
    // завершаем обработку параметров
    va_end(factor);
    return false;
};

bool FAST_FUNC(_is_any_press)(kb_u_state_t* kb_state, ...)
{
    uint32_t* new_st=(uint32_t*)&(kb_state->new_state);
    uint32_t* old_st=(uint32_t*)&(kb_state->old_state);
    for(int i=sizeof(kb_state->new_state)/4;i--;)
    {
        uint32_t xor_data=(*new_st)^(*old_st);
        if (xor_data&(*new_st)) return true;
        new_st++;
        old_st++;
    }
    return false;
}

void FAST_FUNC(sum_kb_state)(kb_state_t* kb_state_sum,kb_state_t* kb_state1,kb_state_t* kb_state2)
{
    uint32_t* sum_st=(uint32_t*)(kb_state_sum);
    uint32_t* kb_st1=(uint32_t*)(kb_state1);
    uint32_t* kb_st2=(uint32_t*)(kb_state2);
    
    for(int i=sizeof(*kb_state_sum)/4;i--;)
    {
       *sum_st++=*kb_st1++|*kb_st2++;
    }

};

bool FAST_FUNC(_is_any_release)(kb_u_state_t* kb_state, ...)
{
    uint32_t* new_st=(uint32_t*)&(kb_state->new_state);
    uint32_t* old_st=(uint32_t*)&(kb_state->old_state);
    for(int i=sizeof(kb_state->new_state)/4;i--;)
    {
        uint32_t xor_data=(*new_st)^(*old_st);
        if (xor_data&(*old_st)) return true;
        new_st++;
        old_st++;
    }
    return false;
}

bool FAST_FUNC(_is_any_hold)(kb_u_state_t* kb_state, ...)
{
    uint32_t* new_st=(uint32_t*)&(kb_state->new_state);
    uint32_t* old_st=(uint32_t*)&(kb_state->old_state);
    for(int i=sizeof(kb_state->new_state)/4;i--;)
    {
        uint32_t and_data=(*new_st)&(*old_st);
        if (and_data) return true;
        new_st++;
        old_st++;
    }
    return false;
}


int FAST_FUNC(get_press_keys_codes)(kb_u_state_t* kb_state, uint8_t* press_codes, int len)
{
    int inx=0;
    int codes_sh=0;

    uint32_t* new_st=(uint32_t*)&(kb_state->new_state);
    uint32_t* old_st=(uint32_t*)&(kb_state->old_state);
    for(int i=sizeof(kb_state->new_state)/4;i--;)
    {
        uint32_t press_mask_data=((*new_st)^(*old_st))&(*new_st);
        if (press_mask_data) 
            for(int k=0;k<32;k++)
                {
                   if (press_mask_data&1)
                    {
                        *press_codes++=codes_sh+k;
                        inx++;
                        if (inx>=len)  {*press_codes=0; return inx;};
                    };
                    press_mask_data>>=1;
                }
        new_st++;
        old_st++;
        codes_sh+=32;
    }
    *press_codes=0;
    return inx;
};

int FAST_FUNC(get_active_keys_codes)(kb_u_state_t* kb_state, uint8_t* press_codes, int len)
{
    int inx=0;
    int codes_sh=0;

    uint32_t* new_st=(uint32_t*)&(kb_state->new_state);
    for(int i=sizeof(kb_state->new_state)/4;i--;)
    {
        uint32_t press_mask_data=(*new_st);
        if (press_mask_data) 
            for(int k=0;k<32;k++)
                {
                   if (press_mask_data&1)
                    {
                        *press_codes++=codes_sh+k;
                        inx++;
                        if (inx>=len)  {*press_codes=0; return inx;};
                    };
                    press_mask_data>>=1;
                }
        new_st++;
        codes_sh+=32;
    }
    *press_codes=0;
    return inx;
};

int FAST_FUNC(get_release_keys_codes)(kb_u_state_t* kb_state, uint8_t* release_codes, int len)
{
    int inx=0;
    int codes_sh=0;

    uint32_t* new_st=(uint32_t*)&(kb_state->new_state);
    uint32_t* old_st=(uint32_t*)&(kb_state->old_state);
    for(int i=sizeof(kb_state->new_state)/4;i--;)
    {
        uint32_t release_mask_data=((*new_st)^(*old_st))&(*old_st);
        if (release_mask_data) 
            for(int k=0;k<32;k++)
                {
                   if (release_mask_data&1)
                    {
                        *release_codes++=codes_sh+k;
                        inx++;
                        if (inx>=len) {*release_codes=0; return inx;};
                    };
                    release_mask_data>>=1;
                }
        new_st++;
        old_st++;
        codes_sh+=32;
    }
    *release_codes=0;
    return inx;
};
void keys_to_str(char* str_buf,char s_char,kb_state_t kb_state)
{
    char s_str[2];
    s_str[0]=s_char;
    s_str[1]='\0';

    str_buf[0]=0;
    strcat(str_buf," ");
//0 набор
    if GET_STATE_KEY(kb_state,KEY_A) {strcat(str_buf,"A");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_B) {strcat(str_buf,"B");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_C) {strcat(str_buf,"C");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_D) {strcat(str_buf,"D");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_E) {strcat(str_buf,"E");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F) {strcat(str_buf,"F");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_G) {strcat(str_buf,"G");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_H) {strcat(str_buf,"H");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_I) {strcat(str_buf,"I");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_J) {strcat(str_buf,"J");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_K) {strcat(str_buf,"K");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_L) {strcat(str_buf,"L");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_M) {strcat(str_buf,"M");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_N) {strcat(str_buf,"N");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_O) {strcat(str_buf,"O");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_P) {strcat(str_buf,"P");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_Q) {strcat(str_buf,"Q");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_R) {strcat(str_buf,"R");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_S) {strcat(str_buf,"S");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_T) {strcat(str_buf,"T");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_U) {strcat(str_buf,"U");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_V) {strcat(str_buf,"V");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_W) {strcat(str_buf,"W");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_X) {strcat(str_buf,"X");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_Y) {strcat(str_buf,"Y");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_Z) {strcat(str_buf,"Z");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_SEMICOLON) {strcat(str_buf,";");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_QUOTE) {strcat(str_buf,"\"");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_COMMA) {strcat(str_buf,",");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_PERIOD) {strcat(str_buf,".");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_LEFT_BR) {strcat(str_buf,"[");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_RIGHT_BR) {strcat(str_buf,"]");strcat(str_buf,s_str);};
//1 набор
    if GET_STATE_KEY(kb_state,KEY_0) {strcat(str_buf,"0");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_1) {strcat(str_buf,"1");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_2) {strcat(str_buf,"2");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_3) {strcat(str_buf,"3");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_4) {strcat(str_buf,"4");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_5) {strcat(str_buf,"5");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_6) {strcat(str_buf,"6");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_7) {strcat(str_buf,"7");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_8) {strcat(str_buf,"8");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_9) {strcat(str_buf,"9");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_ENTER) {strcat(str_buf,"ENTER");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_SLASH) {strcat(str_buf,"/");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_MINUS) {strcat(str_buf,"MINUS");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_EQUALS) {strcat(str_buf,"=");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_BACKSLASH) {strcat(str_buf,"BACKSLASH");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_CAPS_LOCK) {strcat(str_buf,"CAPS_LOCK");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_TAB) {strcat(str_buf,"TAB");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_BACK_SPACE) {strcat(str_buf,"BACK_SPACE");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_ESC) {strcat(str_buf,"ESC");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_TILDE) {strcat(str_buf,"TILDE");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_MENU) {strcat(str_buf,"MENU");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_L_SHIFT) {strcat(str_buf,"L_SHIFT");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_L_CTRL) {strcat(str_buf,"L_CTRL");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_L_ALT) {strcat(str_buf,"L_ALT");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_L_WIN) {strcat(str_buf,"L_WIN");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_R_SHIFT) {strcat(str_buf,"R_SHIFT");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_R_CTRL) {strcat(str_buf,"R_CTRL");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_R_ALT) {strcat(str_buf,"R_ALT");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_R_WIN) {strcat(str_buf,"R_WIN");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_SPACE) {strcat(str_buf,"SPACE");strcat(str_buf,s_str);};


//2 набор
    if GET_STATE_KEY(kb_state,KEY_NUM_0) {strcat(str_buf,"NUM_0");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_1) {strcat(str_buf,"NUM_1");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_2) {strcat(str_buf,"NUM_2");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_3) {strcat(str_buf,"NUM_3");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_4) {strcat(str_buf,"NUM_4");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_5) {strcat(str_buf,"NUM_5");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_6) {strcat(str_buf,"NUM_6");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_7) {strcat(str_buf,"NUM_7");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_8) {strcat(str_buf,"NUM_8");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_9) {strcat(str_buf,"NUM_9");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_NUM_ENTER) {strcat(str_buf,"NUM_ENTER");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_SLASH) {strcat(str_buf,"NUM_/");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_MINUS) {strcat(str_buf,"NUM_MINUS");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_NUM_PLUS) {strcat(str_buf,"NUM_PLUS");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_MULT) {strcat(str_buf,"NUM_MULT");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_PERIOD) {strcat(str_buf,"NUM_PERIOD");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_NUM_LOCK) {strcat(str_buf,"NUM_LOCK");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_DELETE) {strcat(str_buf,"DEL");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_SCROLL_LOCK) {strcat(str_buf,"SCROLL_LOCK");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_PAUSE_BREAK) {strcat(str_buf,"PAUSE_BREAK");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_INSERT) {strcat(str_buf,"INSERT");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_HOME) {strcat(str_buf,"HOME");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_PAGE_UP) {strcat(str_buf,"PG_UP");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_PAGE_DOWN) {strcat(str_buf,"PG_DOWN");strcat(str_buf,s_str);};

    if GET_STATE_KEY(kb_state,KEY_PRT_SCR) {strcat(str_buf,"PRT_SCR");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_END) {strcat(str_buf,"END");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_UP) {strcat(str_buf,"UP");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_DOWN) {strcat(str_buf,"DOWN");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_LEFT) {strcat(str_buf,"LEFT");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_RIGHT) {strcat(str_buf,"RIGHT");strcat(str_buf,s_str);};

//3 набор
    if GET_STATE_KEY(kb_state,KEY_F1) {strcat(str_buf,"F1");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F2) {strcat(str_buf,"F2");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F3) {strcat(str_buf,"F3");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F4) {strcat(str_buf,"F4");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F5) {strcat(str_buf,"F5");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F6) {strcat(str_buf,"F6");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F7) {strcat(str_buf,"F7");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F8) {strcat(str_buf,"F8");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F9) {strcat(str_buf,"F9");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F10) {strcat(str_buf,"F10");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F11) {strcat(str_buf,"F11");strcat(str_buf,s_str);};
    if GET_STATE_KEY(kb_state,KEY_F12) {strcat(str_buf,"F12");strcat(str_buf,s_str);};

    //strcat(str_buf,"\n");

};

