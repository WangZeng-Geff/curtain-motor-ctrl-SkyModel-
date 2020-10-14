#include "headfiles.h"
#include "protocol.h"

uint16 task_monitor = 0;

DATA_SEG uint8 g_frame_buffer[MAX_BUFFER_SZ];//
DATA_SEG struct EEP_PARAM   eep_param;
DATA_SEG struct REG reg;
DATA_SEG struct PLC_MACHINE  plc_state;
uint8 taker_is_gateway;
uint8 cmd_is_boardcast;

DATA_SEG static uint8 rst_plc_t = 0;
#define clear_rst_time(pframe)        do{if(CMD_ACK_EID == pframe->infor[0]) rst_plc_t = 0;}while(0)

#define MAX_OVERTIME    (plc_state.trycnt+5)
#define MAX_TRYCNT      2
#define MAX_RST_TIME    3

void chg_state(uint8 cur_state);
#define next_state()         do{chg_state(plc_state.pstate->next_state);}while(0)
#define try_state_again()    do{chg_state(plc_state.pstate->cur_state); plc_state.trycnt++;}while(0)

static uint8 local_ack_opt(struct SHS_frame *pframe)
{
    if(pframe != NULL)
    {
        if((0x00 == is_all_xx(pframe->said, 0x00, ID_LEN))
           || (0x00 == is_all_xx(pframe->taid, 0x00, ID_LEN)))
        {
            return(0);
        }
        if((CMD_ACK == pframe->infor[0]) || (CMD_NAK == pframe->infor[0]))
        {
            next_state();
            return(1);
        }
    }
    if(plc_state.wait_t > MAX_OVERTIME)
    {
        try_state_again();
    }
    return(0);
}

#define     MIN_PLC_RUN        2
static uint8 reset_plc(uint8 init, void *args)
{
    if(init)
    {
        SET_PIN_L(PLC_RESET_PORT, PLC_RESET_PIN);
        return(0);
    }
    if(plc_state.wait_t >= 1)
    {
        SET_PIN_H(PLC_RESET_PORT, PLC_RESET_PIN);
    }
    if(plc_state.wait_t >= MIN_PLC_RUN)
    {
        next_state();
    }
    return(0);
}
void init_frame_head(struct SHS_frame * pframe)
{
    pframe->stc = STC;
    memset_my(pframe->said, 0x00, ADDRESS_LEN);
    memset_my(pframe->taid, 0x00, ADDRESS_LEN);
    pframe->seq = 0;
}

static void send_local_frame(uint8 buffer[], uint8 len)
{
    struct SHS_frame *pframe = (struct SHS_frame *)buffer;

    memmove_my(&buffer[offset_of(struct SHS_frame, infor)], buffer, len);
    init_frame_head(pframe);
    pframe->length = len;
    pframe->infor[len] = checksum((uint8 *)pframe, SHS_FRAME_HEAD+len);

    uart_write(buffer, SHS_FRAME_HEAD+len+1);
}

/************************************************* 
  �����ز�оƬע������(1:����ע�᣻0������ע��)
*************************************************/
static void set_REG(uint8 buffer[])
{
    uint8 len=0;

    buffer[len++] = CMD_SET_REG;
    buffer[len++] = reg.type | reg.last_status;
    reg.last_status = 0;
    send_local_frame(buffer, len);
}

/************************************************* 
            ֪ͨ�ز�оƬ�Ͽ���·
*************************************************/
static void set_UNLINK(uint8 buffer[])
{
    buffer[0] = CMD_UNLINK;

    send_local_frame(buffer, 1);
}
/*************************************************
                �����ز�PANID
*************************************************/
static uint8 set_PANID(uint8 buffer[])
{
    uint8 len = 0;

    buffer[len++] = CMD_SET_PANID;
    //panid��־��Чͬʱpanidû�б����
    if(VALID_DATA != eep_param.panid_flag) return(0);
    mymemcpy(&buffer[len], eep_param.panid, PANID_LEN);
    len += PANID_LEN;

    send_local_frame(buffer, len);
    return(1);
}

