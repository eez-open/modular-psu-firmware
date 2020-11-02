/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SPI4_CSA_Pin GPIO_PIN_3
#define SPI4_CSA_GPIO_Port GPIOE
#define SPI4_CSB_Pin GPIO_PIN_4
#define SPI4_CSB_GPIO_Port GPIOE
#define DIN2_Pin GPIO_PIN_13
#define DIN2_GPIO_Port GPIOC
#define UART_RX_DIN1_Pin GPIO_PIN_6
#define UART_RX_DIN1_GPIO_Port GPIOF
#define NFAULT_Pin GPIO_PIN_2
#define NFAULT_GPIO_Port GPIOB
#define TFT_BRIGHTNESS_Pin GPIO_PIN_15
#define TFT_BRIGHTNESS_GPIO_Port GPIOB
#define SPI2_CSB_Pin GPIO_PIN_11
#define SPI2_CSB_GPIO_Port GPIOD
#define SPI2_CSA_Pin GPIO_PIN_12
#define SPI2_CSA_GPIO_Port GPIOD
#define OE_SYNC_Pin GPIO_PIN_13
#define OE_SYNC_GPIO_Port GPIOD
#define PWR_DIRECT_Pin GPIO_PIN_2
#define PWR_DIRECT_GPIO_Port GPIOG
#define PWR_SSTART_Pin GPIO_PIN_3
#define PWR_SSTART_GPIO_Port GPIOG
#define ENC_A_Pin GPIO_PIN_6
#define ENC_A_GPIO_Port GPIOC
#define ENC_A_EXTI_IRQn EXTI9_5_IRQn
#define ENC_B_Pin GPIO_PIN_7
#define ENC_B_GPIO_Port GPIOC
#define ENC_B_EXTI_IRQn EXTI9_5_IRQn
#define MCLK_25_Pin GPIO_PIN_9
#define MCLK_25_GPIO_Port GPIOC
#define SPI2_IRQ_Pin GPIO_PIN_8
#define SPI2_IRQ_GPIO_Port GPIOA
#define SPI2_IRQ_EXTI_IRQn EXTI9_5_IRQn
#define USB_OTG_FS_ID_Pin GPIO_PIN_10
#define USB_OTG_FS_ID_GPIO_Port GPIOA
#define SPI5_IRQ_Pin GPIO_PIN_15
#define SPI5_IRQ_GPIO_Port GPIOA
#define SPI5_IRQ_EXTI_IRQn EXTI15_10_IRQn
#define SD_DETECT_Pin GPIO_PIN_11
#define SD_DETECT_GPIO_Port GPIOC
#define SD_DETECT_EXTI_IRQn EXTI15_10_IRQn
#define USB_OTG_FS_OC_Pin GPIO_PIN_4
#define USB_OTG_FS_OC_GPIO_Port GPIOD
#define USB_OTG_FS_PSO_Pin GPIO_PIN_5
#define USB_OTG_FS_PSO_GPIO_Port GPIOD
#define IRQ_TOUCH_Pin GPIO_PIN_7
#define IRQ_TOUCH_GPIO_Port GPIOD
#define USER_SW_Pin GPIO_PIN_9
#define USER_SW_GPIO_Port GPIOG
#define SPI5_CSB_Pin GPIO_PIN_13
#define SPI5_CSB_GPIO_Port GPIOG
#define SPI5_CSA_Pin GPIO_PIN_14
#define SPI5_CSA_GPIO_Port GPIOG
#define UART_TX_DOUT1_Pin GPIO_PIN_4
#define UART_TX_DOUT1_GPIO_Port GPIOB
#define DOUT2_Pin GPIO_PIN_5
#define DOUT2_GPIO_Port GPIOB
#define SPI4_IRQ_Pin GPIO_PIN_9
#define SPI4_IRQ_GPIO_Port GPIOB
#define SPI4_IRQ_EXTI_IRQn EXTI9_5_IRQn
#define ENC_SW_Pin GPIO_PIN_4
#define ENC_SW_GPIO_Port GPIOI
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
