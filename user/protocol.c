
#include "headfiles.h"
#include "smart_plc.h"



#define scat_flag(f) {(uint8)(f), (uint8)(f>>8)}
const static uint8 soft_ver[]  = "EASTSOFT(v1.0)";
const static uint8 dev_infor[] = "ESACT-CC1-AC-A-63(v1.4)-20200308";
uint8 WaiteTime = 0xff;

struct SHS_frame* get_smart_frame(uint8 rxframe_raw[], uint8 rxlen)
{
   struct SHS_frame *pframe;
   uint8 i = 0;
   uint8 len;

start_lbl:
   while (i < rxlen)
   {
      if (STC == rxframe_raw[i]) break;
      i++;
   }
//   if(i >= rxlen) return(NULL);
   if (rxlen - i < SHS_FRAME_HEAD) return (NULL); //接收等待length长度
   pframe = (struct SHS_frame *)&rxframe_raw[i];
   len = pframe->length;

   if (i + SHS_FRAME_HEAD + len + 1 > rxlen)
   {
      i++;
      goto start_lbl;
   }

   if (pframe->infor[len] != checksum((uint8 *)pframe, len + SHS_FRAME_HEAD))
   {
      i++;
      goto start_lbl;
   }
//    uart_read(rxframe_raw, i+SHS_FRAME_HEAD+len+1);
//    mymemcpy(&rxframe_raw[0], &rxframe_raw[i], SHS_FRAME_HEAD+len+1);
   pframe = (struct SHS_frame *)&rxframe_raw[i];
   return (pframe);
}

/*************************************************
                读取设备类型
*************************************************/
#define     LIGHT_CTRL      0x0013
#define     DIMMING_DEV     0x0014
#define     COLOR_DEV       0x0015
#define     PLUG_DEV        0x0018
#define     CURTAIN_DEV     0x0004
#define     CURTAIN_DEV_II  0x0004

#define     ES      0xFFFF

#if PWRCTRL_86
const uint8 _device_type[8] = { 0xFF, 0xFF, 0x18, 0x00, 0x01, 0x00, 0x01, 0x00 };
#elif LIGHTCTRL_2
const uint8 _device_type[8] = { 0xFF, 0xFF, 0x13, 0x00, 0x00, 0x00, 0x02, 0x00 };
#elif DIMMER|DIMMER_LIGHT
const uint8 _device_type[8] = { 0xFF, 0xFF, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00 };
#elif COLOR_DIMMER
const uint8 _device_type[8] = { 0xFF, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x01, 0x00 };
#elif CURTAIN_MOTOR

#if DOUBLE_CURTAIN
const uint8 _device_type[8] = { 0xFF, 0xFF, 0x04, 0x00, 0x02, 0x00, 0x02, 0x00 };
#else
const uint8 _device_type[8] = { 0xFF, 0xFF, 0x04, 0x00, 0x01, 0x00, 0x03, 0x00 };
#endif
#endif
void _get_dev_type(uint8 *buff)
{
   mymemcpy(buff, _device_type, sizeof(_device_type));
}

static uint8 get_device_type(uint8 *buff, uint8 max_len)
{
   if (max_len < 8) return (0);
   _get_dev_type(&buff[1]);

   return (0x08);
}


uint8 compare_soft_ver(uint8 *buff, uint8 len)
{
   return (memcmp_my((uint8 *)dev_infor, buff, strlen((char const *)dev_infor)));
}

static uint8 get_dkey(uint8 *buff, uint8 max_len)
{
   struct ENCODE_PARAM encode_param;

   if (max_len < DKEY_LEN) return (0);

   EEP_Read((ENCODE_MAGIC_ADDR - OF_EEPROM_ADDRESS), (uint8 *)&encode_param, sizeof(struct ENCODE_PARAM));
   if (ENCODE_MAGIC == encode_param.sole_magic)
   {
      mymemcpy(&buff[1], encode_param.dev.dkey, DKEY_LEN);
      return (DKEY_LEN);
   }

   buff[1] = DATA_ERR;
   buff[2] = 0x00;
   return (0x82);
}

uint8 _get_dev_infor(uint8 *buff)
{
   mymemcpy(&buff[0], (uint8 *)dev_infor, strlen(dev_infor));
   return (strlen(dev_infor));
}

