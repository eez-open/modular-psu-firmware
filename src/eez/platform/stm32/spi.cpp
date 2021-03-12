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
#include <eez/tasks.h>
#include <eez/firmware.h>
#include <eez/modules/psu/conf_advanced.h>
#include <eez/platform/stm32/spi.h>
#include <eez/modules/bp3c/comm.h>

namespace eez {
namespace spi {

SPI_HandleTypeDef *handle[] = { &hspi2, &hspi4, &hspi5 };

static GPIO_TypeDef *SPI_CSA_GPIO_Port[] = { SPI2_NSS_GPIO_Port, SPI4_NSS_GPIO_Port, SPI5_NSS_GPIO_Port };
static GPIO_TypeDef *SPI_CSB_GPIO_Port[] = { SPI2_CSB_GPIO_Port, SPI4_CSB_GPIO_Port, SPI5_CSB_GPIO_Port };
static const uint16_t SPI_CSA_Pin[] = { SPI2_NSS_Pin, SPI4_NSS_Pin, SPI5_NSS_Pin };
static const uint16_t SPI_CSB_Pin[] = { SPI2_CSB_Pin, SPI4_CSB_Pin, SPI5_CSB_Pin };

static GPIO_TypeDef *R2B4_SPI_CSA_GPIO_Port[] = { R2B4_SPI2_NSS_GPIO_Port, R2B4_SPI4_NSS_GPIO_Port, R2B4_SPI5_NSS_GPIO_Port };
static GPIO_TypeDef *R2B4_SPI_CSB_GPIO_Port[] = { SPI2_CSB_GPIO_Port, R2B4_SPI4_CSB_GPIO_Port, SPI5_CSB_GPIO_Port };
static const uint16_t R2B4_SPI_CSA_Pin[] = { R2B4_SPI2_NSS_Pin, R2B4_SPI4_NSS_Pin, R2B4_SPI5_NSS_Pin };
static const uint16_t R2B4_SPI_CSB_Pin[] = { SPI2_CSB_Pin, R2B4_SPI4_CSB_Pin, SPI5_CSB_Pin };

static int g_chip[] = { -1, -1, -1 };

GPIO_TypeDef *IRQ_GPIO_Port[] = { SPI2_IRQ_GPIO_Port, SPI4_IRQ_GPIO_Port, SPI5_IRQ_GPIO_Port };
const uint16_t IRQ_Pin[] = { SPI2_IRQ_Pin, SPI4_IRQ_Pin, SPI5_IRQ_Pin };

void select(uint8_t slotIndex, int chip) {
    auto &slot = *g_slots[slotIndex];

    if (g_chip[slotIndex] != chip) {
        __HAL_SPI_DISABLE(handle[slotIndex]);

        if (chip == CHIP_SLAVE_MCU) {
            uint32_t crcCalculation = (slot.spiCrcCalculationEnable ? SPI_CRCCALCULATION_ENABLE : SPI_CRCCALCULATION_DISABLE);
            WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | slot.spiBaudRatePrescaler | SPI_FIRSTBIT_MSB | crcCalculation);
            handle[slotIndex]->Init.CRCCalculation = crcCalculation;
        } else if (chip == CHIP_SLAVE_MCU_NO_CRC) {
            WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | slot.spiBaudRatePrescaler | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
            handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        } else if (chip == CHIP_IOEXP || chip == CHIP_TEMP_SENSOR) {
            WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
            handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        } else if (chip == CHIP_FPGA) {
            WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_1LINE | SPI_POLARITY_HIGH | SPI_PHASE_1EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
            handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        } else {
            WRITE_REG(handle[slotIndex]->Instance->CR1, SPI_MODE_MASTER | SPI_DIRECTION_2LINES | SPI_POLARITY_LOW | SPI_PHASE_2EDGE | (SPI_NSS_SOFT & SPI_CR1_SSM) | SPI_BAUDRATEPRESCALER_16 | SPI_FIRSTBIT_MSB | SPI_CRCCALCULATION_DISABLE);
            handle[slotIndex]->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        }

        g_chip[slotIndex] = chip;

        // __HAL_SPI_ENABLE(handle[slotIndex]);
    }

