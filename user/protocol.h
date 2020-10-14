#ifndef	_PROTOCOL_H_
#define	_PROTOCOL_H_

#include "smart_plc.h"

enum
{
    LOCK_DISABLE = 0,
    LOCK_ENABLE
};

enum
{
    POWERON_INIT = 1,
    MOTORTIME_INIT = 2
};

struct EEP_PARAM
{
    uint8   panid[PANID_LEN];
    uint8   panid_flag;         //0x80:set plc panid, valid panid flag
    uint8   password[PWD_LEN];
    uint8   pwd_magic;          //0x55 is valid , or is 0xAA
    uint8   id[ID_LEN];
    uint8   gateway_id[ID_LEN];
    uint8   sid[SID_LEN];
    uint8   report_enable; //�ϱ�ʹ�ܱ�־

    uint16  motor_up_time; //��������ʱ��
    uint16  motor_dn_time; //��������ʱ��

    uint8  relay_on_delay; //�̵����պ���ʱʱ��        delay time when the relay on 
    uint8  relay_off_delay; //�̵����Ͽ���ʱʱ��        delay time when the relay off
    uint8  degree;
    uint8  acting;
    uint8  lock;
    uint8  enlock;

    uint8_t  SkyModelContral; //���ģ�ͽ�ʹ����Ĭ��0x00
    uint16_t SkyBodyMagicKey;//���ģ�Ϳ��ƶ����ƴ�     
    uint8_t  SkyBodyUperLin;  //���ģ�Ͷ����������� 
    uint8_t  SkyTimeUperLin;  //���ģ�Ͷ�������ʱ������   

    uint8_t  ForceCaliTime;  //ǿ��У׼�����趨Ĭ��30
    uint8_t  ActionCount;    //����У׼��������

};

struct func_ops
{
    uint8	di[2];                       //���ݱ�־
	uint8	(*read)(uint8 *buff, uint8 max_len);//������ִ�к���
    uint8	(*write)(uint8 *buff, uint8 w_len);//д����ִ�к���
};

struct FBD_Frame
{
    uint8 did[2];
    uint8 ctrl;
    uint8 data[1];
};
#define FBD_FRAME_HEAD   offset_of(struct FBD_Frame, data)

struct EVENT_INFOR
{
   uint8 type   :6; 
   uint8 report :1;
   uint8 invalid:1;
};

struct SHS_frame
{
    uint8 stc;
    uint8 said[ADDRESS_LEN];
    uint8 taid[ADDRESS_LEN];
    uint8 seq;
    uint8 length;
    uint8 infor[1];
};

#define SHS_FRAME_HEAD       offset_of(struct SHS_frame, infor)
extern const struct func_ops func_items[];

extern uint8 WaiteTime;

//extern struct RAM  ram;

uint8 compare_soft_ver(uint8 *buff, uint8 len);
static uint8 get_dkey(uint8 *buff, uint8 max_len);
uint8 _get_dev_infor(uint8* buff);
static uint8 get_device_attribute(uint8 *buff, uint8 max_len);
static uint8 get_sn(uint8 *buff, uint8 max_len);

#if DIMMER
void dimmer_100ms_hook(void);
#endif
#if COLOR_DIMMER 
void clour_dimmer_hook(void);
#endif
struct SHS_frame *get_smart_frame(uint8 rxframe_raw[],uint8 rxlen);
uint8 set_group_parameter(uint8 data[], uint8 len);
uint8 set_parameter(uint8 data[], uint8 len);
uint8 read_parameter(uint8 data[], uint8 len);
void _get_dev_type(uint8 *buff);
uint8 is_gid_equal(uint8 data[]);
void SkyModel_IdentifyActions_Count(uint16 *DID);
void SkyModel_CountTime(void);
extern uint8 waite_resetsky_handy(void);

#endif