static uint8 wr_plc_panid(uint8 init, void *args)
{
    struct SHS_frame *pframe = (struct SHS_frame *)args;
    uint8 ret;

    if(init) 
    {
        ret = set_PANID(g_frame_buffer);
        if(0x00 == ret) next_state();
    }
    else
    {
        local_ack_opt(pframe);
    }
    return(0);
}
/*************************************************
                �����ز�AID
*************************************************/
static void set_AID(uint8 buffer[])
{
    uint8 len = 0;

    buffer[len++] = CMD_SET_AID;
    mymemcpy(&buffer[len], eep_param.id, ID_LEN);
    len += ID_LEN;

    send_local_frame(buffer, len);
}

static uint8 wr_plc_aid(uint8 init, void *args)
{
    struct SHS_frame *pframe = (struct SHS_frame *)args;

    if(init)
    {
        set_AID(g_frame_buffer);
    }
    else
    {
        local_ack_opt(pframe);
    }
    return(0);
}

/*************************************************
�ϵ����ز�ģ��ͨ��һ�Σ��õ��ز�ģ���ַ�����ص�ַ 
type = MACID / GWID 
*************************************************/
void get_ID(uint8 type, uint8 buffer[])
{
    buffer[0] = type;

    send_local_frame(buffer, 1);
}
static uint8 rd_gw_aid(uint8 init, void *args)
{
    struct SHS_frame *pframe = (struct SHS_frame *)args;

    if(init)
    {
        get_ID(CMD_GET_GWAID,g_frame_buffer);
    }
    else
    {
        if((pframe != NULL) && (CMD_GET_GWAID == pframe->infor[0]))
        {
            if(0x00 != memcmp_my(eep_param.gateway_id, &pframe->infor[1], ID_LEN))
            {
                mymemcpy(eep_param.gateway_id, &pframe->infor[1], ID_LEN);
                EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
            }
            next_state();
            return(0);
        }
        if(plc_state.wait_t > MAX_OVERTIME)
        {
            try_state_again();
        }
    }
    return(0);
}

static uint8 rd_plc_sid(uint8 init, void *args)
{
    struct SHS_frame *pframe = (struct SHS_frame *)args;

    if(init)
    {
        get_ID(CMD_GET_SID,g_frame_buffer);
    }
    else
    {
        if((pframe != NULL) && (CMD_ACK_SID == pframe->infor[0]))
        {
            if(0x00 != memcmp_my(eep_param.sid, &pframe->infor[1], SID_LEN))
            {
                mymemcpy(eep_param.sid, &pframe->infor[1], SID_LEN);
                EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
            }
            next_state();
            return(0);
        }
        if(plc_state.wait_t > MAX_OVERTIME)
        {
            try_state_again();
        }
    }
    return(0);
}


/****************************************************************
*                       ѡ��Զ��������ʽ
****************************************************************/
static uint8 rd_update_way(uint8 init, void *args)
{
    uint8 buffer[14]={0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x22, 0x01,0x00};
    
    if (init)
    {
        buffer[13] = 0xA3;
        uart_write(buffer, SHS_FRAME_HEAD + 0x02 + 1);
    }
    else
    {
        struct SHS_frame *pframe = (struct SHS_frame *)args;
        if ((pframe != NULL))
        {
            if (pframe->infor[0] ==  0x00)
            {
                next_state();
                return (0);
            }
        }
        if (plc_state.wait_t > MAX_OVERTIME)
        try_state_again();
    }
    return (0);
}

/*************************************************
                ��ȡ�ز�оƬEID
*************************************************/
uint8 check_sole_encode(struct EEP_PARAM *param)
{
    struct ENCODE_PARAM encode_param;

    EEP_Read((ENCODE_MAGIC_ADDR - OF_EEPROM_ADDRESS),(uint8 *)&encode_param, sizeof(struct ENCODE_PARAM));
    if((0x00 == memcmp_my(param->id, encode_param.dev.id, ID_LEN))
       && (0x00 == memcmp_my(param->password, encode_param.dev.password, PWD_LEN))
       && (VALID_DATA == param->pwd_magic)) 
    {
        return(1);
    }

    if(ENCODE_MAGIC == encode_param.sole_magic) 
    {
        mymemcpy(param->id, encode_param.dev.id, ID_LEN);
        mymemcpy(param->password, encode_param.dev.password, PWD_LEN);
        param->pwd_magic = VALID_DATA;
        EEP_Write(OF_PLC_PARAM, (uint8 *)param, sizeof(struct EEP_PARAM));
        return(1);
    }
    return(0);
}

