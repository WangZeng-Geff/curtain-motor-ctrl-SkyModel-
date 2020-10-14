#include "headfiles.h"
#include "smart_plc.h"
uint8 curtain_init;
uint8 curtaindegree; //��������
uint8 action_cnt;    //�����������������ڿ��Ƹ�λ
uint8 reset_cnt;     //��λǰ��������
uint8 reset_flag;    //��λ��־
uint8 motor_ctrl_mode;  //�����Ʒ�ʽ��ʱ��򿪶�
uint8 last_degree;   //�ϴο��ȿ���Ŀ�꿪��

uint8 next_action_cmd;     //��λ����һ�ζ���ָ��
uint16 next_action_time;   //��λ����һ�ζ���ʱ��

uint16  motor_time0025;
uint16  motor_time2550;
uint16  motor_time5075;
uint16  motor_time7500;

uint16  motordn_time0075;
uint16  motordn_time7550;
uint16  motordn_time5025;
uint16  motordn_time2500;

uint8 relay_delay;
uint8 relay_delay_flag;
uint8 check_flag;
uint8 relay_error;

uint8 curtain_ctrl_cmd;


struct MOTOR Motor1 = 
{
    MOTOR_F_PORT,MOTOR_F_PIN,MOTOR_R_PORT,MOTOR_R_PIN,MOTOR_OFF_PORT,MOTOR_OFF_PIN,MOTOR_ROFF,MOTOR_ROFF, 0, 0, 0, 0, 0
};


#define OFF            0x00
#define RCTRL          0x01
#define FCTRL          0x02


const static uint8 motor_run_state[]=
{//�����ת����                                       //�����ת����
    MOTOR_PWR_OFF, MOTOR_ROFF, MOTOR_FOFF, MOTOR_FON, MOTOR_F, MOTOR_PWR_OFF, MOTOR_FOFF, MOTOR_ROFF, MOTOR_RON, MOTOR_R,
};
#define MAX_MOTOR_STATE_SZ          sizeof(motor_run_state)

void relay_check(void)
{
    static uint8 check_cnt = 0;

    if ((Motor1.motorflag) || (relay_error))
        return;

    if ((GPIO_ReadInputPin(CHECK_F_PORT, CHECK_F_PIN))
        || (GPIO_ReadInputPin(CHECK_R_PORT, CHECK_R_PIN)))
        {
            check_cnt++;
        }
    else
        check_cnt = 0;

    if (check_cnt > 5)
        relay_error = 1;

    return;	
}

void relay_delay_ms(void)
{
    if (relay_delay)
    {
        relay_delay--;		
        if(relay_delay == 0)
        {
            if (_OFF == relay_delay_flag)
                SET_PIN_L(Motor1.GPIOx_off,Motor1.GPIO_Pin_off);
            else
            {
                if (!relay_error)
                    SET_PIN_H(Motor1.GPIOx_off,Motor1.GPIO_Pin_off); 
            }
        }
    }
}

uint8 find_state(uint8 cur_s, uint8 next_s)
{
    uint8 i, j;

    for(i = 0; i < MAX_MOTOR_STATE_SZ; i++)
    {
        if(motor_run_state[i] == next_s) break;
    }
    if(i >= MAX_MOTOR_STATE_SZ)  return(0);
 
    for(j = i; j>0; j--)
    {
        if(motor_run_state[j] == cur_s) break;
    }
    return(j);
}


void ctrl_relay(struct MOTOR *motor)
{
    uint8 ctrl = motor_run_state[motor->cur_state];

    motor->delay = 5;//�л�������ʱ50ms��λ
    if(MOTOR_PWR_OFF == ctrl)
    {
        relay_delay = 4;	//��ʱ����40mS
        relay_delay_flag = _OFF;
        GPIO_Init(ZCP_PORT, ZCP_PIN, GPIO_MODE_IN_FL_IT);//���ж�
        motor->delay = 14;  //����ر�ʱ����140ms
    }
    else if(MOTOR_FOFF == ctrl)
    {
			if(Motor1.Lastmotorflag){//�����һ�ζ�������ֹͣ״̬
					motor->delay = 50;  //����رպ���500ms
				}
			SET_PIN_L(motor->GPIOx_f,motor->GPIO_Pin_f);
    }
    else if(MOTOR_ROFF == ctrl)
    {
				if(Motor1.Lastmotorflag){//�����һ�ζ�������ֹͣ
						motor->delay = 50;  //����رպ�ʱ����500ms
						}
				SET_PIN_L(motor->GPIOx_r,motor->GPIO_Pin_r);
    }
    else if(MOTOR_FON == ctrl)
    {
        if (!relay_error)
            SET_PIN_H(motor->GPIOx_f,motor->GPIO_Pin_f);
    }
    else if(MOTOR_RON == ctrl)
    {
        if (!relay_error)
            SET_PIN_H(motor->GPIOx_r,motor->GPIO_Pin_r);
    }
    else if((MOTOR_F == ctrl) || (MOTOR_R == ctrl))
    {
        relay_delay = 4;	//��ʱ����40mS
        relay_delay_flag = _ON;
        GPIO_Init(ZCP_PORT, ZCP_PIN, GPIO_MODE_IN_FL_IT);//���ж�
    }
}

