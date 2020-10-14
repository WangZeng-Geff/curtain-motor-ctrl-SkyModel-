#include "stm8s_rst.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private Constants ---------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/
/**
  * @addtogroup RST_Public_Functions
  * @{
  */


/**
  * @brief Checks whether the specified RST flag is set or not.
  * @param[in] RST_Flag : specify the reset flag to check.
  * can be one of the values of @ref RST_Flag_TypeDef.
  * @retval FlagStatus: status of the given RST flag.
  * @par Required preconditions:
  * None
  */
FlagStatus RST_GetFlagStatus(RST_Flag_TypeDef RST_Flag)
{
  /* Check the parameters */
  assert_param(IS_RST_FLAG_OK(RST_Flag));

  /* Get flag status */

  return ((FlagStatus)((u8)RST->SR & (u8)RST_Flag));
}

/**
  * @brief Clears the specified RST flag.
  * @param[in] RST_Flag : specify the reset flag to clear.
  * can be one of the values of @ref RST_Flag_TypeDef.
  * @retval None
  * @par Required preconditions:
  * None
  */
void RST_ClearFlag(RST_Flag_TypeDef RST_Flag)
{
  /* Check the parameters */
  assert_param(IS_RST_FLAG_OK(RST_Flag));

  RST->SR = (u8)RST_Flag;
}

/**
  * @}
  */

/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/