    if (g_mcuRevision >= MCU_REVISION_R3B3) {
		if (slot.moduleType == MODULE_TYPE_DCP405) {
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
    } else {
		if (slot.moduleType == MODULE_TYPE_DCP405) {
			if (chip == CHIP_DAC) {
				// 00
				R2B4_SPI_CSA_GPIO_Port[slotIndex]->BSRR = (uint32_t)R2B4_SPI_CSA_Pin[slotIndex] << 16U; // RESET CSB
				R2B4_SPI_CSB_GPIO_Port[slotIndex]->BSRR = (uint32_t)R2B4_SPI_CSB_Pin[slotIndex] << 16U; // RESET CSB
			} else if (chip == CHIP_ADC) {
				// 01
				R2B4_SPI_CSA_GPIO_Port[slotIndex]->BSRR = R2B4_SPI_CSA_Pin[slotIndex]; // SET CSA
				R2B4_SPI_CSB_GPIO_Port[slotIndex]->BSRR = (uint32_t)R2B4_SPI_CSB_Pin[slotIndex] << 16U; // RESET CSB
			} else if (chip == CHIP_IOEXP) {
				// 10
				R2B4_SPI_CSA_GPIO_Port[slotIndex]->BSRR = (uint32_t)R2B4_SPI_CSA_Pin[slotIndex] << 16U; // RESET CSB
				R2B4_SPI_CSB_GPIO_Port[slotIndex]->BSRR = R2B4_SPI_CSB_Pin[slotIndex]; // SET CSA
			} else {
				// TEMP_SENSOR
				// 11
				R2B4_SPI_CSA_GPIO_Port[slotIndex]->BSRR = R2B4_SPI_CSA_Pin[slotIndex]; // SET CSA
				R2B4_SPI_CSB_GPIO_Port[slotIndex]->BSRR = R2B4_SPI_CSB_Pin[slotIndex]; // SET CSA
			}
		} else {
			R2B4_SPI_CSA_GPIO_Port[slotIndex]->BSRR = (uint32_t)R2B4_SPI_CSA_Pin[slotIndex] << 16U; // RESET CSB
		}
    }
}

void deselect(uint8_t slotIndex) {
    auto &slot = *g_slots[slotIndex];

    if (g_mcuRevision >= MCU_REVISION_R3B3) {
		if (slot.moduleType == MODULE_TYPE_DCP405) {
			// 01 ADC
			SPI_CSA_GPIO_Port[slotIndex]->BSRR = SPI_CSA_Pin[slotIndex]; // SET CSA
			SPI_CSB_GPIO_Port[slotIndex]->BSRR = (uint32_t)SPI_CSB_Pin[slotIndex] << 16U; // RESET CSB
		} else {
			SPI_CSA_GPIO_Port[slotIndex]->BSRR = SPI_CSA_Pin[slotIndex]; // SET CSA
		}
    } else {
		if (slot.moduleType == MODULE_TYPE_DCP405) {
			// 01 ADC
			R2B4_SPI_CSA_GPIO_Port[slotIndex]->BSRR = R2B4_SPI_CSA_Pin[slotIndex]; // SET CSA
			R2B4_SPI_CSB_GPIO_Port[slotIndex]->BSRR = (uint32_t)R2B4_SPI_CSB_Pin[slotIndex] << 16U; // RESET CSB
		} else {
			R2B4_SPI_CSA_GPIO_Port[slotIndex]->BSRR = R2B4_SPI_CSA_Pin[slotIndex]; // SET CSA
		}
    }
}

HAL_StatusTypeDef SPI_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size) {
    // Variable used to alternate Rx and Tx during transfer
    bool txAllowed = true;

    // Set the transaction information
    uint8_t *pRxBuffPtr  = (uint8_t *)pRxData;
    uint16_t RxXferCount = Size;
    uint8_t *pTxBuffPtr  = (uint8_t *)pTxData;
    uint16_t TxXferCount = Size;

    // Set the Rx Fifo threshold
    if (Size > 1) {
        // Set fiforxthreshold according the reception data length: 16bit
        CLEAR_BIT(hspi->Instance->CR2, SPI_RXFIFO_THRESHOLD);
    } else {
        // Set fiforxthreshold according the reception data length: 8bit
        SET_BIT(hspi->Instance->CR2, SPI_RXFIFO_THRESHOLD);
    }

    // Check if the SPI is already enabled
    if ((hspi->Instance->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE) {
        // Enable SPI peripheral
        __HAL_SPI_ENABLE(hspi);
    }

    // Transmit and Receive data in 8 Bit mode
    if (Size == 1) {
        if (TxXferCount > 1) {
            hspi->Instance->DR = *((uint16_t *)pTxBuffPtr);
            pTxBuffPtr += 2;
            TxXferCount -= 2;
        } else {
            *(__IO uint8_t *)&hspi->Instance->DR = *pTxBuffPtr;
            pTxBuffPtr++;
            TxXferCount--;
        }
    }

    while (TxXferCount > 0 || RxXferCount > 0) {
        // Check TXE flag
        if ((__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE)) && (TxXferCount > 0U) && txAllowed) {
            if (TxXferCount > 1) {
                hspi->Instance->DR = *((uint16_t *)pTxBuffPtr);
                pTxBuffPtr += 2;
                TxXferCount -= 2;
            } else {
                *(__IO uint8_t *)&hspi->Instance->DR = *pTxBuffPtr;
                pTxBuffPtr++;
                TxXferCount--;
            }

            // Next Data is a reception (Rx). Tx not allowed
            txAllowed = false;
        }

        // Wait until RXNE flag is reset
        if ((__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE)) && (RxXferCount > 0)) {
            if (RxXferCount > 1) {
                *((uint16_t *)pRxBuffPtr) = (uint16_t)hspi->Instance->DR;
                pRxBuffPtr += 2;
                RxXferCount -= 2;

                if (RxXferCount <= 1) {
                    // Set RX Fifo threshold before to switch on 8 bit data size
                    SET_BIT(hspi->Instance->CR2, SPI_RXFIFO_THRESHOLD);
                }
            } else {
                (*(uint8_t *)pRxBuffPtr) = *(__IO uint8_t *)&hspi->Instance->DR;
                pRxBuffPtr++;
                RxXferCount--;
            }

            // Next Data is a Transmission (Tx). Tx is allowed
            txAllowed = true;
        }
    }

