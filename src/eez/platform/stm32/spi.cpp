/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(EEZ_PLATFORM_STM32)

#include <spi.h>
#include <cmsis_os.h>

#include <eez/index.h>
#include <eez/platform/stm32/spi.h>
#include <eez/modules/bp3c/comm.h>

namespace eez {
namespace spi {

SPI_HandleTypeDef *handle[] = { &hspi2, &hspi4, &hspi5 };

static GPIO_TypeDef *SPI_CSA_GPIO_Port[] = { SPI2_CSA_GPIO_Port, SPI4_CSA_GPIO_Port, SPI5_CSA_GPIO_Port };
static GPIO_TypeDef *SPI_CSB_GPIO_Port[] = { SPI2_CSB_GPIO_Port, SPI4_CSB_GPIO_Port, SPI5_CSB_GPIO_Port };

static const uint16_t SPI_CSA_Pin[] = { SPI2_CSA_Pin, SPI4_CSA_Pin, SPI5_CSA_Pin };
static const uint16_t SPI_CSB_Pin[] = { SPI2_CSB_Pin, SPI4_CSB_Pin, SPI5_CSB_Pin };

static int g_chip[] = { -1, -1, -1 };

GPIO_TypeDef *IRQ_GPIO_Port[] = { SPI2_IRQ_GPIO_Port, SPI4_IRQ_GPIO_Port, SPI5_IRQ_GPIO_Port };
const uint16_t IRQ_Pin[] = { SPI2_IRQ_Pin, SPI4_IRQ_Pin, SPI5_IRQ_Pin };

void select(uint8_t slotIndex, int chip) {
	auto &slot = *g_slots[slotIndex];

	if (g_chip[slotIndex] != chip) {
		__HAL_SPI_DISABLE(handle[slotIndex]);

		if (chip == CHIP_SLAVE_MCU) {
			uint32_t crcCalculation = (slot.moduleInfo->spiCrcCalculationEnable ? SPI_CRCCALCULATION_ENABLE : SPI_CRCCALCULATION_DISABLE);
			WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | slot.moduleInfo->spiBaudRatePrescaler | SPI_FIRSTBIT_MSB | crcCalculation);
			handle[slotIndex]->Init.CRCCalculation = crcCalculation;
		} else if (chip == CHIP_SLAVE_MCU_BOOTLOADER) {
			// CRC calculation should be always disabled for bootloader
			WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | slot.moduleInfo->spiBaudRatePrescaler | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
			handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		} else if (chip == CHIP_IOEXP || chip == CHIP_TEMP_SENSOR) {
			WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
			handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		} else {
			WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_2EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
			handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		}

		g_chip[slotIndex] = chip;

		// __HAL_SPI_ENABLE(handle[slotIndex]);
	}

    if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP405) {
    	if (chip == CHIP_DAC) {
			// 00
    		SPI_CSA_GPIO_Port[slotIndex]->BSRR = (uint32_t)SPI_CSA_Pin[slotIndex] << 16U; // RESET CSB
    		SPI_CSB_GPIO_Port[slotIndex]->BSRR = (uint32_t)SPI_CSB_Pin[slotIndex] << 16U; // RESET CSB
		} else if (chip == CHIP_ADC) {
    		// 01
    		SPI_CSA_GPIO_Port[slotIndex]->BSRR = SPI_CSA_Pin[slotIndex]; // SET CSA
    		SPI_CSB_GPIO_Port[slotIndex]->BSRR = (uint32_t)SPI_CSB_Pin[slotIndex] << 16U; // RESET CSB
    	} else if (chip == CHIP_IOEXP) {
    		// 10
    		SPI_CSA_GPIO_Port[slotIndex]->BSRR = (uint32_t)SPI_CSA_Pin[slotIndex] << 16U; // RESET CSB
    		SPI_CSB_GPIO_Port[slotIndex]->BSRR = SPI_CSB_Pin[slotIndex]; // SET CSA
    	} else {
    		// TEMP_SENSOR
    		// 11
    		SPI_CSA_GPIO_Port[slotIndex]->BSRR = SPI_CSA_Pin[slotIndex]; // SET CSA
    		SPI_CSB_GPIO_Port[slotIndex]->BSRR = SPI_CSB_Pin[slotIndex]; // SET CSA
    	}
    } else {
		SPI_CSA_GPIO_Port[slotIndex]->BSRR = (uint32_t)SPI_CSA_Pin[slotIndex] << 16U; // RESET CSB
	}
}