static uint8 rd_plc_eid(uint8 init, void *args)
{
    struct SHS_frame *pframe = (struct SHS_frame *)args;

    if(init)         
    {
	if(check_sole_encode(&eep_param)) 
	{
		next_state();
		return(0);
	}
        get_ID(CMD_GET_EID, g_frame_buffer);
    }
    else
    {
        if((pframe != NULL) && (CMD_ACK_EID == pframe->infor[0]))
        {
            if(0x00 != memcmp_my(eep_param.id, &pframe->infor[1], ID_LEN))
            {
                mymemcpy(eep_param.id, &pframe->infor[1], ID_LEN);
                EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
            }
//            if(bps != eep_param.bps) eep_param.bps=0;//�������ز�ģ��,ԭ����9600bps,������!!
            next_state();
            return(0);
        }
        if(plc_state.wait_t > MAX_OVERTIME)
        {
            try_state_again();
        }
    }

    return(0);
}
static uint8 wait_sec(uint8 init, void *args)
{
    if(plc_state.wait_t > 1)
    { 
       next_state();
    }
    return(0);
}
/************************************************ 
                    ����ע��
 ************************************************/
static uint8 set_unlink(uint8 init, void *args)
{
    struct SHS_frame *pframe = (struct SHS_frame *)args;

    if(init) 
    {
        set_UNLINK(g_frame_buffer);
    }
    else
    {
        local_ack_opt(pframe);
    }
    return(0);
}
/**********************************************
�豸ע�᣺1����һ��ע�᲻ʹ�����룻 
          2���ڶ����Ժ�ע�ᣬ��ԭ��¼�����غ���ͬ��ʹ������
          3���ڶ����Ժ�ע�ᣬ��������ȡ�豸��Ҫʹ���豸����
***********************************************/
static uint8 get_reg(struct SHS_frame *pframe)
{
    uint8 need_wr=1;

    if(PASSWORD_REG == reg.type)
    {
        if(VALID_DATA != eep_param.panid_flag)
        {//��һ��ע�ᣬ���ж�����
        }
        else if((VALID_DATA == eep_param.panid_flag)
           && (0x00 == memcmp_my(eep_param.panid, &pframe->infor[1], PANID_LEN)))
        {//�ɹ�ע����ɺ��ٴ�����ע�ᣬֻ�ж�panid
            need_wr = 0;
        }
        else if((eep_param.pwd_magic == VALID_DATA)
                && (0x00 != memcmp_my(eep_param.password, &pframe->infor[PANID_LEN+1], PWD_LEN)))
        {//�ж��Ƿ����Ѿ�ע��ɹ���panid
            reg.last_status = PASSWORD_ERR;
            chg_state(UNLINK2);//20141018,���ز��Ͽ����ӻ��������,�޸��ز�����!!
            return(0);
        }
    }
    //�ɹ�����������ע��ɹ�

    reg.type = PASSWORD_REG;
    chg_state(G_GWID);//��ȡ����aid

    if(need_wr)
    {
        mymemcpy(eep_param.panid, &pframe->infor[1], PANID_LEN);
        eep_param.panid_flag = VALID_DATA;
        EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
    }
    return(0);
}

static uint8 set_register(uint8 init, void *args)
{
    struct SHS_frame *pframe = (struct SHS_frame *)args;

    if(init)
    {
        set_REG(g_frame_buffer);
    }
    else
    {
        local_ack_opt(pframe);
    }
    return(0);
}

/****************************************
            �ز���ʼ��״̬ת��
****************************************/

