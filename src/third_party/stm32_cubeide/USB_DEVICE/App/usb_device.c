/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usb_device.c
  * @version        : v1.0_Cube
  * @brief          : This file implements the USB Device
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

/* Includes ------------------------------------------------------------------*/

#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

/* USER CODE BEGIN Includes */

#include "usbd_msc.h"
#include "usbd_storage_if.h"

/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Device Core handle declaration. */
USBD_HandleTypeDef hUsbDeviceFS;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */
int g_mxUsbDeviceOperationUsbResult;
int g_mxUsbDeviceOperationResult;

extern USBD_DescriptorsTypeDef FS_Desc_MSC;
extern int g_usbDeviceClass;
extern int g_selectedMassStorageDevice;

extern USBD_StorageTypeDef g_fsDriver_USBD_Storage_Interface;

void MX_USB_DEVICE_DeInit(void) {
  g_mxUsbDeviceOperationUsbResult = USBD_Stop(&hUsbDeviceFS);
  if (g_mxUsbDeviceOperationUsbResult != USBD_OK){
    g_mxUsbDeviceOperationResult = -1;
  }

  g_mxUsbDeviceOperationUsbResult = USBD_DeInit(&hUsbDeviceFS);
  if (g_mxUsbDeviceOperationUsbResult != USBD_OK){
    g_mxUsbDeviceOperationResult = -2;
  }

  g_mxUsbDeviceOperationUsbResult = USBD_OK;
  g_mxUsbDeviceOperationResult = 0;
}
/* USER CODE END 1 */

/**
  * Init USB device Library, add supported class and start the library
  * @retval None
  */
void MX_USB_DEVICE_Init(void)
{
  /* USER CODE BEGIN USB_DEVICE_Init_PreTreatment */
  if (g_usbDeviceClass == 0) {
    /* Init Device Library, add supported class and start the library. */
    g_mxUsbDeviceOperationUsbResult = USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    if (g_mxUsbDeviceOperationUsbResult != USBD_OK) {
      g_mxUsbDeviceOperationResult = -1;
      return;
    }

    g_mxUsbDeviceOperationUsbResult = USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    if (g_mxUsbDeviceOperationUsbResult != USBD_OK) {
      g_mxUsbDeviceOperationResult = -2;
      return;
    }

    g_mxUsbDeviceOperationUsbResult = USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
    if (g_mxUsbDeviceOperationUsbResult != USBD_OK) {
      g_mxUsbDeviceOperationResult = -3;
      return;
    }

    g_mxUsbDeviceOperationUsbResult = USBD_Start(&hUsbDeviceFS);
    if (g_mxUsbDeviceOperationUsbResult != USBD_OK) {
      g_mxUsbDeviceOperationResult = -4;
      return;
    }
  } else {
    /* Init Device Library, add supported class and start the library. */
    g_mxUsbDeviceOperationUsbResult = USBD_Init(&hUsbDeviceFS, &FS_Desc_MSC, DEVICE_FS);
    if (g_mxUsbDeviceOperationUsbResult != USBD_OK) {
      g_mxUsbDeviceOperationResult = -1;
      return;
    }
    g_mxUsbDeviceOperationUsbResult = USBD_RegisterClass(&hUsbDeviceFS, &USBD_MSC);
    if (g_mxUsbDeviceOperationUsbResult != USBD_OK) {
      g_mxUsbDeviceOperationResult = -2;
      return;
    }

    g_mxUsbDeviceOperationUsbResult = USBD_MSC_RegisterStorage(&hUsbDeviceFS,
      g_selectedMassStorageDevice == 0 ? &USBD_Storage_Interface_fops_FS : &g_fsDriver_USBD_Storage_Interface);
    if (g_mxUsbDeviceOperationUsbResult != USBD_OK) {
      g_mxUsbDeviceOperationResult = -3;
      return;
    }

    g_mxUsbDeviceOperationUsbResult = USBD_Start(&hUsbDeviceFS);
    if (g_mxUsbDeviceOperationUsbResult != USBD_OK) {
      g_mxUsbDeviceOperationResult = -4;
      return;
    }
  }
#if 0    
  /* USER CODE END USB_DEVICE_Init_PreTreatment */

  /* Init Device Library, add supported class and start the library. */
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
  {
    Error_Handler();
  }
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN USB_DEVICE_Init_PostTreatment */
#endif

  g_mxUsbDeviceOperationUsbResult = USBD_OK;
  g_mxUsbDeviceOperationResult = 0;
  /* USER CODE END USB_DEVICE_Init_PostTreatment */
}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