static uint8 get_device_attribute(uint8 *buff, uint8 max_len)
{
   uint8 len;

   EEP_Read(ENCODE_PARAM_ADDR - OF_EEPROM_ADDRESS + ENCODE_LEN, (uint8 *)&len, 1);

   if ((len < 1) || (len > MAX_ATTRIBUTE_LEN))
   {
      buff[1] = DATA_ERR;
      buff[2] = 0x00;
      return (0x82);
   }

   EEP_Read(ENCODE_PARAM_ADDR - OF_EEPROM_ADDRESS + offset_of(struct DEV_INFOR, infor), (uint8 *)&buff[1], len);
   return (len);
}

static uint8 get_sn(uint8 *buff, uint8 max_len)
{
   struct ENCODE_PARAM encode_param;

   if (max_len < SN_LEN) return (0);

   EEP_Read((ENCODE_MAGIC_ADDR - OF_EEPROM_ADDRESS), (uint8 *)&encode_param, sizeof(struct ENCODE_PARAM));
   if (ENCODE_MAGIC == encode_param.sole_magic)
   {
      mymemcpy(&buff[1], encode_param.dev.sn, SN_LEN);
      return (SN_LEN);
   }

   buff[1] = DATA_ERR;
   buff[2] = 0x00;
   return (0x82);
}




/*******************************************************
                应用层通信协议及版本
********************************************************/


static uint8 get_string(uint8 *buff, uint8 max_len, uint8 *str)
{
   if (max_len < strlen(str)) return (0);

   mymemcpy(&buff[1], (uint8 *)str, strlen(str));
   return (strlen(str));
}
/******************************************************
    设备描述信息设备制造商
    **************************************************/
static uint8 get_dev_infor(uint8 *buff, uint8 max_len)
{
   return (get_string(buff, max_len, dev_infor));
}
/*******************************************************/
static uint8 get_soft_ver(uint8 *buff, uint8 max_len)
{
   return (get_string(buff, max_len, soft_ver));
}

static uint8 set_password(uint8 *buff, uint8 w_len)
{
   uint8 i = 0, ret = 0;

   if (w_len != 0x02)
   {
      return (DATA_ERR);
   }
   if ((VALID_DATA == eep_param.pwd_magic)
       && (0x00 == memcmp_my(eep_param.password, buff, sizeof(eep_param.password))))
   {
      ret = 1;
   }
   else if (INVALID_DATA == eep_param.pwd_magic)
   {
      ret = 1;
      mymemcpy(eep_param.password, buff, sizeof(eep_param.password));
      eep_param.pwd_magic = VALID_DATA;
      EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
   }
   buff[i++] = 0x30;
   buff[i++] = 0xC0;
   buff[i++] = 0x01;
   buff[i++] = ret;
   return (CHG_DID | i);
}


uint8 get_report_enable_infor(uint8 *buff, uint8 max_len)
{
   if (max_len < 0x01) return (0);

   buff[1] = 0x00;
   buff[2] = eep_param.report_enable;
   return (0x02);
}

uint8 set_report_enable_infor(uint8 *buff, uint8 w_len)
{
   if ((w_len < 0x02) || (buff[1] > 0x03)) return (DATA_ERR); //窗帘只将状态上报网关

   eep_param.report_enable = buff[1];
   EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));

   return (NO_ERR);
}

uint8 set_motor_time(uint8 *buff, uint8 w_len)
{
    uint16 uptime, dntime, acttime;

   if (w_len != 0x04) return (DATA_ERR);

   uptime = buff[1] * 256 + buff[0];
   dntime = buff[3] * 256 + buff[2];

   if ((uptime < MIN_RUN_TIME) || (dntime < MIN_RUN_TIME))
       return (DATA_ERR);

   if ((uptime > MAX_RUN_TIME) || (dntime > MAX_RUN_TIME))
       return (DATA_ERR);

   if ((LOCK_ENABLE == eep_param.enlock) && (LOCK_ENABLE == eep_param.lock))
   {
      return (ENLOCK_ERR);
   }

   if (dntime > eep_param.motor_dn_time)
   {
       acttime = dntime + (dntime >> 2);
   }
   else
   {
       acttime = eep_param.motor_dn_time + (eep_param.motor_dn_time >> 2);
   }

   eep_param.motor_up_time = uptime;
   eep_param.motor_dn_time = dntime;

   EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, motor_up_time),
             (uint8 *)&(eep_param.motor_up_time), 2 * sizeof(eep_param.motor_up_time));


    curtain_time_init();

    curtain_init = MOTORTIME_INIT;

    next_action_cmd = 0;
    next_action_time = 0;