void deselect(uint8_t slotIndex) {
	auto &slot = *g_slots[slotIndex];

	if (slot.moduleInfo->moduleType == MODULE_TYPE_DCP405) {
		// 01 ADC
		SPI_CSA_GPIO_Port[slotIndex]->BSRR = SPI_CSA_Pin[slotIndex]; // SET CSA
		SPI_CSB_GPIO_Port[slotIndex]->BSRR = (uint32_t)SPI_CSB_Pin[slotIndex] << 16U; // RESET CSB
	} else {
		SPI_CSA_GPIO_Port[slotIndex]->BSRR = SPI_CSA_Pin[slotIndex]; // SET CSA
	}
}

static HAL_StatusTypeDef SPI_WaitFifoStateUntilTimeout(SPI_HandleTypeDef *hspi, uint32_t Fifo, uint32_t State,
                                                       uint32_t Timeout, uint32_t Tickstart)
{
  while ((hspi->Instance->SR & Fifo) != State)
  {
    if ((Fifo == SPI_SR_FRLVL) && (State == SPI_FRLVL_EMPTY))
    {
      /* Read 8bit CRC to flush Data Register */
      READ_REG(*((__IO uint8_t *)&hspi->Instance->DR));
    }

    if (Timeout != HAL_MAX_DELAY)
    {
      if (((HAL_GetTick() - Tickstart) >= Timeout) || (Timeout == 0U))
      {
        hspi->State = HAL_SPI_STATE_READY;

        /* Process Unlocked */
        __HAL_UNLOCK(hspi);

        return HAL_TIMEOUT;
      }
    }
  }

  return HAL_OK;
}

static HAL_StatusTypeDef SPI_WaitFlagStateUntilTimeout(SPI_HandleTypeDef *hspi, uint32_t Flag, FlagStatus State,
                                                       uint32_t Timeout, uint32_t Tickstart)
{
  while ((__HAL_SPI_GET_FLAG(hspi, Flag) ? SET : RESET) != State)
  {
    if (Timeout != HAL_MAX_DELAY)
    {
      if (((HAL_GetTick() - Tickstart) >= Timeout) || (Timeout == 0U))
      {
        hspi->State = HAL_SPI_STATE_READY;

        /* Process Unlocked */
        __HAL_UNLOCK(hspi);

        return HAL_TIMEOUT;
      }
    }
  }

  return HAL_OK;
}