uint16 cal_time(uint8 dstdegree)
{
   uint8 difdegree, degree = 0;
   uint16 time = 0;

   if (curtaindegree == INVALID_DEGREE)
   {
       return 0;
   }

   if (curtaindegree > dstdegree)
   {
      degree = curtaindegree - dstdegree;

      if (curtaindegree >= 75)
      {
         difdegree = curtaindegree - 75;
         if (degree > difdegree)
         {
            time = (uint32)difdegree * (uint32)motordn_time0075 / (uint32)25;
            degree -= difdegree;

            if (degree > 25)
            {
               degree -= 25;
               time += motordn_time7550;

               if (degree > 25)
               {
                  degree -= 25;
                  time += motordn_time5025;
                  time += (uint32)degree * (uint32)motordn_time2500 / (uint32)25;
               }
               else
               {
                  time += (uint32)degree * (uint32)motordn_time5025 / (uint32)25;
               }
            }
            else
            {
               time += (uint32)degree * (uint32)motordn_time7550 / (uint32)25;
            }
         }
         else
         {
            time = (uint32)degree * (uint32)motordn_time0075 / (uint32)25;
         }
      }
      else if (curtaindegree >= 50)
      {
         difdegree = curtaindegree - 50;
         if (degree > difdegree)
         {
            time = (uint32)difdegree * (uint32)motordn_time7550 / (uint32)25;
            degree -= difdegree;

            if (degree > 25)
            {
               degree -= 25;
               time += motordn_time5025;

               time += (uint32)degree * (uint32)motordn_time2500 / (uint32)25;
            }
            else
            {
               time += (uint32)degree * (uint32)motordn_time5025 / (uint32)25;
            }
         }
         else
         {
            time = (uint32)degree * (uint32)motordn_time7550 / (uint32)25;
         }
      }
      else if (curtaindegree >= 25)
      {
         difdegree = curtaindegree - 25;
         if (degree > difdegree)
         {
            time = (uint32)difdegree * (uint32)motordn_time5025 / (uint32)25;
            degree -= difdegree;
            time += (uint32)degree * (uint32)motordn_time2500 / (uint32)25;
         }
         else
         {
            time = (uint32)degree * (uint32)motordn_time5025 / (uint32)25;
         }
      }
      else // >= 0
      {
         time = (uint32)degree * (uint32)motordn_time2500 / (uint32)25;
      }
   }

   if (curtaindegree < dstdegree)
   {
      degree = dstdegree - curtaindegree;

      if (curtaindegree <= 25)
      {
         difdegree = 25 - curtaindegree;
         if (degree > difdegree)
         {
            time = (uint32)difdegree * (uint32)motor_time0025 / (uint32)25;
            degree -= difdegree;

            if (degree > 25)
            {
               degree -= 25;
               time += motor_time2550;

               if (degree > 25)
               {
                  degree -= 25;
                  time += motor_time5075;
                  time += (uint32)degree * (uint32)motor_time7500 / (uint32)25;
               }
               else
               {
                  time += (uint32)degree * (uint32)motor_time5075 / (uint32)25;
               }
            }
            else
            {
               time += (uint32)degree * (uint32)motor_time2550 / (uint32)25;
            }
         }
         else
         {
            time = (uint32)degree * (uint32)motor_time0025 / (uint32)25;
         }
      }
      else if (curtaindegree <= 50)
      {
         difdegree = 50 - curtaindegree;
         if (degree > difdegree)
         {
            time = (uint32)difdegree * (uint32)motor_time2550 / (uint32)25;
            degree -= difdegree;

            if (degree > 25)
            {
               degree -= 25;
               time += motor_time5075;

               time += (uint32)degree * (uint32)motor_time7500 / (uint32)25;
            }
            else
            {
               time += (uint32)degree * (uint32)motor_time5075 / (uint32)25;
            }
         }
         else
         {
            time = (uint32)degree * (uint32)motor_time2550 / (uint32)25;
         }
      }
      else if (curtaindegree <= 75)
      {
         difdegree = 75 - curtaindegree;
         if (degree > difdegree)
         {
            time = (uint32)difdegree * (uint32)motor_time5075 / (uint32)25;
            degree -= difdegree;
            time += (uint32)degree * (uint32)motor_time7500 / (uint32)25;
         }
         else
         {
            time = (uint32)degree * (uint32)motor_time5075 / (uint32)25;
         }
      }
      else // <= 100
      {
         time = (uint32)degree * (uint32)motor_time7500 / (uint32)25;
      }
   }

   return time;

}