//天空模型增加操作
    eep_param.ActionCount = 0;//校准后动作计数
		EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
						 (uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));

    action_cnt = 0; 
    reset_flag = 0;
    stage_restart_data_init(REPORT_OVER);

    last_degree = 0; 
    curtaindegree = 100;
    motor_ctrl(MOTOR_REVERSE, acttime, (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL);

    return (NO_ERR);
}

uint8 get_motor_time(uint8 *buff, uint8 max_len)
{
   if (max_len < 4) return (0);

   buff[2] = eep_param.motor_up_time >> 8;
   buff[1] = eep_param.motor_up_time & 0xFF;
   buff[4] = eep_param.motor_dn_time >> 8;
   buff[3] = eep_param.motor_dn_time & 0xFF;

   return (4);
}

uint8 set_curtain_degree(uint8 *buff, uint8 w_len)
{
   uint8 react = 0, ret, cmd, degree, percent = buff[0];
   uint16 time;

   if ((w_len != 0x01) || (percent > 100)) return (DATA_ERR);

   if (eep_param.enlock && eep_param.lock)
   {
       curtain_ctrl_cmd = 1;

       if (cmd_is_boardcast) 
       {
           enlock_restart_data_init(REPORT_START); 
           enlock_data.wait_cnt = 13 + 5 * stage_data.equipment_gid;  //间隔2S
          return ENLOCK_ERR;
       }

       if (taker_is_gateway)
       {
           enlock_restart_data_init(REPORT_START); 
           enlock_data.wait_cnt = 13 + 5 * stage_data.equipment_gid;  //间隔2S
           return ENLOCK_ERR;
       }
   }

   if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))
      return (DATA_ERR);

   if ((curtain_init != 0) || (motor_ctrl_mode == MOTOR_INIT_CTRL))
       return DEV_BUSY;

   

   if (Motor1.motorflag)  //电机正在转动中
   {
       if (curtaindegree != INVALID_DEGREE) //当前开度有效
       {
          if (percent == last_degree)
          {
             return (NO_ERR);
          }

          cal_degree((struct MOTOR *)&Motor1); 

          if ((curtaindegree == 0) || (curtaindegree == 100))
          {

              //天空模型增加操作
              eep_param.ActionCount = 0;//校准后动作计数
						  EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
											 (uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));

              action_cnt = 0; 
              reset_flag = 0;
          }
          else
          {

//天空模型增加操作
             eep_param.ActionCount ++;//校准后动作计数
						 EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
											(uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));
             action_cnt++; 
             if (action_cnt >= reset_cnt && 0 == eep_param.SkyModelContral)  //增加天空模型控制条件
             {
                reset_flag = 1;
             } else if (eep_param.ActionCount > eep_param.ForceCaliTime && 1 == eep_param.SkyModelContral) {
                    reset_flag = 1;
             }
          }
       }
       else //当前开度无效
       {

            //天空模型增加操作
            eep_param.ActionCount = 0;//校准后动作计数
					  EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ActionCount),
										(uint8 *)&(eep_param.ActionCount), sizeof(eep_param.ActionCount));

            action_cnt = 0; 
            reset_flag = 0;
       }
   }

   next_action_cmd = 0;
   next_action_time = 0;

   if (curtaindegree == INVALID_DEGREE)
   {
       if (percent == 100)
       {
           time = eep_param.motor_up_time; 
           cmd = MOTOR_FORWARD;
       }
       else if (percent == 0)
       {
           time = eep_param.motor_dn_time; 
           cmd = MOTOR_REVERSE;
       }
       else
       {
            time = eep_param.motor_dn_time + (eep_param.motor_dn_time >> 2); 
            cmd = MOTOR_REVERSE;

            next_action_cmd = MOTOR_FORWARD;
            curtaindegree = 0;
            next_action_time = cal_time(percent);
            curtaindegree = INVALID_DEGREE;
       }
   }
   else if (curtaindegree > percent) //当前开度大于控制命令开度，电机反转
   {
      time = cal_time(percent);
      cmd = MOTOR_REVERSE;
   }
   else if (curtaindegree < percent)
   {
      time = cal_time(percent);
      cmd = MOTOR_FORWARD;
   }
   else //当前开度=控制命令开度，无须动作
   {
      stage_restart_data_init(REPORT_START); 
      stage_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //间隔13 + 2*GIDOrder (100us)
      return (NO_ERR);
   }

   if ((Motor1.motorflag) && (Motor1.rotateflag == cmd))
   {
      react = 1;
   }

   if ((reset_flag && (taker_is_gateway)))
   {
      if ((percent == 100) || (percent == 0))
      {
         reset_flag = 0;
      }
      else if (cmd == MOTOR_REVERSE) 
      {
         time = cal_time(0);
         time += eep_param.motor_dn_time >> 2;

         next_action_cmd = MOTOR_FORWARD;
         degree = curtaindegree;
         curtaindegree = 0;
         next_action_time = cal_time(percent);
         curtaindegree = degree;
      }
      else if (cmd == MOTOR_FORWARD)
      {
          #if 0
         time = cal_time(100);
         time += eep_param.motor_up_time >> 2;

         next_action_cmd = MOTOR_REVERSE;
         degree = curtaindegree;
         curtaindegree = 100;
         next_action_time = cal_time(percent);
         curtaindegree = degree;
         #endif
      }
   }

   if ((percent == 100) || (percent == 0))
   {
      time += eep_param.motor_up_time >> 2;
   }

   last_degree = percent;
   stage_restart_data_init(REPORT_START);
   stage_data.wait_cnt = 13 + 2 * stage_data.equipment_gid;  //间隔13 + 2*GIDOrder (100us)
   
   if (react == 1)  //本次转向与上次一致，无须关闭继电器，只需更改运行时间即可
   {
      Motor1.time = time;
      Motor1.run_t = 0;
      Motor1.delay = 0;
      return (NO_ERR);
   }
   else
   {
      ret = motor_ctrl(cmd, time, (struct MOTOR *)&Motor1, MOTOR_DEGREE_CTRL); 
   }

   if (ret) return (DATA_ERR);

   return (NO_ERR);
}