    // Check the end of the transaction
    
    // Control if the TX fifo is empty
    while ((hspi->Instance->SR & SPI_FLAG_FTLVL) != SPI_FTLVL_EMPTY) {
        if (SPI_FLAG_FTLVL == SPI_SR_FRLVL) {
            /* Read 8bit CRC to flush Data Register */
            READ_REG(*((__IO uint8_t *)&hspi->Instance->DR));
        }
    }
    
    // Control the BSY flag
    while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_BSY));

    // Control if the RX fifo is empty
    while ((hspi->Instance->SR & SPI_FLAG_FRLVL) != SPI_FTLVL_EMPTY) {
        if (SPI_FLAG_FRLVL == SPI_SR_FRLVL) {
            /* Read 8bit CRC to flush Data Register */
            READ_REG(*((__IO uint8_t *)&hspi->Instance->DR));
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef transfer1(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 1);
}

HAL_StatusTypeDef transfer2(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 2);
}

HAL_StatusTypeDef transfer3(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 3);
}

HAL_StatusTypeDef transfer4(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 4);
}

HAL_StatusTypeDef transfer5(uint8_t slotIndex, uint8_t *input, uint8_t *output) {
	return SPI_TransmitReceive(handle[slotIndex], input, output, 5);
}

HAL_StatusTypeDef transfer(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size) {
	return HAL_SPI_TransmitReceive(handle[slotIndex], input, output, size, 5);
}

HAL_StatusTypeDef transmit(uint8_t slotIndex, uint8_t *input, uint16_t size) {
    return HAL_SPI_Transmit(handle[slotIndex], input, size, 5);
}

HAL_StatusTypeDef transferDMA(uint8_t slotIndex, uint8_t *input, uint8_t *output, uint16_t size) {
    return HAL_SPI_TransmitReceive_DMA(handle[slotIndex], input, output, size);
}

void abortTransfer(uint8_t slotIndex) {
	HAL_SPI_Abort(handle[slotIndex]);
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

    if (g_isBooted) {
	    sendMessageToPsu(PSU_MESSAGE_SPI_DMA_TRANSFER_COMPLETED, slotIndex | (TRANSFER_STATUS_OK << 8));
    } else {
        g_slots[slotIndex]->onSpiDmaTransferCompleted(TRANSFER_STATUS_OK);
    }
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

	if (handle[slotIndex]->ErrorCode == HAL_SPI_ERROR_CRC) {
        if (g_isBooted) {
            sendMessageToPsu(PSU_MESSAGE_SPI_DMA_TRANSFER_COMPLETED, slotIndex | (TRANSFER_STATUS_CRC_ERROR << 8));
        } else {
            g_slots[slotIndex]->onSpiDmaTransferCompleted(TRANSFER_STATUS_CRC_ERROR);
        }
	} else {
        if (g_isBooted) {
            sendMessageToPsu(PSU_MESSAGE_SPI_DMA_TRANSFER_COMPLETED, slotIndex | (TRANSFER_STATUS_ERROR << 8));
        } else {
            g_slots[slotIndex]->onSpiDmaTransferCompleted(TRANSFER_STATUS_ERROR);
        }
	}
}

#endif // EEZ_PLATFORM_STM32