static HAL_StatusTypeDef SPI_EndRxTxTransaction(SPI_HandleTypeDef *hspi, uint32_t Timeout, uint32_t Tickstart)
{
  /* Control if the TX fifo is empty */
  if (SPI_WaitFifoStateUntilTimeout(hspi, SPI_FLAG_FTLVL, SPI_FTLVL_EMPTY, Timeout, Tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
    return HAL_TIMEOUT;
  }

  /* Control the BSY flag */
  if (SPI_WaitFlagStateUntilTimeout(hspi, SPI_FLAG_BSY, RESET, Timeout, Tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
    return HAL_TIMEOUT;
  }

  /* Control if the RX fifo is empty */
  if (SPI_WaitFifoStateUntilTimeout(hspi, SPI_FLAG_FRLVL, SPI_FRLVL_EMPTY, Timeout, Tickstart) != HAL_OK)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
    return HAL_TIMEOUT;
  }

  return HAL_OK;
}

HAL_StatusTypeDef SPI_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size,
                                          uint32_t Timeout)
{
  uint16_t             initial_TxXferCount;
  uint16_t             initial_RxXferCount;
  uint32_t             tmp_mode;
  HAL_SPI_StateTypeDef tmp_state;
  uint32_t             tickstart;

  /* Variable used to alternate Rx and Tx during transfer */
  uint32_t             txallowed = 1U;
  HAL_StatusTypeDef    errorcode = HAL_OK;

  /* Check Direction parameter */
  assert_param(IS_SPI_DIRECTION_2LINES(hspi->Init.Direction));

  /* Process Locked */
  __HAL_LOCK(hspi);

  /* Init tickstart for timeout management*/
  tickstart = HAL_GetTick();

  /* Init temporary variables */
  tmp_state           = hspi->State;
  tmp_mode            = hspi->Init.Mode;
  initial_TxXferCount = Size;
  initial_RxXferCount = Size;

  if (!((tmp_state == HAL_SPI_STATE_READY) || \
        ((tmp_mode == SPI_MODE_MASTER) && (hspi->Init.Direction == SPI_DIRECTION_2LINES) && (tmp_state == HAL_SPI_STATE_BUSY_RX))))
  {
    errorcode = HAL_BUSY;
    goto error;
  }

  if ((pTxData == NULL) || (pRxData == NULL) || (Size == 0U))
  {
    errorcode = HAL_ERROR;
    goto error;
  }

  /* Don't overwrite in case of HAL_SPI_STATE_BUSY_RX */
  if (hspi->State != HAL_SPI_STATE_BUSY_RX)
  {
    hspi->State = HAL_SPI_STATE_BUSY_TX_RX;
  }

  /* Set the transaction information */
  hspi->ErrorCode   = HAL_SPI_ERROR_NONE;
  hspi->pRxBuffPtr  = (uint8_t *)pRxData;
  hspi->RxXferCount = Size;
  hspi->RxXferSize  = Size;
  hspi->pTxBuffPtr  = (uint8_t *)pTxData;
  hspi->TxXferCount = Size;
  hspi->TxXferSize  = Size;

  /*Init field not used in handle to zero */
  hspi->RxISR       = NULL;
  hspi->TxISR       = NULL;

  /* Set the Rx Fifo threshold */
  if ((hspi->Init.DataSize > SPI_DATASIZE_8BIT) || (initial_RxXferCount > 1U))
  {
    /* Set fiforxthreshold according the reception data length: 16bit */
    CLEAR_BIT(hspi->Instance->CR2, SPI_RXFIFO_THRESHOLD);
  }
  else
  {
    /* Set fiforxthreshold according the reception data length: 8bit */
    SET_BIT(hspi->Instance->CR2, SPI_RXFIFO_THRESHOLD);
  }

  /* Check if the SPI is already enabled */
  if ((hspi->Instance->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE)
  {
    /* Enable SPI peripheral */
    __HAL_SPI_ENABLE(hspi);
  }

  /* Transmit and Receive data in 16 Bit mode */
  if (hspi->Init.DataSize > SPI_DATASIZE_8BIT)
  {
    if ((hspi->Init.Mode == SPI_MODE_SLAVE) || (initial_TxXferCount == 0x01U))
    {
      hspi->Instance->DR = *((uint16_t *)hspi->pTxBuffPtr);
      hspi->pTxBuffPtr += sizeof(uint16_t);
      hspi->TxXferCount--;
    }
    while ((hspi->TxXferCount > 0U) || (hspi->RxXferCount > 0U))
    {
      /* Check TXE flag */
      if ((__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE)) && (hspi->TxXferCount > 0U) && (txallowed == 1U))
      {
        hspi->Instance->DR = *((uint16_t *)hspi->pTxBuffPtr);
        hspi->pTxBuffPtr += sizeof(uint16_t);
        hspi->TxXferCount--;
        /* Next Data is a reception (Rx). Tx not allowed */
        txallowed = 0U;
      }

      /* Check RXNE flag */
      if ((__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE)) && (hspi->RxXferCount > 0U))
      {
        *((uint16_t *)hspi->pRxBuffPtr) = (uint16_t)hspi->Instance->DR;
        hspi->pRxBuffPtr += sizeof(uint16_t);
        hspi->RxXferCount--;
        /* Next Data is a Transmission (Tx). Tx is allowed */
        txallowed = 1U;
      }
      if (((HAL_GetTick() - tickstart) >=  Timeout) && (Timeout != HAL_MAX_DELAY))
      {
        errorcode = HAL_TIMEOUT;
        goto error;
      }
    }
  }
  /* Transmit and Receive data in 8 Bit mode */
  else
  {
    if ((hspi->Init.Mode == SPI_MODE_SLAVE) || (initial_TxXferCount == 0x01U))
    {
      if (hspi->TxXferCount > 1U)
      {
        hspi->Instance->DR = *((uint16_t *)hspi->pTxBuffPtr);
        hspi->pTxBuffPtr += sizeof(uint16_t);
        hspi->TxXferCount -= 2U;
      }
      else
      {
        *(__IO uint8_t *)&hspi->Instance->DR = (*hspi->pTxBuffPtr);
        hspi->pTxBuffPtr++;
        hspi->TxXferCount--;
      }
    }
    while ((hspi->TxXferCount > 0U) || (hspi->RxXferCount > 0U))
    {
      /* Check TXE flag */
      if ((__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE)) && (hspi->TxXferCount > 0U) && (txallowed == 1U))
      {
        if (hspi->TxXferCount > 1U)
        {
          hspi->Instance->DR = *((uint16_t *)hspi->pTxBuffPtr);
          hspi->pTxBuffPtr += sizeof(uint16_t);
          hspi->TxXferCount -= 2U;
        }
        else
        {
          *(__IO uint8_t *)&hspi->Instance->DR = (*hspi->pTxBuffPtr);
          hspi->pTxBuffPtr++;
          hspi->TxXferCount--;
        }
        /* Next Data is a reception (Rx). Tx not allowed */
        txallowed = 0U;
      }

      /* Wait until RXNE flag is reset */
      if ((__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE)) && (hspi->RxXferCount > 0U))
      {
        if (hspi->RxXferCount > 1U)
        {
          *((uint16_t *)hspi->pRxBuffPtr) = (uint16_t)hspi->Instance->DR;
          hspi->pRxBuffPtr += sizeof(uint16_t);
          hspi->RxXferCount -= 2U;
          if (hspi->RxXferCount <= 1U)
          {
            /* Set RX Fifo threshold before to switch on 8 bit data size */
            SET_BIT(hspi->Instance->CR2, SPI_RXFIFO_THRESHOLD);
          }
        }
        else
        {
          (*(uint8_t *)hspi->pRxBuffPtr) = *(__IO uint8_t *)&hspi->Instance->DR;
          hspi->pRxBuffPtr++;
          hspi->RxXferCount--;
        }
        /* Next Data is a Transmission (Tx). Tx is allowed */
        txallowed = 1U;
      }
      if ((((HAL_GetTick() - tickstart) >=  Timeout) && ((Timeout != HAL_MAX_DELAY))) || (Timeout == 0U))
      {
        errorcode = HAL_TIMEOUT;
        goto error;
      }
    }
  }

  /* Check the end of the transaction */
  if (SPI_EndRxTxTransaction(hspi, Timeout, tickstart) != HAL_OK)
  {
    errorcode = HAL_ERROR;
    hspi->ErrorCode = HAL_SPI_ERROR_FLAG;
  }