uint8 get_curtain_degree(uint8 *buff, uint8 max_len)
{
   if (max_len < 1) return (0);

   if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))
   {
      buff[1] = DATA_ERR;
      buff[2] = 0x00;
      return (0x82);
   }

   buff[1] = last_degree;

   return (1);
}

#if 0
uint8 get_current_degree(uint8 *buff, uint8 max_len)
{
   if (max_len < 1) return (0);

   if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))
   {
      buff[1] = DATA_ERR;
      buff[2] = 0x00;
      return (0x82);
   }

   if (Motor1.motorflag)
   {
      cal_degree((struct MOTOR *)&Motor1); 
   }

   buff[1] = curtaindegree; 

   return (1);
}
#endif

static uint8 get_delay_time(uint8 *buff, uint8 max_len)
{
    if (max_len < 3) return (0);

    buff[1] = 0x01;
    buff[2] = eep_param.relay_off_delay;// close duan
    buff[3] = eep_param.relay_on_delay; // open tong
		
    return(0x03);
}

static uint8 set_delay_time(uint8 *buff, uint8 w_len)
{
    if ((w_len < 0x03) || (0x01 != buff[0]))
        return(DATA_ERR);

    eep_param.relay_off_delay = buff[1];// close  duan
    eep_param.relay_on_delay = buff[2]; // open tong

    EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, relay_on_delay),
             (uint8 *)&(eep_param.relay_on_delay), sizeof(eep_param.relay_off_delay) + sizeof(eep_param.relay_on_delay));

    return(NO_ERR);
}


uint8 get_lock(uint8 *buff, uint8 max_len)
{
   if (max_len < 1) return (0);

   buff[1] = eep_param.lock;

   return (1);
}


static uint8 set_lock(uint8 *buff, uint8 w_len)
{
    if ((w_len != 0x01) || (buff[0] > LOCK_ENABLE))
        return(DATA_ERR);

    eep_param.lock = buff[0];
    EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, lock),
              (uint8 *)&(eep_param.lock), sizeof(eep_param.lock));
    
    return(NO_ERR);
}

uint8 get_lock_enflag(uint8 *buff, uint8 max_len)
{
   if (max_len < 1) return (0);

   buff[1] = eep_param.enlock;

   return (1);
}


