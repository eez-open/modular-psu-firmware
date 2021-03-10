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

static const size_t CONF_UART_INPUT_BUFFER_SIZE = 256;

namespace eez {
namespace uart {

////////////////////////////////////////////////////////////////////////////////

static bool g_initialized = false;
static bool g_scpiInitialized;

static uint8_t g_buffer[128];
static volatile uint32_t g_RxXferCount;
// static bool g_rxCplt = true;
static volatile uint32_t g_rxState = 0;

////////////////////////////////////////////////////////////////////////////////

class CircularBuffer {
public:
	explicit CircularBuffer(uint8_t *buffer, size_t bufferSize)
		: m_buffer(buffer), m_bufferSize(bufferSize)
	{
	}

	void reset() {
#ifdef EEZ_PLATFORM_STM32
		taskENTER_CRITICAL();
#endif
		m_head = m_tail;
		m_full = false;

#ifdef EEZ_PLATFORM_STM32
		taskEXIT_CRITICAL();
#endif
	}

	bool isEmpty() const {
		return !m_full && m_head == m_tail;
	}

	bool isFull() const {
		return m_full;
	}

	size_t getSize() const {
		if (m_full) {
			return m_bufferSize;
		}
		if(m_head >= m_tail) {
			return m_head - m_tail;
		}
		return m_bufferSize + m_head - m_tail;
	}

	void put(uint8_t item) {
		m_buffer[m_head] = item;

		if (m_full) {
			m_tail = (m_tail + 1) % m_bufferSize;
		}

		m_head = (m_head + 1) % m_bufferSize;

		m_full = m_head == m_tail;
	}

	uint8_t get() {
#ifdef EEZ_PLATFORM_STM32
		taskENTER_CRITICAL();
#endif

		if (isEmpty()) {
#ifdef EEZ_PLATFORM_STM32
			taskEXIT_CRITICAL();
#endif
			return 0;
		}

		uint8_t val = m_buffer[m_tail];
		m_full = false;
		m_tail = (m_tail + 1) % m_bufferSize;

#ifdef EEZ_PLATFORM_STM32
		taskEXIT_CRITICAL();
#endif
		return val;
	}

private:
	uint8_t *m_buffer;
	const size_t m_bufferSize;
	size_t m_head = 0;
	size_t m_tail = 0;
	bool m_full = false;
};

static uint8_t g_inputBufferMemory[CONF_UART_INPUT_BUFFER_SIZE];
CircularBuffer g_inputBuffer(g_inputBufferMemory, sizeof(g_inputBufferMemory));

////////////////////////////////////////////////////////////////////////////////

static const size_t OUTPUT_BUFFER_MAX_SIZE = 1024;
static char g_outputBuffer[OUTPUT_BUFFER_MAX_SIZE];

static size_t uartOutputBufferWriteFunc(const char *data, size_t len) {
	g_messageAvailable = true;
	if (g_initialized) {
#ifdef EEZ_PLATFORM_STM32
		HAL_UART_Transmit(PHUART, (uint8_t *)data, (uint16_t)len, 10);
#endif
	}
    return len;
}

static OutputBufferWriter g_outputBufferWriter(
    &g_outputBuffer[0], OUTPUT_BUFFER_MAX_SIZE, uartOutputBufferWriteFunc
);

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

static void flushRxBuffer(uint16_t RxXferCount) {
	if (RxXferCount < g_RxXferCount) {
		auto n = g_RxXferCount - RxXferCount;
		auto p = g_buffer + sizeof(g_buffer) - g_RxXferCount;
		for (uint32_t i = 0; i < n; i++) {
			g_inputBuffer.put(p[i]);
		}

		g_RxXferCount = RxXferCount;
	}
}

void tick() {
	if (g_initialized && !bp3c::flash_slave::g_bootloaderMode) {
#ifdef EEZ_PLATFORM_STM32
		taskENTER_CRITICAL();
		flushRxBuffer(PHUART->hdmarx->Instance->NDTR);
	    taskEXIT_CRITICAL();
#endif

#ifdef EEZ_PLATFORM_SIMULATOR
	    flushRxBuffer(g_RxXferCount);
#endif

		if (persist_conf::devConf.uartMode == UART_MODE_SCPI) {
			while (!g_inputBuffer.isEmpty()) {
				auto ch = (char)g_inputBuffer.get();
				input(g_scpiContext, &ch, 1);
			}
		}
	}
}

void refresh() {
    if (g_initialized) {
        if (!bp3c::flash_slave::g_bootloaderMode && psu::io_pins::g_ioPins[DIN1].function != psu::io_pins::FUNCTION_UART) {
//        	if (!g_rxCplt) {
//        		g_rxCplt = true;
//        		HAL_UART_AbortReceive_IT(PHUART);
//        	}
        	HAL_UART_DMAStop(PHUART);

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

            g_inputBuffer.reset();
            if (!bp3c::flash_slave::g_bootloaderMode) {
            	g_RxXferCount = sizeof(g_buffer);
            	HAL_UART_Receive_DMA(PHUART, g_buffer, sizeof(g_buffer));
            }
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

HAL_StatusTypeDef receive(uint8_t *data, uint16_t size, uint32_t timeout, uint16_t *nreceived) {
    if (nreceived) {
        *nreceived = 0;
    }

    if (!g_initialized) {
        return HAL_ERROR;
    }

#ifdef EEZ_PLATFORM_STM32
    auto result = HAL_UART_Receive(PHUART, data, size, timeout);
    if (result == HAL_OK || result == HAL_TIMEOUT) {
    	*nreceived = size - PHUART->RxXferCount;
    }
    return result;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return HAL_ERROR;
#endif
}

bool receiveFromBuffer(uint8_t *data, uint16_t size, uint16_t &n, int *err) {
	if (!g_initialized || bp3c::flash_slave::g_bootloaderMode || persist_conf::devConf.uartMode == UART_MODE_SCPI) {
		if (*err) {
			*err = SCPI_ERROR_EXECUTION_ERROR;
		}
		return false;
	}

	for (n = 0; !g_inputBuffer.isEmpty() && n < size; n++) {
		data[n] = g_inputBuffer.get();
	}

	return true;
}

void resetInputBuffer() {
	g_inputBuffer.reset();
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

extern "C" void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {
	using namespace eez::uart;
	if (huart == PHUART) {
		g_rxState = 1;

		flushRxBuffer(sizeof(g_buffer) / 2);
   }
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	using namespace eez::uart;
	if (huart == PHUART) {
		g_rxState = 2;

		flushRxBuffer(0);

		g_RxXferCount = sizeof(g_buffer);
   }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	using namespace eez::uart;
	if (huart == PHUART) {
		g_rxState = 3;
	}
}

#endif