uint16 time2degree(uint32 time, uint32 total_degree, uint32 degree_time)
{
   if (time > degree_time)
   {
      return (uint16)total_degree;
   }
   return (uint16)(((time + 2) * total_degree) / degree_time); 
}

void cal_degree(struct MOTOR *motor)
{
   uint16 degree = 0, difdegree;
   uint16 diftime;


   if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))
      return;

    if (motor->rotateflag == MOTOR_FORWARD)
    {
        if (curtaindegree > 100)    //��Ч����
        {
            if ((eep_param.motor_up_time) && (eep_param.motor_dn_time)) //ת��ʱ�䳬��1.25���г�ʱ��
            {
                if (motor->run_t >= (eep_param.motor_up_time + (eep_param.motor_up_time >> 2) - 6)) // ��ȥһ����С��������߳���ɿ���
                {
                    curtaindegree = 100; 
                }
                return;
            }

           if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))  //δ�����г�ʱ��
           {
               if ((Motor1.run_t >= MAX_RUN_TIME)) //ת��ʱ�䳬�������תʱ��
               {
                   curtaindegree = 100;
               }
               return;
           }
        }
        else if (curtaindegree >= 75) // 75 < curtaindegree < 100
       {
          degree = time2degree(motor->run_t, 25, motor_time7500);
       }
       else if (curtaindegree >= 50) // 50 < curtaindegree < 75
       {
          difdegree = 75 - curtaindegree;
          diftime = (uint32)difdegree * (uint32)motor_time5075 / (uint32)25;

          if (motor->run_t > diftime)
          {
             degree = difdegree;
             motor->run_t -= diftime;
             degree += time2degree(motor->run_t, 25, motor_time7500);
          }
          else
          {
             degree = time2degree(motor->run_t, 25, motor_time5075);
          }
       }
       else if (curtaindegree >= 25) // 25 < curtaindegree < 50
       {
          difdegree = 50 - curtaindegree;
          diftime = (uint32)difdegree * (uint32)motor_time2550 / (uint32)25;

          if (motor->run_t >= diftime)
          {
             degree = difdegree;
             motor->run_t -= diftime;
             if (motor->run_t >= motor_time5075)
             {
                degree += 25;
                motor->run_t -= motor_time5075;
                degree += time2degree(motor->run_t, 25, motor_time7500);
             }
             else
             {
                degree += time2degree(motor->run_t, 25, motor_time5075);
             }
          }
          else
          {
             degree = time2degree(motor->run_t, 25, motor_time2550);
          }
       }
       else // 0 < curtaindegree < 25
       {
          difdegree = 25 - curtaindegree;
          diftime = (uint32)difdegree * (uint32)motor_time0025 / (uint32)25;

          if (motor->run_t >= diftime)
          {
             degree = difdegree;
             motor->run_t -= diftime;
             if (motor->run_t >= motor_time2550)
             {
                degree += 25;
                motor->run_t -= motor_time2550;

                if (motor->run_t >= motor_time5075) 
                {
                   degree += 25;
                   motor->run_t -= motor_time5075;
                   degree += time2degree(motor->run_t, 25, motor_time7500);
                }
                else
                {
                   degree += time2degree(motor->run_t, 25, motor_time5075);
                }
             }
             else
             {
                degree += time2degree(motor->run_t, 25, motor_time2550);
             }
          }
          else
          {
             degree = time2degree(motor->run_t, 25, motor_time0025);
          }
       }

       motor->run_t = 0;

       if (curtaindegree == 100)
       {
          return;
       }

       if (((uint8)(curtaindegree + degree)) > 100)
       {
          curtaindegree = 100;
       }
       else
       {
          curtaindegree += (uint8)degree; 
       }
    }

    if (motor->rotateflag == MOTOR_REVERSE)
    {
       if (curtaindegree <= 25)
       {
          degree = time2degree(motor->run_t, 25, motordn_time2500);
       }
       else if (curtaindegree <= 50)
       {
          difdegree = curtaindegree - 25;
          diftime = (uint32)difdegree * (uint32)motordn_time5025 / (uint32)25;

          if (motor->run_t > diftime)
          {
             degree = difdegree;
             motor->run_t -= diftime;
             degree += time2degree(motor->run_t, 25, motordn_time2500);
          }
          else
          {
             degree = time2degree(motor->run_t, 25, motordn_time5025);
          }
       }
       else if (curtaindegree <= 75)
       {
          difdegree = curtaindegree - 50;
          diftime = (uint32)difdegree * (uint32)motordn_time7550 / (uint32)25;

          if (motor->run_t >= diftime)
          {
             degree = difdegree;
             motor->run_t -= diftime;
             if (motor->run_t >= motordn_time5025)
             {
                degree += 25;
                motor->run_t -= motordn_time5025;
                degree += time2degree(motor->run_t, 25, motordn_time2500);
             }
             else
             {
                degree += time2degree(motor->run_t, 25, motordn_time5025);
             }
          }
          else
          {
             degree = time2degree(motor->run_t, 25, motordn_time7550);
          }
       }
       else if (curtaindegree <= 100) // < 100
       {
          difdegree = curtaindegree - 75;
          diftime = (uint32)difdegree * (uint32)motordn_time0075 / (uint32)25;

          if (motor->run_t >= diftime)
          {
             degree = difdegree;
             motor->run_t -= diftime;
             if (motor->run_t >= motordn_time7550)
             {
                degree += 25;
                motor->run_t -= motordn_time7550;

                if (motor->run_t >= motordn_time5025) 
                {
                   degree += 25;
                   motor->run_t -= motordn_time5025;
                   degree += time2degree(motor->run_t, 25, motordn_time2500);
                }
                else
                {
                   degree += time2degree(motor->run_t, 25, motordn_time5025);
                }
             }
             else
             {
                degree += time2degree(motor->run_t, 25, motordn_time7550);
             }
          }
          else
          {
             degree = time2degree(motor->run_t, 25, motordn_time0075);
          }
       }
       else   //��Ч����
       {
            if ((eep_param.motor_up_time) && (eep_param.motor_dn_time)) //ת��ʱ�䳬��1.25���г�ʱ��
            {
                if (motor->run_t >= (eep_param.motor_dn_time + (eep_param.motor_dn_time >> 2) - 6)) // ��ȥһ����С��������߳���ɿ���
                {
                    curtaindegree = 0; 
                }
                return;
            }

           if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))  //δ�����г�ʱ��
           {
               if (Motor1.run_t >= (MAX_RUN_TIME - 6)) //ת��ʱ�䳬�������תʱ��
               {
                   curtaindegree = 0;
               }
               return;
           }
       }


       motor->run_t = 0;

       if (curtaindegree == 0)
       {
          return;
       }

       if (curtaindegree > (uint8)degree)
       {
          curtaindegree -= (uint8)degree;
       }
       else
       {
          curtaindegree = 0;
       }
    }

    return;
}