static uint8 set_lock_enflag(uint8 *buff, uint8 w_len)
{
    if ((w_len != 0x01) || (buff[0] > LOCK_ENABLE))
        return(DATA_ERR);

    eep_param.enlock = buff[0];
    EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, enlock),
              (uint8 *)&(eep_param.enlock), sizeof(eep_param.enlock));
    
    return(NO_ERR);
}

static uint8 set_silent_time(uint8 *buff, uint8 w_len)
{
    if(w_len != 0x02) return(DATA_ERR);//len is zero
    dev_search_param.silent_time = get_le_val(&buff[0],TIME_LEN);
    return(NO_ERR);
}
uint8 waite_resetsky_handy(void)
{
    if (0xff == WaiteTime) return 0;
    if (0 == WaiteTime) {
        eep_param.motor_up_time = 0; 
        eep_param.motor_dn_time = 0; 
        eep_param.lock = 0;
        eep_param.enlock = 0;
        eep_param.SkyModelContral = 0;
        eep_param.ForceCaliTime  = 30;
        eep_param.ActionCount = 0;
        eep_param.report_enable = 0x03;
        EEP_Read(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
        WaiteTime = 0xff;
    }
    WaiteTime-- ;
		return 0;
}
static uint8 set_reset_devinf(uint8 *buff, uint8 w_len)
{
		uint32 magic = 0xFFFFFFFF;
    if(w_len != 0x01) return(DATA_ERR);//len is zero
		if(0xAB == buff[0])
		{
        EEP_Write(OF_MAGIC_NUM, (uint8 *)&magic, OF_MAGIC_NUM_LEN);
				return(NO_ERR);
		}
    WaiteTime = buff[0];
		WaiteTime = WaiteTime == 0 ? 1 : WaiteTime;
    return(NO_ERR);
}

static uint8 get_psw(uint8 *buff, uint8 max_len)
{
    if(max_len < 0x01) return(DATA_ERR);
		
    memcpy(&buff[1], eep_param.password,PWD_LEN); 
	return(PWD_LEN);
}

static uint8 set_dev_show(uint8 *buff, uint8 w_len)
{
    if(w_len) return(DATA_ERR);//len is zero

    if (curtain_init != 0)
       return DEV_BUSY;

    if (eep_param.enlock && eep_param.lock)
        return ENLOCK_ERR;
    
    dev_search_param.dev_show_flag = 5;
    return(NO_ERR);
}


//---------------------------------------------------------------------------------------
//-------------------------------天空模型控制部分-----------------------------------------
///天空模型状态读写
static uint8 do_set_SkyModel_cotral(uint8 *buff, uint8 max_len)
{
    if(max_len != 0x01 || buff[0] > 1)
        return DATA_ERR;
    eep_param.SkyModelContral = buff[0];

    EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, SkyModelContral),
              (uint8 *)&(eep_param.SkyModelContral), sizeof(eep_param.SkyModelContral));

    return (NO_ERR);
}
//---------------------------------------------------------------------------------------
static uint8 do_get_SkyModel_cotral(uint8 *buff, uint8 max_len)
{
    buff[1] = eep_param.SkyModelContral;
    return (1);
}
///强制校准次数读写 
static uint8 do_set_ForceCaliTime_cotral(uint8 *buff, uint8 max_len)
{
    if(max_len != 0x01)
        return DATA_ERR;
    eep_param.ForceCaliTime = buff[0];
    EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, ForceCaliTime),
              (uint8 *)&(eep_param.ForceCaliTime), sizeof(eep_param.ForceCaliTime));
    return (NO_ERR);
}
//---------------------------------------------------------------------------------------
static uint8 do_get_ForceCaliTime_cotral(uint8 *buff, uint8 max_len)
{
    buff[1] = eep_param.ForceCaliTime;
    return (1);
}
///校准后动作次数读
static uint8 do_get_ActionCount_cotral(uint8 *buff, uint8 max_len)
{
    buff[1] = eep_param.ActionCount;
    return (1);
}
///天空模型自动关闭参数读取
static uint8 do_get_SkyOffPara_cotral(uint8 *buff, uint8 max_len)
{
    buff[0] = eep_param.SkyBodyUperLin;
    buff[1] = eep_param.SkyTimeUperLin;
    return (2);
}
///天空模型自动关闭参数设定 
static uint8 do_Set_SkyOffPara_cotral(uint8 *buff, uint8 max_len)
{
    if((max_len != 0x02))
        return DATA_ERR;
    if ((buff[1] == 0) || (buff[1] > 5)) {
        return DATA_ERR;
    }
    eep_param.SkyBodyUperLin = buff[0];
    eep_param.SkyTimeUperLin = buff[1];
    EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, SkyBodyUperLin),
              (uint8 *)&(eep_param.SkyBodyUperLin), sizeof(eep_param.SkyBodyUperLin) + sizeof(eep_param.SkyTimeUperLin));
    return (NO_ERR);
}
#if 0
//天空模型人工控制计数
void SkyModel_IdentifyActions_Count(uint16 *DID)
{
    if((1 != eep_param.SkyModelContral) || (1 == taker_is_gateway)) return ;
    if ((scat_flag(0x0A01) != DID) && (scat_flag(0x0A03) != DID) && (scat_flag(0x0A04) != DID))
        return ;//DID不是控制类不进行之后操作
    if ((++eep_param.SkyBodyCtrlCount) > eep_param.SkyBodyUperLin) {
        eep_param.SkyModelContral = 0;
        eep_param.SkyBodyCtrlCount = 0;
        eep_param.SkyTimeCount = 0;
        EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, SkyModelContral),
                  (uint8 *)&(eep_param.SkyModelContral), 
                  sizeof(eep_param.SkyModelContral) + sizeof(eep_param.SkyBodyCtrlCount) + sizeof(eep_param.SkyTimeCount));
    }
    EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, SkyBodyCtrlCount),
             (uint8 *)&(eep_param.SkyBodyCtrlCount), sizeof(eep_param.SkyBodyCtrlCount));
}
//天空模型计数时间更新(执行分辨率1分钟)
void SkyModel_CountTime()
{
    static uint8 SkyTimer = 0;    //天空模型计时
    if (1 == eep_param.SkyModelContral) {
        if (++SkyTimer >= 30) {//计时器计数频率为30min一次
            if((++eep_param.SkyTimeCount) > (SkyTimeUperLin * 48)){ //计数上限以整数天设置
                eep_param.SkyBodyCtrlCount = 0; //清空控制次数计数
                EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, SkyBodyCtrlCount),
                          (uint8 *)&(eep_param.SkyBodyCtrlCount), sizeof(eep_param.SkyBodyCtrlCount));
                eep_param.SkyTimeCount = 0;//清空时间计数
            }
            EEP_Write(OF_PLC_PARAM + offset_of(struct EEP_PARAM, SkyTimeCount),
                      (uint8 *)&(eep_param.SkyTimeCount), sizeof(eep_param.SkyTimeCount));
            SkyTimer = 0;
        }
    }
}
#endif