void plc_machine_opt(void *args)
{
    struct PLC_STATE *pstate = plc_state.pstate;
    uint8 init;

    init = plc_state.init;
    if(init) plc_state.wait_t=0;
    plc_state.init = 0;
    if(NULL != pstate)
    {
        pstate->action(init, args);
    }
}


/**************************************************************** 
                        ��֯��Ӧ����(��ӦPLCоƬ)
****************************************************************/
uint8 set_ret_frame(struct SHS_frame *pframe, uint8 len)
{
    mymemcpy(pframe->taid, pframe->said, ID_LEN);  //tid and sid exchanged
    mymemcpy(pframe->said, eep_param.id, ID_LEN);

    pframe->seq |= 0x80;                         //�ظ����ı�־
    pframe->length = len;
	pframe->infor[len] = checksum((uint8 *)pframe, len +SHS_FRAME_HEAD);
    len++;

    /*the total bytes to be sent*/
    return(len + SHS_FRAME_HEAD);
}

/**************************************************************** 
                        ����ͨ��Э�����
****************************************************************/
void local_frame_opt(struct SHS_frame *pframe)
{
    if(CMD_REGINFOR == pframe->infor[0])
    {
        get_reg(pframe);
    }
    else if(CMD_REQ_AID == pframe->infor[0])
    {
        chg_state(S_AID);//�ز�����������ȡaid��˵���ز�оƬ���ܸ�λ����
    }
    else if(CMD_UNLINK == pframe->infor[0])
    {//���ضϿ����ӣ��ز�оƬ�ϱ��Ͽ����ӣ������ز�ע��panid���ã�������
        chg_state(S_PWDREG);
    }
    else if(CMD_UPDATE_START == pframe->infor[0])
    {
        bootloader_start_update(&pframe->infor[1],pframe->length-1);
    }
//    else if(CMD_GET_GWAID == pframe->infor[0])
//    {
//        mymemcpy(eep_param.gateway_id, &pframe->infor[1], ID_LEN);
//        EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
//    }
}
/**************************************************************** 
                        ��أ�Զ�̣�ͨ��Э�����
****************************************************************/
uint8 remote_frame_opt(struct SHS_frame *pframe)
{
    uint8 ret = 0;
		
   if(pframe->seq & 0x80)//FSEQ bit7=1�ظ�����
   {
       if((RELIABLE_REPORT == (pframe->infor[0]))&&(0x00 == memcmp_my(eep_param.gateway_id,&pframe->said[0],ID_LEN)))
       {
           stage_data.state_change_mode = GATEWAY_ACK;
           enlock_data.state_change_mode = GATEWAY_ACK;
           power_on_report.report_ack = GATEWAY_ACK;
           check_power_on_report_ack();
           check_gw_ack();
           check_gw_enlock_ack();
       } 
       return(0);
   }
   if(pframe->length < 1)return(0);
   /*set command*/
   if(CMD_SET == (pframe->infor[0] & 0x07))
    {
       cmd_is_boardcast = 0;
       curtain_ctrl_cmd = 0;
       stage_data.equipment_gid = 0;
       stage_data.find_myself_flag = 0;
       if (memcmp_my(pframe->said, eep_param.gateway_id, ID_LEN))
       {
           taker_is_gateway = 0;
       }
       else
       {
           taker_is_gateway = 1;
       }
       
       if(is_all_xx(pframe->taid, 0xff, ID_LEN))
       {
           cmd_is_boardcast = 1;
           ret = set_group_parameter(&pframe->infor[1], pframe->length-1);
           if(0 == stage_data.find_myself_flag)
           {
              return 0;
           }
           stage_data.find_myself_flag = 0;
           mymemcpy(stage_data.taker_id,&pframe->said[0],ID_LEN);
           mymemcpy(enlock_data.taker_id,&pframe->said[0],ID_LEN);
#if 0
           if (curtain_ctrl_cmd) 
           {
               enlock_restart_data_init(REPORT_START); 
               enlock_data.wait_cnt = 2 * stage_data.equipment_gid;  //���2S
           }
           else
           {
               if ((eep_param.report_enable != REPORT_NO))
                {
                    if (stage_data.equipment_gid)
                    {
                        stage_data.state_change_mode = get_message_source(&pframe->said[0],0xff);
                    }
                }
           }
#endif
          stage_data.state_change_mode = get_message_source(&pframe->said[0],0xff);
       }
       else
       {
           ret = set_parameter(&pframe->infor[1], pframe->length-1)+1;
           mymemcpy(stage_data.taker_id,&pframe->said[0],ID_LEN);
           mymemcpy(enlock_data.taker_id,&pframe->said[0],ID_LEN);
           
#if 0
           if ((curtain_ctrl_cmd == 0) && (eep_param.report_enable != REPORT_NO))
            {
                stage_data.state_change_mode = get_message_source(&pframe->said[0],0x00);
            }
#endif
					stage_data.state_change_mode = get_message_source(&pframe->said[0],0x00);

       }
   }
    /*read command*/
    else if(CMD_READ == (pframe->infor[0] & 0x07))
    {
        ret = read_parameter(&pframe->infor[1], pframe->length-1)+1;
    }

    else if(CMD_UPDATE == (pframe->infor[0] & 0x07))
    {
        ret = update_frame_opt(&pframe->infor[1], pframe->length-1)+1;
    }
    else if(CMD_SHOW == (pframe->infor[0] & 0x07))
    {
        ret = search_frame_opt(pframe); //08 00 07 00 06 00 XX XX XX XX
    }
    return(ret);
}