void curtain_ctrl(struct MOTOR *motor)
{
   uint8 degree;

    if(((MOTOR_FOFF == motor_run_state[motor->cur_state]) && (MOTOR_FOFF == motor_run_state[motor->next_state]))
       || ((MOTOR_ROFF == motor_run_state[motor->cur_state]) && (MOTOR_ROFF == motor_run_state[motor->next_state])))
    {
      if (motor->motorflag)  //�������ת����
      {
         cal_degree((struct MOTOR *)&Motor1);

         if ((curtaindegree == 100) || (curtaindegree == 0))
         {
            action_cnt = 0;

            //���ģ�����Ӳ���
            eep_param.ActionCount = 0;//У׼��������
						EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
										 (uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));

            reset_flag = 0;
         }
         else
         {
//            if (motor_ctrl_mode == MOTOR_DEGREE_CTRL)
            {
//���ģ�����Ӳ���
                eep_param.ActionCount ++;//У׼��������
								EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
												 (uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));
                action_cnt++; 
                if(eep_param.ActionCount >= eep_param.ForceCaliTime && 1 == eep_param.SkyModelContral)  //�������ģ�Ϳ�������
                {
                    reset_flag = 1;
                } else if(action_cnt >= reset_cnt)  {
                    reset_flag = 1;
                }
            }
         }

         motor_ctrl_mode = 0;
         motor->time = 0;
         motor->run_t = 0;
				 //��ȡ�豸ָ���ǰ�ϵ���������Ͳ�����
         motor->Lastmotorflag = motor->motorflag;
         motor->motorflag = 0;
         motor->rotateflag = 0;
         last_degree = curtaindegree;

         eep_param.degree = curtaindegree;
         eep_param.acting = 0;
         EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));

         if ((next_action_cmd == 0) && (eep_param.motor_up_time) && (eep_param.motor_dn_time) && (curtain_init != POWERON_INIT))
         {
            stage_data.state_change_mode = P2P;
            stage_restart_data_init(REPORT_START); 
            stage_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //���13 + 2*GIDOrder (100us)
         }

         curtain_init = 0;
         
      }

       return; 
    }

    if(motor->delay > 0)
    {
        motor->delay--;
        if(motor->delay) return;
        if(motor->cur_state < motor->next_state)
        {
            motor->cur_state++;
            ctrl_relay(motor);
        }                 
        return;
    }

    if(motor->run_t > MAX_RUN_TIME)
    {
        motor->time = 0;
    }

    if(motor->time > 0)
    {
        motor->time--;
        motor->run_t++;
        return;
    }

    if (motor->rotateflag == MOTOR_FORWARD)
    {
        motor->next_state = find_state(MOTOR_FOFF, MOTOR_FOFF); 
        motor->cur_state = find_state(MOTOR_PWR_OFF, MOTOR_FOFF);
    }
    else
    {
        motor->next_state = find_state(MOTOR_ROFF, MOTOR_ROFF); 
        motor->cur_state = find_state(MOTOR_PWR_OFF, MOTOR_ROFF);
    }
    
    ctrl_relay(motor);
    
    return;
}