const struct func_ops func_items[] =
{
   { scat_flag(0x0001), get_device_type, NULL },
   { scat_flag(0x0002), get_soft_ver,    NULL },
   { scat_flag(0x0003), get_dev_infor,   NULL },
   { scat_flag(0x0005), get_dkey,        NULL },
   { scat_flag(0x0007), get_sn,          NULL },
   { scat_flag(0xC030), NULL,            set_password },//NoTest

#if CURTAIN_MOTOR
   { scat_flag(0x0A03), get_curtain_degree,    set_curtain_degree },
   { scat_flag(0x0A04), NULL,       	       set_motor_ctrl },
// { scat_flag(0x0A05), get_current_degree,    NULL },
   { scat_flag(0x0A01), NULL,       	       set_motor_ctrl_inf },
   { scat_flag(0x0A02), get_motor_time,        set_motor_time },
   { scat_flag(0x0A06), get_lock,    set_lock },
   { scat_flag(0x0A30), get_lock_enflag,    set_lock_enflag },
#endif

   { scat_flag(0xd005), get_report_enable_infor, set_report_enable_infor },
   { scat_flag(0x0009), NULL,           	set_dev_show},
   { scat_flag(0x000A), get_psw,        NULL},
   { scat_flag(0x000B), NULL,        set_silent_time},
	 
   { scat_flag(0x0006), get_device_attribute,   NULL },
   { scat_flag(0xC020), get_delay_time,set_delay_time},
   { scat_flag(0xCD00), NULL,        set_reset_devinf},
	 
//天空模型控制                                                                                                    
    {scat_flag(0x0A0A), do_get_SkyModel_cotral       ,do_set_SkyModel_cotral      },    //天空模型状态设置/读取
    {scat_flag(0x0A0B), do_get_ForceCaliTime_cotral  ,do_set_ForceCaliTime_cotral },    //强制校准上限设置设置/读取
    {scat_flag(0x0A0C), do_get_ActionCount_cotral    ,NULL                        },    //校准后动作次数读取
};
#define	METER_ITEM_MAX		ARRAY_SIZE(func_items)
/**************************************
根据数据标志执行相应的功能
**************************************/
#define DATA_LEN(pframe)    (pframe->ctrl&0x7F)
uint8 set_parameter(uint8 data[], uint8 len)
{
   uint8 i, ret;
   struct FBD_Frame *pframe;
   uint8 * pw,*pr;

   pw = data;
   pr = &g_frame_buffer[MAX_BUFFER_SZ - 1 - 1] - len;
   memmove_my(pr, pw, len);

   while ((len >= FBD_FRAME_HEAD) && (pw < pr))
   {
      pframe = (struct FBD_Frame *)pr;

      if (len <  FBD_FRAME_HEAD + DATA_LEN(pframe))
      { //ctrl 长度出错
         mymemcpy(pw, pframe, 2);
         pw += 2;
         *(pw++) = 0x82;
         *(pw++) = LEN_ERR;
         *(pw++) = 0x00;
         break;
      }
      mymemcpy(pw, pr, FBD_FRAME_HEAD + DATA_LEN(pframe));
      pframe = (struct FBD_Frame *)pw;
      pw += 2;
      len -= FBD_FRAME_HEAD + DATA_LEN(pframe);
      pr += FBD_FRAME_HEAD + DATA_LEN(pframe);
//      SkyModel_IdentifyActions_Count(pframe->did);
      for (i = 0; i < METER_ITEM_MAX; i++)
      {
         if (memcmp_my(pframe->did, func_items[i].di, 2) == 0) break;
      }
      if ((i >= METER_ITEM_MAX) || (NULL == func_items[i].write))
      {
         *(pw++) = 0x82;
         *(pw++) = DID_ERR;
         *(pw++) = 0x00;
      }
      else
      { //调用函数，函数指针(各种控制命令等)
         ret = func_items[i].write(pframe->data, DATA_LEN(pframe));
         if (CHG_DID & ret)
         { //修改数据did
            ret &= 0x7f;
            mymemcpy(pframe, pframe->data, ret);
            ret -= 2;
            pw += ret;
         }
         else if (NO_ERR != ret)
         {
            *(pw++) = 0x82;
            *(pw++) = ret;
            *(pw++) = 0x00;
         }
         else
         {
            ret = DATA_LEN(pframe) + 1; //设置数据长度
            pw += ret;
         }
      }
   }
   return (pw - data);
}