uint8 frame_handle(uint8 init, void *args)
{
    uint8 len,ret;
    struct SHS_frame *pframe = (struct SHS_frame *)args;

    if(pframe == NULL) return(0);

    if(is_all_xx(pframe->said, 0x00, ID_LEN))
    {//����ͨ��
        local_frame_opt(pframe);
        return(1);
    }
    else
    {//���ͨ��
        ret = remote_frame_opt(pframe);
        if (ret > 1) 
        {
            len = set_ret_frame(pframe, ret);
            uart_write((uint8 *)pframe, len);
        }
    }
    
    return(0); 
}

const struct PLC_STATE plc_state_slot[]=
{
    //init plc
    {RST_PLC,   R_EID,   reset_plc},
    {R_EID,     S_AID,   rd_plc_eid},
    {S_AID,     R_UPDATE,wr_plc_aid},
    {R_UPDATE,  UNLINK2, rd_update_way },
    {UNLINK2,   WAIT,     set_unlink},
    {WAIT,      S_PWDREG, wait_sec},
    {S_PWDREG,  S_PANID, set_register},
    {S_PANID,   _END,    wr_plc_panid},
    //key reg
#if KEY_REG
    {UNLINK1,   S_REG,   set_unlink},
#endif        
    //����ע�ᣬ�������������豸���벻ͬ���Ͽ��������Ӻ������ز�panid�������ز�ע��
    {UNLINK2,   PWD_ERR, set_unlink},
    {PWD_ERR,   S_PANID, set_register},

    //ע��ɹ�
    {G_GWID,    S_REG,   rd_gw_aid},
    {S_REG,     G_SID,   set_register},//����ע�����ԣ��ȴ��ز�оƬ�ϱ�panid
    {G_SID,     _END,    rd_plc_sid},

    {_END,      _END,    frame_handle},
};
#define   PLC_SLOT_SZ         (sizeof(plc_state_slot)/sizeof(plc_state_slot[0]))
#if DEBUG
//this is just for dubug purpose, no uart output
#define MAX_LOG_STATES     20
DATA_SEG static uint8 state_logs[MAX_LOG_STATES*2];
DATA_SEG static uint8 state_idx;
#endif
void chg_state(uint8 cur_state)
{
    uint8 i;
    static uint8 last_state=INVALID;
    
    plc_state.wait_t = 0;
    plc_state.init=1;
    for(i = 0; i < PLC_SLOT_SZ; i++)
    {
        if(plc_state_slot[i].cur_state == cur_state)
        {
            plc_state.pstate = (struct PLC_STATE *)&plc_state_slot[i];
            break;
        }
    }
    if(i >= PLC_SLOT_SZ)
    {
        plc_state.pstate = (struct PLC_STATE *)&plc_state_slot[0];
    }

    if(last_state != plc_state.pstate->cur_state)
    {
    #if DEBUG
        state_idx += 2;
        if(state_idx >= sizeof(state_logs)) state_idx=0;
    
        state_logs[state_idx] = plc_state.pstate->cur_state;
    #endif
        plc_state.trycnt = 0;
    }
    #if DEBUG
      state_logs[state_idx+1]++;
    #endif
    last_state = plc_state.pstate->cur_state;
    notify(EV_STATE);
}
/**************************************************** 
                    ͨ��Э�����
 ***************************************************/