void curtain_state_fun(void)
{
    curtain_ctrl((struct MOTOR *)&Motor1);    
}


uint8 motor_ctrl(uint8 cmd, uint16 time, struct MOTOR *motor, uint8 flag)
{
   uint8 degree;

   if (motor_ctrl_mode == MOTOR_INIT_CTRL)
   {
      return 1;
   }

   motor_ctrl_mode = flag;

    switch(cmd)
    {
    case MOTOR_STOP:  //�����ͣ
        motor->cur_state = motor->next_state;  
        motor->delay = 0;
        motor->time = 0;
        return(0);

    case MOTOR_FORWARD:    //�����ת
        motor->next_state = find_state(MOTOR_F, MOTOR_F);
        motor->cur_state = find_state(MOTOR_PWR_OFF, MOTOR_F); 
        ctrl_relay(motor);
        motor->time = time;
        motor->run_t = 0;
				 //��ȡ�豸ָ���ǰ�ϵ���������Ͳ�����
         motor->Lastmotorflag = motor->motorflag;
        motor->motorflag = 1;
        motor->rotateflag = MOTOR_FORWARD;
        break;

    case MOTOR_REVERSE:     //�����ת 
        motor->next_state = find_state(MOTOR_R, MOTOR_R);
        motor->cur_state = find_state(MOTOR_PWR_OFF, MOTOR_R); 
        ctrl_relay(motor);
        motor->time = time;
        motor->run_t = 0;
				 //��ȡ�豸ָ���ǰ�ϵ���������Ͳ�����
        motor->Lastmotorflag = motor->motorflag;
        motor->motorflag = 1;
        motor->rotateflag = MOTOR_REVERSE;
        break;

    default:
        return(1);
    }

    if (motor->motorflag)
    {
        eep_param.acting = 1;
        EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, acting),
             (uint8 *)&(eep_param.acting), sizeof(eep_param.acting));


    }
    return(0);
}