#define GROUP_LEN       0x3F
uint8 is_gid_equal(uint8 data[])
{
   uint16 s_gid, t_gid;
   uint8 k, connt_size = 0;
   uint8 i, len_t, type_t;
   uint16 j = 0;
   s_gid = eep_param.sid[1]; //网关分配的SID,2B
   s_gid <<= 8;
   s_gid += eep_param.sid[0];
   len_t = data[0] & GROUP_LEN; //组地址字节数
   type_t = data[0] >> 6; //组地址类型
   if (0x00 == type_t) //组地址用一个位表示
   {
      s_gid--;
      k = s_gid + 1;
      if (k % 8)
      {
         k = k % 8;
      }
      else
      {
         k = 8;
      }
      if (len_t < (s_gid >> 3) + 1)
      {
         goto deal_find_none;
      }
      if (data[(s_gid >> 3) + 1] & (0x01 << (s_gid & 0x07)))
      {
         for (j = 0; j < (s_gid >> 3); j++)
         {
            connt_size += get_1byte_bit1_number(data[j + 1], 8);
         }
         connt_size += get_1byte_bit1_number(data[(s_gid >> 3) + 1], k);
         if (0 == stage_data.find_myself_flag)
         {
            stage_data.equipment_gid += connt_size;
         }
         stage_data.find_myself_flag = 1;
         return (1);
      }
   }
   else //组地址用字节表示
   {
      for (i = 1; i <= len_t; i++)
      {
         t_gid = data[i];
         if (0x02 == type_t)
         {
            if (len_t & 1) return (0);
            t_gid += data[i + 1] << 8;
            i++;
         }
         if (t_gid == s_gid)
         {
            if (0x02 == type_t)
            {
               connt_size += i / 2;
            }
            else
            {
               connt_size += i;
            }
            if (0 == stage_data.find_myself_flag)
            {
               stage_data.equipment_gid += connt_size;
            }
            stage_data.find_myself_flag = 1;
            return (1);
         }
         if (0x00 == t_gid) //全广播
         {
            stage_data.equipment_gid += s_gid;
            stage_data.find_myself_flag = 1;
            return (1);
         }
      }
   }
deal_find_none:
   if (0 == stage_data.find_myself_flag)
   {
      if (0x00 == type_t)
      {
         for (j = 0; j < len_t; j++)
         {
            stage_data.equipment_gid += get_1byte_bit1_number(data[j + 1], 8);
         }
      }
      else if (0x01 == type_t)
      {
         stage_data.equipment_gid += len_t;
      }
      else if (0x02 == type_t)
      {
         stage_data.equipment_gid += len_t / 2;
      }
   }
   return (0);
}

