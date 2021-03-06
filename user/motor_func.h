#ifndef MOTOR_FUNC_H
#define MOTOR_FUNC_H

#include "headfiles.h"

#define     MAX_RUN_TIME         18000        //10ms unit
#define     MIN_RUN_TIME         500        //10ms unit

#define MOTOR_DEGREE_CTRL     1
#define MOTOR_TIME_CTRL       2
#define MOTOR_INIT_CTRL       3

#define INVALID_DEGREE       0xFF

enum
{
    MOTOR_STOP = 1,
    MOTOR_FORWARD,
    MOTOR_REVERSE
};

enum MOTOR_S
{
    MOTOR_PWR_OFF=0, MOTOR_FOFF, MOTOR_FON, MOTOR_F, MOTOR_ROFF, MOTOR_RON, MOTOR_R, MOTOR_END
};



struct MOTOR
{
   GPIO_TypeDef* GPIOx_f;
   GPIO_Pin_TypeDef GPIO_Pin_f;
   GPIO_TypeDef* GPIOx_r;
   GPIO_Pin_TypeDef GPIO_Pin_r;
   GPIO_TypeDef* GPIOx_off;
   GPIO_Pin_TypeDef GPIO_Pin_off;

    uint8 cur_state;
    uint8 next_state;
    uint8 delay;
    uint8 Lastmotorflag;//缓存上一次电机动作类型以用作电机方向切换的延时判断
    uint8 motorflag;	//鐢垫満宸ヤ綔鏍囧織  1-宸ヤ綔涓紱0-鍋滄
    uint8 rotateflag;	//杞姩鏍囧織   0-鍋滄   2-姝ｈ浆  3-鍙嶈浆
    uint16 time;
    uint16 run_t;
};

extern uint16  motor_time0025;
extern uint16  motor_time2550;
extern uint16  motor_time5075;
extern uint16  motor_time7500;
extern uint16  motordn_time0075;
extern uint16  motordn_time7550;
extern uint16  motordn_time5025;
extern uint16  motordn_time2500;

extern uint8 curtain_init;

extern uint8 curtaindegree;
extern struct MOTOR Motor1;

extern uint8 last_degree;   //上次开度控制目标开度
extern uint8 reset_cnt;     //澶嶄綅鍓嶅姩浣滄鏁�
extern uint8 reset_flag;    //澶嶄綅鏍囧織
extern uint8 action_cnt;
extern uint8 next_action_cmd;     //澶嶄綅鍚庝笅涓�娆″姩浣滄寚浠�
extern uint16 next_action_time;   //澶嶄綅鍚庝笅涓�娆″姩浣滄椂闂�
extern uint8 motor_ctrl_mode;  //椹揪鎺у埗鏂瑰紡锛屾椂闂存垨寮�搴�

extern uint8 relay_delay;
extern uint8 relay_delay_flag;
extern uint8 check_flag;
extern uint8 relay_error;

extern uint8 curtain_ctrl_cmd;

uint8 motor_ctrl(uint8 cmd, uint16 time, struct MOTOR *motor, uint8 flag);
uint8 set_motor_ctrl_inf(uint8 *buff, uint8 w_len);
void curtain_state_fun(void);
uint8 set_motor_ctrl(uint8 *buff, uint8 w_len);
void cal_degree(struct MOTOR *motor);
uint16 cal_time(uint8 dstdegree);
void relay_delay_ms(void);
void relay_check(void);
uint8 curtain_time_init(void);



#endif