uint8 set_motor_ctrl(uint8 *buff, uint8 w_len)
{
    uint8 ret, cabri = 0, cmd = buff[0];
    uint16 time = 0;

    if(w_len != 0x01) return(LEN_ERR);

    if ((cmd > 3) || (cmd < 1))
       return(DATA_ERR);

    if (eep_param.enlock && eep_param.lock)
    {
        curtain_ctrl_cmd = 1;
        if (cmd_is_boardcast) 
       {
					enlock_restart_data_init(REPORT_START); 
					enlock_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //���2S
          return ENLOCK_ERR;
       }

       if (taker_is_gateway)
       {
           enlock_restart_data_init(REPORT_START); 
           enlock_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //���2S
           return ENLOCK_ERR;
       }
    }

    if ((curtain_init != 0) || (motor_ctrl_mode == MOTOR_INIT_CTRL))
       return DEV_BUSY;

   if ((cmd != MOTOR_STOP) && (Motor1.motorflag))  //�������ת����
   {
       if (curtaindegree == INVALID_DEGREE) //�ϴζ����е��磬�ϵ����δУ׼����ֵ
       {
           if ((eep_param.motor_up_time) && (eep_param.motor_dn_time))  //��������г�ʱ��
           {
               if ((Motor1.rotateflag == MOTOR_FORWARD) 
                   && (Motor1.run_t >= eep_param.motor_up_time)) //��ת������ת��ʱ�䳬���г�ʱ��
               {
                   curtaindegree = 100;
                   eep_param.degree = 100;
                   eep_param.acting = 0;
                   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
                             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));
               }

               if ((Motor1.rotateflag == MOTOR_REVERSE) 
                   && (Motor1.run_t >= eep_param.motor_dn_time)) //��ת��������ת��ʱ�䳬���г�ʱ��
               {
                   curtaindegree = 0;
                   eep_param.degree = 0;
                   eep_param.acting = 0;
                   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
                             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));
               }
           }

           if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))  //���δ�����г�ʱ��
           {
               if ((Motor1.rotateflag == MOTOR_FORWARD) && (Motor1.run_t >= MAX_RUN_TIME)) //��ת������ת��ʱ�䳬�������תʱ��
               {
                   curtaindegree = 100;
                   eep_param.degree = 100;
                   eep_param.acting = 0;
                   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
                             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));
               }

               if ((Motor1.rotateflag == MOTOR_REVERSE) && (Motor1.run_t >= MAX_RUN_TIME)) //��ת������ת��ʱ�䳬�����תʱ��
               {
                   curtaindegree = 0;
                   eep_param.degree = 0;
                   eep_param.acting = 0;
                   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
                             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));
               }
           }
       }

        else
        {
            cal_degree((struct MOTOR *)&Motor1);
        }

          if ((curtaindegree == 0) || (curtaindegree == 100))
          {

              //���ģ�����Ӳ���
              eep_param.ActionCount = 0;//У׼��������
						  EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
											 (uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));

             action_cnt = 0; 
             reset_flag = 0;
          }
          else
          {
             if ((motor_ctrl_mode == MOTOR_DEGREE_CTRL) || (cmd != Motor1.rotateflag))
             {
//���ģ�����Ӳ���
             eep_param.ActionCount ++;//У׼��������
						 EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
											(uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));
             action_cnt++; 
             if (eep_param.ActionCount > eep_param.ForceCaliTime && 1 == eep_param.SkyModelContral)  //�������ģ�Ϳ�������
             {
                reset_flag = 1;
             } else if(action_cnt >= reset_cnt) {
                    reset_flag = 1;
             }
             }
          }
   }
   
    if (cmd == MOTOR_FORWARD)
    {
       last_degree = 100; //Ŀ�꿪��

       if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0)) //δ�����г�ʱ��
           time = MAX_RUN_TIME;
       else
       {
           if (curtaindegree == 100)
           {
              return(NO_ERR);
           }
           else if (curtaindegree == INVALID_DEGREE)
           {
              time = eep_param.motor_up_time + (eep_param.motor_up_time >> 2);
           }
           else
           {
               time = cal_time(100); 
               time += eep_param.motor_up_time >> 2;
           }
       }
    }

    if (cmd == MOTOR_REVERSE)
    {
       last_degree = 0; //Ŀ�꿪��

       if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0)) //δ�����г�ʱ��
           time = MAX_RUN_TIME;
       else
       {
           if (curtaindegree == 0)
           {
              return(NO_ERR);
           }
           else if (curtaindegree == INVALID_DEGREE)
           {
              time = eep_param.motor_dn_time + (eep_param.motor_dn_time >> 2);
           }
           else
           {
               time = cal_time(0); 
               time += eep_param.motor_dn_time >> 2;
           }
       }
    }

    next_action_cmd = 0;
    next_action_time = 0;

    if ((eep_param.motor_up_time) && (eep_param.motor_dn_time) && (cmd != MOTOR_STOP)){
       stage_restart_data_init(REPORT_START);
       stage_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //���13 + 2*GIDOrder (100us)
    }

    if ((Motor1.motorflag == 1) && (cmd == Motor1.rotateflag))
    {
       Motor1.time = time;
       Motor1.run_t = 0;
       return(NO_ERR);
    }
    else
    {
       ret = motor_ctrl(cmd, time, (struct MOTOR *)&Motor1, MOTOR_TIME_CTRL); 
    }

    if(ret) 
       return(DATA_ERR);

    return(NO_ERR);
}