uint8 set_group_parameter(uint8 data[], uint8 len)
{
   uint8 i, j, gid_len, fbd_len;
   struct FBD_Frame *pframe;

   j = 0;
   gid_len = (data[j] & GROUP_LEN) + 1;
   while (len >= (FBD_FRAME_HEAD + gid_len))
   {
      pframe = (struct FBD_Frame *)&data[j + gid_len];
      fbd_len = DATA_LEN(pframe) + FBD_FRAME_HEAD + gid_len;
      if (len < fbd_len) break;

      if (is_gid_equal(&data[j]))
      {
         for (i = 0; i < METER_ITEM_MAX; i++)
         {
            if (memcmp_my(pframe->did, func_items[i].di, 2) == 0) break;
         }
         if ((i < METER_ITEM_MAX) && (NULL != func_items[i].write))
         { //调用函数，函数指针(各种控制命令等)
            func_items[i].write(pframe->data, DATA_LEN(pframe));
         }
      }

      j += fbd_len;
      len -= fbd_len;

      gid_len = (data[j] & GROUP_LEN) + 1;
   }
   return (0);
}
/**************************************
根据数据标志执行相应的功能
**************************************/
uint8 read_parameter(uint8 data[], uint8 len)
{
   uint8 i, ret;
   struct FBD_Frame *pframe;
   uint8 * pw,*pr;

   pw = data;
   pr = &g_frame_buffer[MAX_BUFFER_SZ - 1 - 1] - len; //最大可读数据长度,当数据返回长度刚刚好，cs无法发送出去问题？！
   memmove_my(pr, &data[0], len);

   while (len >= FBD_FRAME_HEAD)
   {
      pframe = (struct FBD_Frame *)pr;

      if (len <  FBD_FRAME_HEAD + DATA_LEN(pframe))
      { //ctrl长度出错
         mymemcpy(pw, pframe, 2);
         pw += 2;
         *(pw++) = 0x82;
         *(pw++) = LEN_ERR;
         *(pw++) = 0x00;
         break;
      }
      mymemcpy(pw, pr, FBD_FRAME_HEAD + DATA_LEN(pframe));
      pframe = (struct FBD_Frame *)pw;
      pw += 2;
      len -= FBD_FRAME_HEAD + DATA_LEN(pframe);
      pr += FBD_FRAME_HEAD + DATA_LEN(pframe);

      for (i = 0; i < METER_ITEM_MAX; i++)
      {
         if (memcmp_my(pframe->did, func_items[i].di, 2) == 0) break;
      }
      if ((i >= METER_ITEM_MAX) || (func_items[i].read == NULL))
      {
         *(pw++) = 0x82;
         *(pw++) = DID_ERR;
         *(pw++) = 0x00;
      }
      else
      {
         ret = func_items[i].read(pw, (uint8)(pr - (pw + 1)));
         if (0x00 == ret) //数据返回0，可能导致读取数据内容没有数据标识，数据长度为1的报文!?
         {
            pw -= 2;
            continue;
         }
         *(pw++) = ret;
         pw += (ret & 0x7F); //最高位为错误标志位，返回的数据如果是带有错误标志位？！
      }
   }
   return (pw - data);
}








