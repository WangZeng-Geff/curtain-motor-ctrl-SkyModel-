#ifndef __STM8S_RST_H
#define __STM8S_RST_H

/* Includes ------------------------------------------------------------------*/
/* Contains the description of all STM8 hardware registers */
#include "stm8s.h"

/* Exported types ------------------------------------------------------------*/
/** @addtogroup RST_Exported_Types
  * @{
  */
typedef enum {
  RST_FLAG_EMCF    = (u8)0x10, /*!< EMC reset flag */
  RST_FLAG_SWIMF   = (u8)0x08, /*!< SWIM reset flag */
  RST_FLAG_ILLOPF  = (u8)0x04, /*!< Illigal opcode reset flag */
  RST_FLAG_IWDGF   = (u8)0x02, /*!< Independent watchdog reset flag */
  RST_FLAG_WWDGF   = (u8)0x01  /*!< Window watchdog reset flag */
}RST_Flag_TypeDef;

/**
  * @}
  */



/** @addtogroup RST_Private_Macros
  * @{
  */

/**
  * @brief Macro used by the assert function to check the different functions parameters.
  */
/**
  * @brief Macro used by the assert function to check the different RST flags.
  */
#define IS_RST_FLAG_OK(FLAG) (((FLAG) == RST_FLAG_EMCF) || \
                              ((FLAG) == RST_FLAG_SWIMF)  ||\
                              ((FLAG) == RST_FLAG_ILLOPF) ||\
                              ((FLAG) == RST_FLAG_IWDGF)  ||\
                              ((FLAG) == RST_FLAG_WWDGF))

/**
  * @}
  */

/* Exported functions ------------------------------------------------------- */
/** @addtogroup RST_Exported_functions
  * @{
  */
FlagStatus RST_GetFlagStatus(RST_Flag_TypeDef RST_Flag);
void RST_ClearFlag(RST_Flag_TypeDef RST_Flag);

/**
  * @}
  */
#endif /* __STM8S_RST_H */
/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/