error :
  hspi->State = HAL_SPI_STATE_READY;
  __HAL_UNLOCK(hspi);
  return errorcode;
}

HAL_StatusTypeDef transfer1(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 1, 100);
}

HAL_StatusTypeDef transfer2(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 2, 100);
}

HAL_StatusTypeDef transfer3(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 3, 100);
}

HAL_StatusTypeDef transfer4(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 4, 100);
}

HAL_StatusTypeDef transfer5(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 5, 100);
}

HAL_StatusTypeDef transfer(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, size, 100);
}

HAL_StatusTypeDef transmit(uint8_t slotIndex, uint8_t *input, uint16_t size) {
    return HAL_SPI_Transmit(handle[slotIndex], input, size, 100);
}

HAL_StatusTypeDef transferDMA(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size) {
    return HAL_SPI_TransmitReceive_DMA(handle[slotIndex], input, output, size);
}

} // namespace spi
} // namespace eez

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
	using namespace eez;
	using namespace eez::spi;
	using namespace eez::bp3c::comm;

	uint8_t slotIndex;
	if (hspi == handle[0]) {
		slotIndex = 0;
	} else if (hspi == handle[1]) {
		slotIndex = 1;
	} else {
		slotIndex = 2;
	}

	deselect(slotIndex);

	auto &slot = *g_slots[slotIndex];

	slot.onSpiDmaTransferCompleted(TRANSFER_STATUS_OK);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
	using namespace eez;
	using namespace eez::spi;
	using namespace eez::bp3c::comm;

	uint8_t slotIndex;
	if (hspi == handle[0]) {
		slotIndex = 0;
	} else if (hspi == handle[1]) {
		slotIndex = 1;
	} else {
		slotIndex = 2;
	}

	deselect(slotIndex);

	auto &slot = *g_slots[slotIndex];

	if (spi::handle[slotIndex]->ErrorCode == HAL_SPI_ERROR_CRC) {
		slot.onSpiDmaTransferCompleted(TRANSFER_STATUS_CRC_ERROR);
	} else {
		slot.onSpiDmaTransferCompleted(TRANSFER_STATUS_ERROR);
	}
}

#endif // EEZ_PLATFORM_STM32