void scan_uart_opt(void *args)
{
	struct SHS_frame *pframe;
	uint8  idx,len;
    
    idx = uart_peek_data(g_frame_buffer, sizeof(g_frame_buffer));

    pframe = get_smart_frame(g_frame_buffer,idx);

    if(NULL == pframe) return;

    idx = (uint8 *)pframe-g_frame_buffer;
    len = pframe->length+SHS_FRAME_HEAD+1;
    clear_uart(idx + len);
    memmove_my(&g_frame_buffer[0], &g_frame_buffer[idx], len);
    pframe = (struct SHS_frame *)g_frame_buffer;
    //����2Сʱ��ͨѶ��λ�ز�оƬ����
    clear_rst_time(pframe);
    plc_machine_opt(pframe);		    //protocol handle (buffer[i] is a complete frame data)
}

/****************************************************************** 
                    ��ʼ��ȫ�ֱ�����eeprom����
 *****************************************************************/

void system_init(void)
{
    uint32 magic;

    //init eeprom param
    EEP_Read(OF_MAGIC_NUM, (uint8 *)&magic, OF_MAGIC_NUM_LEN);
    if(MAGIC_NUM != magic)
    {
        memset_my((uint8 *)&eep_param.gateway_id, 0x00, (sizeof(eep_param)-8));
        eep_param.pwd_magic = INVALID_DATA;
        eep_param.report_enable = 0x03;
        eep_param.relay_off_delay = 0x0E;
        eep_param.relay_on_delay = 0x24;
        eep_param.degree = 0xFF;
        eep_param.acting = 1;
        eep_param.lock = 0;
        eep_param.enlock = 0;
//���ģ�Ͳ�����ʼ��
        eep_param.SkyModelContral = 0;
        eep_param.SkyBodyMagicKey = 0x0720;
        eep_param.SkyBodyUperLin = 5;  
        eep_param.SkyTimeUperLin = 1;  
        eep_param.ForceCaliTime  = 30;
        eep_param.ActionCount = 0;
        EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
        magic = MAGIC_NUM;
        EEP_Write(OF_MAGIC_NUM, (uint8 *)&magic, OF_MAGIC_NUM_LEN);
    }
    EEP_Read(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
    if (0x0720 != eep_param.SkyBodyMagicKey) {
        eep_param.SkyBodyMagicKey = 0x0720;
        eep_param.SkyModelContral = (eep_param.SkyModelContral > 1) ? 0 : eep_param.SkyModelContral;
        eep_param.ForceCaliTime  = 30;
        eep_param.ActionCount = 0;
        EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
    }

    /*��ʼ��״̬������*/
    memset_my(&plc_state, 0x00, sizeof(plc_state));
    memset_my(&reg, 0x00, sizeof(reg));
    reg.type = PASSWORD_REG;//�ϵ�����ע��
    chg_state(RST_PLC);

    stage_data_init(); 
    enlock_data_init();

    #if 0 //�����ϵ�У׼
    #ifdef CURTAIN_LOCK
    if (LOCK_DISABLE != eep_param.lock)
        lock_init = POWERON_INIT;
    else
        curtain_init = POWERON_INIT;
    #else
    curtain_init = POWERON_INIT;
    #endif
    #endif

    poweron_report_init();

    curtain_time_init();

    curtaindegree = eep_param.degree;
    last_degree = curtaindegree;    //Ŀ�꿪���뵱ǰ���ȱ���һ�£�����ƽ̨���ܻ���ƴ�������

    if ((eep_param.acting) || ((eep_param.degree > 100)))
    {
        curtaindegree = 0xFF;
        last_degree = curtaindegree;
//        eep_param.acting = 0; //�˴������壬����ϵ��һֱδ���п��ȿ��ƣ�ʱ�����Ҳδ�ﵽԤ��ʱ��Ҫ�󣬿�����Ȼδ֪
//        EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
    }

    #if 0
    if ((eep_param.motor_up_time == 0) || (eep_param.motor_dn_time == 0))
    {
        #ifdef CURTAIN_LOCK
        if (LOCK_DISABLE == eep_param.lock)
            motor_ctrl(MOTOR_FORWARD, MAX_RUN_TIME, (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL); 
        #else
        motor_ctrl(MOTOR_FORWARD, MAX_RUN_TIME, (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL); 
        #endif
        return;
    }

    #ifdef CURTAIN_LOCK
    if (LOCK_DISABLE == eep_param.lock)
        motor_ctrl(MOTOR_FORWARD, MAX_RUN_TIME, (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL);
    #else
    motor_ctrl(MOTOR_FORWARD, MAX_RUN_TIME, (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL);
    #endif

    #endif

//    eep_param.sid[0] = 1;
//    eep_param.gateway_id[0] = 1;
}

#if 0
/********************************************************* 
                         �¼��ϱ�
 ********************************************************/
void org_report_frame(uint8 type)
{

    struct SHS_frame *pshs_frame = (struct SHS_frame *)g_frame_buffer;
    struct FBD_Frame *pfbd_frame;
    uint8 len;

    pshs_frame->stc = STC;
    pshs_frame->seq = 1;
    mymemcpy(pshs_frame->said, eep_param.id, ID_LEN);
    mymemcpy(pshs_frame->taid, eep_param.gateway_id, ID_LEN);
    pfbd_frame = (struct FBD_Frame *)pshs_frame->infor;
    pfbd_frame->ctrl = 0x00;
    if(type & 0x01)
    {
        pfbd_frame->did[0] = 0x12;
        pfbd_frame->did[1] = 0xc0;
        _get_relay_status(pfbd_frame->data, 1);
        len = 1;
        pfbd_frame->ctrl += len;
    }
    else if(type & 0x02)
    {
        pfbd_frame->did[0] = 0x13;
        pfbd_frame->did[1] = 0xc0;
        len = 1;
        pfbd_frame->ctrl += len;
    }
    else
    {
        event.report = 0;
        return;
    }
    event.type &= ~type;
    pfbd_frame->data[len] = checksum((uint8 *)pshs_frame, len+SHS_FRAME_HEAD+FBD_FRAME_HEAD);
    len += SHS_FRAME_HEAD+FBD_FRAME_HEAD;
    len++;
    uart_write(g_frame_buffer, len);

}
#endif
#if 0
void event_report(void *args)
{
    if (0x00 == (event.report)) return;

    org_report_frame(event.type);
}
#endif
//��������
//2Сʱ�����κ��ز�ͨ�ţ���λ�ز�оƬ������
static void chk_plc_alive(void)
{
    if(rst_plc_t > 120)//2Сʱû��ͨ�ţ���λ�ز�оƬ
    {
        rst_plc_t = 0;
        chg_state(RST_PLC);
    }
    else if((60 == rst_plc_t) || (120 == rst_plc_t))
    {//��ȡ�ز�EID
        get_ID(CMD_GET_EID, g_frame_buffer);
    }
}

#if 0
uint32 minutes = 0;
uint8 updown = 3;
#endif

void task_min(void *args)
{
    rst_plc_t++;
//    minutes++;
    chk_plc_alive();
//    SkyModel_CountTime();//���ģ�Ϳ��ƶ˼�ʱ����

#if 0
    if (PRESSKEY_REG == reg.type)
    {
        if (++reg.wait_t > 5)
        {//����ע�ᳬʱ
            reg.type = PASSWORD_REG;//�ָ�Ĭ������
            chg_state(S_REG);
        }
    }
#endif 
    
   #if 0
   if (minutes % 10 == 0)
   {
      if (updown == 2)
      {
         updown = 3;
         motor_ctrl((uint8)3, eep_param.motor_dn_time + (eep_param.motor_dn_time >> 3), (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL);
      }
      else if (updown == 3)
      {
         updown = 2;
         motor_ctrl((uint8)2, eep_param.motor_up_time + (eep_param.motor_up_time >> 3), (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL);
      }
   }
   #endif
   

}

//������
void task_sec(void *args)
{
    #if 0
    static uint32 a = 0;
    a++;
    if ((a % 10) == 0)
    {
        relay_delay = 4;	//��ʱ����40mS
        relay_delay_flag = _OFF;
        motor_ctrl((uint8)3, MAX_RUN_TIME, (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL);
    }
    else if ((a % 10) == 5)
    {
        relay_delay = 4;	//��ʱ����40mS
        relay_delay_flag = _ON;
        motor_ctrl((uint8)3, MAX_RUN_TIME, (struct MOTOR *)&Motor1, MOTOR_INIT_CTRL);
    }
    #endif

    plc_state.wait_t++;
//    check_state_change();   //����ֵ����ϱ�
//    check_enlock_change();  //���ű����ϱ�
    check_power_on();

    check_dev_show();
    check_search_delay();
    relay_check();
}

void task_10ms(void *args)
{   
    curtain_state_fun();
    relay_delay_ms();
}

//100msΪ��λ�İٺ�������
void task_100ms(void *args)
{
    notify(EV_STATE);
    uart_tick_hook();
#if KEY_REG
    chk_key_pressed();
#endif

    if ((next_action_cmd != 0) && (Motor1.motorflag == 0))
    {
       motor_ctrl(next_action_cmd, next_action_time, (struct MOTOR *)&Motor1, MOTOR_DEGREE_CTRL);
       next_action_cmd = 0;
       next_action_time = 0;
    }
		waite_resetsky_handy();
    check_state_change();   //����ֵ����ϱ�
    check_enlock_change();  //���ű����ϱ�

    check_gw_ack();
    check_gw_enlock_ack();
    check_power_on_report_ack();
}

void clr_watchdog(void *args)
{
    IWDG->KR = IWDG_KEY_REFRESH; 
}
#if KEY_REG
//����ע����ʼ״̬
void pressed_key(void *args)
{
    if (PRESSKEY_REG != reg.type)
    {
        reg.type = PRESSKEY_REG;
        reg.wait_t=0;
        chg_state(UNLINK1);
        if (INVALID_DATA == eep_param.panid_flag) return;
        eep_param.panid_flag = INVALID_DATA+0x80;
        EEP_Write(OF_PLC_PARAM, (uint8 *)&eep_param, sizeof(eep_param));
    }
}
#endif

void state_machine(void *args)
{
    plc_machine_opt(NULL);
}

const struct task tasks[] =
{
    {EV_RXCHAR, 0,            NULL,  scan_uart_opt},
    {EV_CLRDOG, ALWAYS_ALIVE, NULL,  clr_watchdog},
//    {EV_REPORT, 0,            NULL,  event_report},
    {EV_10MS,   0,            NULL,  task_10ms},
//    {EV_20MS,   0,            NULL,  task_20ms},
    {EV_100MS,  0,            NULL,  task_100ms},
    {EV_SEC,    0,            NULL,  task_sec},
#if KEY_REG
    {EV_KEY,    0,            NULL,  pressed_key},
#endif
    {EV_TICK,   ALWAYS_ALIVE, NULL,  sys_tick},
    {EV_MIN,    0,            NULL,  task_min},
    {EV_STATE,  0,            NULL,  state_machine},
};

void task_handle(void)
{
    uint8 i;

    for (i = 0; i < ARRAY_SIZE(tasks); ++i)
    {
        if ((is_task_set(tasks[i].id))
            || (is_task_always_alive(tasks[i].flags)))
        {
            reset_task(tasks[i].id);
            tasks[i].handle(tasks[i].args);
        }
    }
   // wfi();
}






