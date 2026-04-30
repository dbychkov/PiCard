#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "pico.h"
#define FAST_FUNC __not_in_flash_func


#include "inttypes.h"
typedef struct kb_state_t
{
   uint32_t u[4];

}kb_state_t;

typedef struct kb_u_state_t
{
   kb_state_t new_state;
   kb_state_t old_state;
}kb_u_state_t;


// блок 0 буквы
#define NO_KEY (0)
#define KEY_A (1)
#define KEY_B (2)
#define KEY_C (3)
#define KEY_D (4)
#define KEY_E (5)
#define KEY_F (6)
#define KEY_G (7)
#define KEY_H (8)
#define KEY_I (9)
#define KEY_J (10)
#define KEY_K (11)
#define KEY_L (12)
#define KEY_M (13)
#define KEY_N (14)
#define KEY_O (15)
#define KEY_P (16)
#define KEY_Q (17)
#define KEY_R (18)
#define KEY_S (19)
#define KEY_T (20)
#define KEY_U (21)
#define KEY_V (22)
#define KEY_W (23)
#define KEY_X (24)
#define KEY_Y (25)
#define KEY_Z (26)

#define KEY_SEMICOLON (27)
#define KEY_QUOTE (28)
#define KEY_COMMA (29)
#define KEY_PERIOD (30)
#define KEY_LEFT_BR (31)
#define KEY_RIGHT_BR (32)



#define KEY_0 (33)
#define KEY_1 (34)
#define KEY_2 (35)
#define KEY_3 (36)
#define KEY_4 (37)
#define KEY_5 (38)
#define KEY_6 (39)
#define KEY_7 (40)
#define KEY_8 (41)
#define KEY_9 (42)

#define KEY_ENTER (43)
#define KEY_SLASH (44)
#define KEY_MINUS (45)

#define KEY_EQUALS (46)
#define KEY_BACKSLASH (47)
#define KEY_CAPS_LOCK (48)
#define KEY_TAB (49)
#define KEY_BACK_SPACE (50)
#define KEY_ESC (51)
#define KEY_TILDE (52)
#define KEY_MENU (53)

#define KEY_L_SHIFT (54)
#define KEY_L_CTRL (55)
#define KEY_L_ALT (56)
#define KEY_L_WIN (57)
#define KEY_R_SHIFT (58)
#define KEY_R_CTRL (59)
#define KEY_R_ALT (60)
#define KEY_R_WIN (61)

#define KEY_SPACE (62)

//63 -резерв





#define KEY_NUM_0 (64)
#define KEY_NUM_1 (65)
#define KEY_NUM_2 (66)
#define KEY_NUM_3 (67)
#define KEY_NUM_4 (68)
#define KEY_NUM_5 (69)
#define KEY_NUM_6 (70)
#define KEY_NUM_7 (71)
#define KEY_NUM_8 (72)
#define KEY_NUM_9 (73)

#define KEY_NUM_ENTER (74)
#define KEY_NUM_SLASH (75)
#define KEY_NUM_MINUS (76)

#define KEY_NUM_PLUS (77)
#define KEY_NUM_MULT (78)
#define KEY_NUM_PERIOD  (79)
#define KEY_NUM_LOCK (80)


#define KEY_DELETE (81)
#define KEY_SCROLL_LOCK (82)
#define KEY_PAUSE_BREAK (83)
#define KEY_INSERT (84)
#define KEY_HOME (85)
#define KEY_PAGE_UP (86)
#define KEY_PAGE_DOWN (87)

#define KEY_PRT_SCR (88)
#define KEY_END (89)
#define KEY_UP (90)
#define KEY_DOWN (91)
#define KEY_LEFT (92)
#define KEY_RIGHT (93)

#define KEY_F1 (94)
#define KEY_F2 (95)
#define KEY_F3 (96)
#define KEY_F4 (97)
#define KEY_F5 (98)
#define KEY_F6 (99)
#define KEY_F7 (100)
#define KEY_F8 (101)
#define KEY_F9 (102)
#define KEY_F10 (103)
#define KEY_F11 (104)
#define KEY_F12 (105)


//

bool FAST_FUNC(_is_press)(kb_u_state_t* kb_state, ...);
bool FAST_FUNC(_is_hold)(kb_u_state_t* kb_state, ...);
bool FAST_FUNC(_is_release)(kb_u_state_t* kb_state, ...);

bool FAST_FUNC(_is_any_press)(kb_u_state_t* kb_state, ...);
bool FAST_FUNC(_is_any_release)(kb_u_state_t* kb_state, ...);
bool FAST_FUNC(_is_any_hold)(kb_u_state_t* kb_state, ...);

bool FAST_FUNC(_is_active)(kb_u_state_t* kb_state, ...);
bool FAST_FUNC(_is_any_active)(kb_u_state_t* kb_state, ...);

#define IS_ACTIVE(...) (_is_active(__VA_ARGS__,NO_KEY))
#define IS_ANY_ACTIVE(...) (_is_any_active(__VA_ARGS__,NO_KEY))


#define IS_PRESS(...) (_is_press(__VA_ARGS__,NO_KEY))
#define IS_HOLD(...) (_is_hold(__VA_ARGS__,NO_KEY))
#define IS_RELEASE(...) (_is_release(__VA_ARGS__,NO_KEY))

#define IS_ANY_PRESS(...) (_is_any_press(__VA_ARGS__,NO_KEY))
#define IS_ANY_HOLD(...) (_is_any_hold(__VA_ARGS__,NO_KEY))
#define IS_ANY_RELEASE(...) (_is_any_release(__VA_ARGS__,NO_KEY))

#define GET_STATE_KEY(k_state,key_code)  (((k_state).u[(key_code)>>5])&(1<<((key_code)&0x1f))) 


#define SET_STATE_KEY(k_state,key_code)  (((k_state).u[(key_code)>>5])|=(1<<((key_code)&0x1f)))   
#define CLR_STATE_KEY(k_state,key_code)  (((k_state).u[(key_code)>>5])&=~(1<<((key_code)&0x1f)))   

int FAST_FUNC(get_press_keys_codes)(kb_u_state_t* kb_state, uint8_t* press_codes, int len);
int FAST_FUNC(get_release_keys_codes)(kb_u_state_t* kb_state, uint8_t* release_codes, int len);
int FAST_FUNC(get_active_keys_codes)(kb_u_state_t* kb_state, uint8_t* press_codes, int len);

void FAST_FUNC(sum_kb_state)(kb_state_t* kb_state_sum,kb_state_t* kb_state1,kb_state_t* kb_state2);
//#define CLEAR_KB_STATE(kb_state) memset(kb_state,)
//для тестирования
void keys_to_str(char* str_buf,char s_char,kb_state_t kb_state);

#ifdef __cplusplus
}
#endif