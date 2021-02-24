/*
* EEZ Generic Firmware
* Copyright (C) 2020-present, Envox d.o.o.
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

#include <stdio.h>

#include <eez/firmware.h>
#include <eez/tasks.h>
#include <eez/uart.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/io_pins.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/bp3c/flash_slave.h>

#define PHUART (g_mcuRevision >= MCU_REVISION_R3B3 ? &huart4 : &huart7)

using namespace eez::scpi;
using namespace eez::psu;
using namespace eez::psu::scpi;

namespace eez {
namespace uart {

////////////////////////////////////////////////////////////////////////////////

static bool g_initialized = false;
static bool g_scpiInitialized;
// static uint8_t g_buffer[128];
// static uint16_t g_RxXferCount;
// static bool g_rxCplt = true;
// static bool g_txCplt = true;

////////////////////////////////////////////////////////////////////////////////

static const size_t OUTPUT_BUFFER_MAX_SIZE = 1024;
static char g_outputBuffer[OUTPUT_BUFFER_MAX_SIZE];

size_t uartWrite(const char *data, size_t len) {
	g_messageAvailable = true;
	if (g_initialized) {
        //g_txCplt = false;
#ifdef EEZ_PLATFORM_STM32
		HAL_UART_Transmit(PHUART, (uint8_t *)data, (uint16_t)len, 10);
#endif
//        while (!g_txCplt) {
//            osDelay(1);
//        }
	}
    return len;
}

static OutputBufferWriter g_outputBufferWriter(&g_outputBuffer[0], OUTPUT_BUFFER_MAX_SIZE, uartWrite);

////////////////////////////////////////////////////////////////////////////////

size_t SCPI_Write(scpi_t *context, const char *data, size_t len) {
    g_outputBufferWriter.write(data, len);
    return len;
}

scpi_result_t SCPI_Flush(scpi_t *context) {
    g_outputBufferWriter.flush();
    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t *context, int_fast16_t err) {
    return printError(context, err, g_outputBufferWriter);
}

scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    if (g_initialized) {
        char errorOutputBuffer[256];
        if (SCPI_CTRL_SRQ == ctrl) {
            snprintf(errorOutputBuffer, sizeof(errorOutputBuffer), "**SRQ: 0x%X (%d)\r\n", val, val);
        } else {
            snprintf(errorOutputBuffer, sizeof(errorOutputBuffer), "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
        }
        g_outputBufferWriter.write(errorOutputBuffer, strlen(errorOutputBuffer));
    }

    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t *context) {
	if (g_initialized) {
        char errorOutputBuffer[256];
        stringCopy(errorOutputBuffer, sizeof(errorOutputBuffer), "**Reset\r\n");
        g_outputBufferWriter.write(errorOutputBuffer, strlen(errorOutputBuffer));
    }

    return reset() ? SCPI_RES_OK : SCPI_RES_ERR;
}

////////////////////////////////////////////////////////////////////////////////

static scpi_reg_val_t g_scpiPsuRegs[SCPI_PSU_REG_COUNT];
static scpi_psu_t g_scpiPsuContext = { g_scpiPsuRegs };

static scpi_interface_t g_scpiInterface = {
    SCPI_Error, SCPI_Write, SCPI_Control, SCPI_Flush, SCPI_Reset,
};

static char g_scpiInputBuffer[SCPI_PARSER_INPUT_BUFFER_LENGTH];
static scpi_error_t g_errorQueueData[SCPI_PARSER_ERROR_QUEUE_SIZE + 1];

scpi_t g_scpiContext;

////////////////////////////////////////////////////////////////////////////////

void tick() {
#ifdef EEZ_PLATFORM_STM32
//	if (g_rxCplt == true) {
//		if (g_initialized && !bp3c::flash_slave::g_bootloaderMode) {
//			g_rxCplt = false;
//			g_RxXferCount = sizeof(g_buffer);
//			HAL_UART_Receive_IT(PHUART, g_buffer, sizeof(g_buffer));
//		}
//	} else {
//		auto RxXferCount = PHUART->RxXferCount;
//	    if (RxXferCount < g_RxXferCount) {
//	        input(g_scpiContext, (const char *)g_buffer + sizeof(g_buffer) - g_RxXferCount, g_RxXferCount - RxXferCount);
//	        g_RxXferCount = RxXferCount;
//	    }
//	}

    uint8_t buffer[128];
	HAL_UART_Receive(PHUART, buffer, sizeof(buffer), 1);
	auto RxXferCount = PHUART->RxXferCount;
	if (RxXferCount < sizeof(buffer)) {
		input(g_scpiContext, (const char *)buffer, sizeof(buffer) - RxXferCount);
	}
#endif
}

void refresh() {
    if (g_initialized) {
        if (!bp3c::flash_slave::g_bootloaderMode && psu::io_pins::g_ioPins[DIN1].function != psu::io_pins::FUNCTION_UART) {
        	// if (!g_rxCplt) {
        	// 	g_rxCplt = true;
        	// 	HAL_UART_AbortReceive_IT(PHUART);
        	// }

#ifdef EEZ_PLATFORM_STM32
            HAL_UART_DeInit(PHUART);
#endif
            g_initialized = false;
        }
    } else {
        if (bp3c::flash_slave::g_bootloaderMode || psu::io_pins::g_ioPins[DIN1].function == psu::io_pins::FUNCTION_UART) {
            if (g_mcuRevision >= MCU_REVISION_R3B3) {
#ifdef EEZ_PLATFORM_STM32            
                MX_UART4_Init();
#endif
            } else {
#ifdef EEZ_PLATFORM_STM32
                MX_UART7_Init();
#endif
            }
            g_initialized = true;

            initScpi();
        }
    }
}

HAL_StatusTypeDef transmit(uint8_t *data, uint16_t size, uint32_t timeout) {
    if (!g_initialized) {
        return HAL_ERROR;
    }

#ifdef EEZ_PLATFORM_STM32
    return HAL_UART_Transmit(PHUART, data, size, timeout);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return HAL_ERROR;
#endif
}

HAL_StatusTypeDef receive(uint8_t *data, uint16_t size, uint32_t timeout) {
    if (!g_initialized) {
        return HAL_ERROR;
    }

#ifdef EEZ_PLATFORM_STM32
    return HAL_UART_Receive(PHUART, data, size, timeout);
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return HAL_ERROR;
#endif
}

void initScpi() {
    if (!g_scpiInitialized) {
        init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);
        g_scpiInitialized = true;
    }
}

} // uart
} // eez

#ifdef EEZ_PLATFORM_STM32

// extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
//     if (huart == PHUART) {
// 		eez::uart::g_rxCplt = true;
//     }
// }

// extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
//     if (huart == PHUART) {
// 		eez::uart::g_txCplt = true;
//     }
// }

// extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
//     if (huart == PHUART) {
//         eez::uart::g_rxCplt = true;
//         eez::uart::g_txCplt = true;
//     }
// }

#endif