uint8 set_motor_ctrl_inf(uint8 *buff, uint8 w_len)
{
    uint16 time;
    uint8 cmd, style, ret=0;

    if(w_len != 0x04) return(LEN_ERR);

    if (eep_param.enlock && eep_param.lock)
    {
        curtain_ctrl_cmd = 1;
        if (cmd_is_boardcast) 
       {
					enlock_restart_data_init(REPORT_START); 
					enlock_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //���2S
          return ENLOCK_ERR;
       }

       if (taker_is_gateway)
       {
           enlock_restart_data_init(REPORT_START); 
           enlock_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //���2S
           return ENLOCK_ERR;
       }
    }

    if ((curtain_init != 0) || (motor_ctrl_mode == MOTOR_INIT_CTRL))
       return DEV_BUSY;

    style = buff[0];
    cmd = buff[1] & 0x0F;
    time = buff[3]*256 + buff[2];

    if(style)    //���ǽ�����������ش���
    {
        return(DATA_ERR);
    }

   if ((cmd != MOTOR_STOP) && (Motor1.motorflag))  //�������ת����
   {
       if (curtaindegree == INVALID_DEGREE) //�ϴζ����е��磬�ϵ����δУ׼����ֵ
       {
           if ((eep_param.motor_up_time) && (eep_param.motor_dn_time))  //��������г�ʱ��
           {
               if ((Motor1.rotateflag == MOTOR_FORWARD) 
                   && (Motor1.run_t >= eep_param.motor_up_time)) //��ת������ת��ʱ�䳬���г�ʱ��
               {
                   curtaindegree = 100;
                   eep_param.degree = 100;
                   eep_param.acting = 0;
                   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
                             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));
               }

               if ((Motor1.rotateflag == MOTOR_REVERSE) 
                   && (Motor1.run_t >= eep_param.motor_dn_time)) //��ת������ת��ʱ�䳬���г�ʱ��
               {
                   curtaindegree = 0;
                   eep_param.degree = 0;
                   eep_param.acting = 0;
                   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
                             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));
               }
           }

           if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))  //���δ�����г�ʱ��
           {
               if ((Motor1.rotateflag == MOTOR_FORWARD) && (Motor1.run_t >= MAX_RUN_TIME)) //��ת������ת��ʱ�䳬�������תʱ��
               {
                   curtaindegree = 100;
                   eep_param.degree = 100;
                   eep_param.acting = 0;
                   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
                             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));
               }

               if ((Motor1.rotateflag == MOTOR_REVERSE) && (Motor1.run_t >= MAX_RUN_TIME)) //��ת������ת��ʱ�䳬�����תʱ��
               {
                   curtaindegree = 0;
                   eep_param.degree = 0;
                   eep_param.acting = 0;
                   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, degree),
                             (uint8 *)&(eep_param.degree), sizeof(eep_param.degree) + sizeof(eep_param.acting));
               }
           }
       }
       else
       {
          cal_degree((struct MOTOR *)&Motor1);
       }

      if ((curtaindegree == 0) || (curtaindegree == 100))
      {

          //���ģ�����Ӳ���
             eep_param.ActionCount = 0;//У׼��������
						 EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
											(uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));
         action_cnt = 0; 
         reset_flag = 0;
      }
      else
      {
         if ((motor_ctrl_mode == MOTOR_DEGREE_CTRL) || (cmd != Motor1.rotateflag))
         {
//���ģ�����Ӳ���
             eep_param.ActionCount ++;//У׼��������
						 EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
											(uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));
             action_cnt++; 
             if (eep_param.ActionCount > eep_param.ForceCaliTime && 1 == eep_param.SkyModelContral)  //�������ģ�Ϳ�������
             {
                reset_flag = 1;
             } else if (action_cnt >= reset_cnt) {
                reset_flag = 1;
             }
         }
      }
   }
    
    if (cmd == MOTOR_FORWARD)
    {
       last_degree = 100;

#if 0
       if ((eep_param.motor_up_time) && (eep_param.motor_dn_time) && (curtaindegree == 100)) //�������г�ʱ�䣬���Ҵ����Ѿ��������
            return(NO_ERR);

       if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0)) //δ�����г�ʱ��
           time = MAX_RUN_TIME;
       else
       {
           if (curtaindegree == 100)
           {
              return(NO_ERR);
           }
           else if (curtaindegree == INVALID_DEGREE)
           {
              time = eep_param.motor_up_time + (eep_param.motor_up_time >> 2);
           }
           else
           {
               time = cal_time(100); 
               time += eep_param.motor_up_time >> 2;
           }
       }
