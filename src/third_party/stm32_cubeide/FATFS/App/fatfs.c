/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

#include "fatfs.h"

uint8_t retSD;    /* Return value for SD */
char SDPath[4];   /* SD logical drive path */
FATFS SDFatFS;    /* File system object for SD logical drive */
FIL SDFile;       /* File object for SD */

/* USER CODE BEGIN Variables */

#include <stm32f7xx_hal_rtc.h>
#include <rtc.h>

/* USER CODE END Variables */

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the SD driver ###########################*/
  retSD = FATFS_LinkDriver(&SD_Driver, SDPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */     
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
    RTC_TimeTypeDef time;
    HAL_RTC_GetTime(&hrtc, &time, 0);

    RTC_DateTypeDef date;
    HAL_RTC_GetDate(&hrtc, &date, 0);

    DWORD res = (((DWORD)(2000 + date.Year - 1980)) << 25)
        | ((DWORD)date.Month << 21)
        | ((DWORD)date.Date << 16)
        | (WORD)(time.Hours << 11)
        | (WORD)(time.Minutes << 5)
        | (WORD)(time.Seconds >> 1);

    return res;
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */
     
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