#endif

       if ((eep_param.motor_up_time) && (eep_param.motor_dn_time))
       {
           if (curtaindegree == 100)
           {
              return(NO_ERR);
           }
           else if (curtaindegree == INVALID_DEGREE)
           {
              time = eep_param.motor_up_time + (eep_param.motor_up_time >> 2);
           }
           else
           {
               time = cal_time(100); 
               time += eep_param.motor_up_time >> 2;
           }
       }
    }
    if (cmd == MOTOR_REVERSE)
    {
       last_degree = 0;

#if 0
       if ((eep_param.motor_up_time) && (eep_param.motor_dn_time) && (curtaindegree == 0)) //�������г�ʱ�䣬���Ҵ����Ѿ��������
            return(NO_ERR);

       if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0)) //δ�����г�ʱ��
           time = MAX_RUN_TIME;
       else
       {
           if (curtaindegree == 0)
           {
              return(NO_ERR);
           }
           else if (curtaindegree == INVALID_DEGREE)
           {
              time = eep_param.motor_dn_time + (eep_param.motor_dn_time >> 2);
           }
           else
           {
               time = cal_time(0); 
               time += eep_param.motor_dn_time >> 2;
           }
       }
#endif

       if ((eep_param.motor_up_time) && (eep_param.motor_dn_time))
       {
           if (curtaindegree == 0)
           {
              return(NO_ERR);
           }
           else if (curtaindegree == INVALID_DEGREE)
           {
              time = eep_param.motor_dn_time + (eep_param.motor_dn_time >> 2);
           }
           else
           {
               time = cal_time(0); 
               time += eep_param.motor_dn_time >> 2;
           }
       }
    }

    switch(buff[1]&0xF0)    //ͨ����
    {
    case 0x00:
    case 0x10:
        next_action_cmd = 0;
        next_action_time = 0;

        if ((eep_param.motor_up_time) && (eep_param.motor_dn_time) && (cmd != MOTOR_STOP)){
           stage_restart_data_init(REPORT_START); 
           stage_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //���13 + 2*GIDOrder (100us)
        }

        if ((Motor1.motorflag == 1) && (cmd == Motor1.rotateflag))
         {
           Motor1.time = time;
           Motor1.run_t = 0;
           return(NO_ERR);
         }
        else
        {
           ret = motor_ctrl(cmd, time, (struct MOTOR *)&Motor1, MOTOR_TIME_CTRL);
        }
        break;

    default:
        return(DATA_ERR);
    }

    if(ret) 
       return(DATA_ERR);

    return(NO_ERR);
}

uint8 curtain_time_init(void)
{
    uint16 motor_time, cali_up, cali_dn;

    if (eep_param.motor_up_time >= 5000)
    {
       cali_up = 200;
       cali_dn = 120;
       reset_cnt = 13;
    }
    else if (eep_param.motor_up_time >= 4200)
    {
       cali_up = 100;
       cali_dn = 60;
       reset_cnt = 13;
    }
    else if (eep_param.motor_up_time >= 3400)
    {
       cali_up = 50;
       cali_dn = 30;
       reset_cnt = 10;
    }
    else if (eep_param.motor_up_time >= 2600)
    {
       cali_up = 20;
       cali_dn = 15;
       reset_cnt = 7;
    }
    else
    {
       cali_up = 10;
       cali_dn = 5;
       reset_cnt = 7;
    }
    motor_time = eep_param.motor_up_time - (6 * cali_up);
    motor_time7500 = motor_time / 4;
    motor_time5075 = motor_time7500 + cali_up;
    motor_time2550 = motor_time7500 + 2 * cali_up;
    motor_time0025 = motor_time7500 + 3 * cali_up;

    motor_time = eep_param.motor_dn_time - (6 * cali_dn);

    motordn_time0075 = motor_time / 4;
    motordn_time7550 = motordn_time0075 + cali_dn;
    motordn_time5025 = motordn_time0075 + 2 * cali_dn;
    motordn_time2500 = motordn_time0075 + 3 * cali_dn;

    return 0;
}


#if 0
void relay_forward(struct MOTOR *motor)
{
   SET_PIN_H(motor->GPIOx_f_r,motor->GPIO_Pin_f_r);
   SET_PIN_H(motor->GPIOx_off,motor->GPIO_Pin_off);
   return;
}

void relay_reverse(struct MOTOR *motor)
{
   SET_PIN_L(motor->GPIOx_f_r,motor->GPIO_Pin_f_r);
   SET_PIN_H(motor->GPIOx_off,motor->GPIO_Pin_off);
   return;
}


void relay_stop(struct MOTOR *motor)
{
   SET_PIN_L(motor->GPIOx_f_r,motor->GPIO_Pin_f_r);
   SET_PIN_L(motor->GPIOx_off,motor->GPIO_Pin_off);
   return;
}

#endif





