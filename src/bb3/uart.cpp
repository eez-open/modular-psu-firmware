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

#include <bb3/firmware.h>
#include <bb3/memory.h>
#include <bb3/tasks.h>
#include <bb3/uart.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/io_pins.h>
#include <bb3/psu/dlog_record.h>
#include <bb3/psu/scpi/psu.h>
#include <bb3/bp3c/flash_slave.h>

#define PHUART (g_mcuRevision >= MCU_REVISION_R3B3 ? &huart4 : &huart7)

using namespace eez::scpi;
using namespace eez::psu;
using namespace eez::psu::scpi;

uint32_t g_uartBaudRate;
uint32_t g_uartWordLength;
uint32_t g_uartStopBits;
uint32_t g_uartParity;

namespace eez {
namespace uart {

////////////////////////////////////////////////////////////////////////////////

static bool g_initialized = false;
static bool g_scpiInitialized;

static bool g_dmaStarted;
static uint8_t g_buffer[128];
static volatile uint32_t g_RxXferCount;

static bool g_error;
static bool g_abortCplt;
static bool g_abortReceiveCplt;

////////////////////////////////////////////////////////////////////////////////

class CircularBuffer {
public:
	explicit CircularBuffer()
		: m_buffer(0), m_bufferSize(0)
	{
	}

	void setBuffer(uint8_t *buffer, size_t bufferSize)
	{
		m_buffer = buffer;
		m_bufferSize = bufferSize;
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
	size_t m_bufferSize;
	size_t m_head = 0;
	size_t m_tail = 0;
	bool m_full = false;
};

CircularBuffer g_inputBuffer;

////////////////////////////////////////////////////////////////////////////////

static const size_t OUTPUT_BUFFER_MAX_SIZE = 1024;
static char g_outputBuffer[OUTPUT_BUFFER_MAX_SIZE];

static size_t uartOutputBufferWriteFunc(const char *data, size_t len) {
	g_messageAvailable = true;
	if (g_initialized) {
#ifdef EEZ_PLATFORM_STM32
		HAL_UART_Transmit(PHUART, (uint8_t *)data, (uint16_t)len, 1000);
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
	// if (g_initialized) {
    //     char errorOutputBuffer[256];
    //     stringCopy(errorOutputBuffer, sizeof(errorOutputBuffer), "**Reset\r\n");
    //     g_outputBufferWriter.write(errorOutputBuffer, strlen(errorOutputBuffer));
    // }

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

		if (g_RxXferCount == 0) {
			g_RxXferCount = sizeof(g_buffer);
		}
	}
}

void init() {
    g_inputBuffer.setBuffer(UART_BUFFER_MEMORY, UART_BUFFER_MEMORY_SIZE);
}

void tick() {
	if (g_error) {
		//DebugTrace("UART error\n");
		reinit();
		g_error = false;
	}

	if (g_abortCplt) {
		DebugTrace("UART abort cplt\n");
		reinit();
		g_abortCplt = false;
	}

	if (g_abortReceiveCplt) {
		DebugTrace("UART abort receive cplt\n");
		reinit();
		g_abortReceiveCplt = false;
	}

	if (g_initialized && !bp3c::flash_slave::g_bootloaderMode) {
#ifdef EEZ_PLATFORM_STM32
		taskENTER_CRITICAL();
		flushRxBuffer(PHUART->hdmarx->Instance->NDTR);
	    taskEXIT_CRITICAL();
#endif

#ifdef EEZ_PLATFORM_SIMULATOR
	    flushRxBuffer(g_RxXferCount);
#endif

		if (io_pins::g_uartMode == UART_MODE_SCPI) {
			while (!g_inputBuffer.isEmpty()) {
				auto ch = (char)g_inputBuffer.get();
				input(g_scpiContext, &ch, 1);
			}
		} else if (io_pins::g_uartMode == UART_MODE_BOOKMARK) {
			static char bookmarkText[dlog_file::MAX_BOOKMARK_TEXT_LEN];
			static unsigned bookmarkTextLen;

			while (!g_inputBuffer.isEmpty()) {
				auto ch = (char)g_inputBuffer.get();
				if (ch == '\r' || ch == '\n') {
					if (bookmarkTextLen > 0) {
						dlog_record::logBookmark(bookmarkText, bookmarkTextLen);
						bookmarkTextLen = 0;
					}
				} else {
					if (bookmarkTextLen < dlog_file::MAX_BOOKMARK_TEXT_LEN) {
						bookmarkText[bookmarkTextLen++] = ch;
					}
				}
			}
		}
	}
}

void refresh() {
    if (g_initialized) {
        if (!bp3c::flash_slave::g_bootloaderMode && psu::io_pins::g_ioPins[DIN1].function != psu::io_pins::FUNCTION_UART) {
#ifdef EEZ_PLATFORM_STM32
        	if (g_dmaStarted) {
        		auto result = HAL_UART_DMAStop(PHUART);
				if (result != HAL_OK) {
					DebugTrace("HAL_UART_DMAStop error: %d\n", (int)result);
				}
        		g_dmaStarted = false;
        	}
            HAL_UART_DeInit(PHUART);
#endif
            g_initialized = false;
        }
    } else {
        if (bp3c::flash_slave::g_bootloaderMode || psu::io_pins::g_ioPins[DIN1].function == psu::io_pins::FUNCTION_UART) {
#ifdef EEZ_PLATFORM_STM32
			if (bp3c::flash_slave::g_bootloaderMode) {
				g_uartBaudRate = 115200;
				g_uartWordLength = UART_WORDLENGTH_9B;
				g_uartStopBits = UART_STOPBITS_1;
				g_uartParity = UART_PARITY_EVEN;
			} else {
				int dataBits = io_pins::g_uartDataBits + (io_pins::g_uartParity == 0 ? 0 : 1);

				g_uartBaudRate = io_pins::g_uartBaudRate;
				g_uartWordLength = dataBits == 7 ? UART_WORDLENGTH_7B : dataBits == 8 ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;
				g_uartStopBits = io_pins::g_uartStopBits == 1 ? UART_STOPBITS_1 : UART_STOPBITS_2;
				g_uartParity = io_pins::g_uartParity == 0 ? UART_PARITY_NONE : io_pins::g_uartParity == 1 ? UART_PARITY_EVEN : UART_PARITY_ODD;
			}

            if (g_mcuRevision >= MCU_REVISION_R3B3) {
            	MX_UART4_Init();
            } else {
            	MX_UART7_Init();
            }
#endif
            g_initialized = true;
        }
    }

    if (g_initialized) {
    	if (bp3c::flash_slave::g_bootloaderMode) {
        	if (g_dmaStarted) {
#ifdef EEZ_PLATFORM_STM32
        		auto result = HAL_UART_DMAStop(PHUART);
				if (result != HAL_OK) {
					DebugTrace("HAL_UART_DMAStop error: %d\n", (int)result);
				}
#endif
        		g_dmaStarted = false;
        	}
    	} else {
            initScpi();

            if (!g_dmaStarted) {
                g_inputBuffer.reset();
            	g_RxXferCount = sizeof(g_buffer);

#ifdef EEZ_PLATFORM_STM32
				auto result = HAL_UART_Receive_DMA(PHUART, g_buffer, sizeof(g_buffer));
				if (result != HAL_OK) {
					DebugTrace("HAL_UART_Receive_DMA error: %d\n", (int)result);
				}
#endif
        		g_dmaStarted = true;
        	}
    	}
    }
}

void reinit() {
	if (g_initialized) {
#ifdef EEZ_PLATFORM_STM32
		if (g_dmaStarted) {
			auto result = HAL_UART_DMAStop(PHUART);
			if (result != HAL_OK) {
				DebugTrace("HAL_UART_DMAStop error: %d\n", (int)result);
			}
			g_dmaStarted = false;
		}

		HAL_UART_DeInit(PHUART);
#endif

		g_initialized = false;

#ifdef EEZ_PLATFORM_STM32
		if (bp3c::flash_slave::g_bootloaderMode) {
			g_uartBaudRate = 115200;
			g_uartWordLength = UART_WORDLENGTH_9B;
			g_uartStopBits = UART_STOPBITS_1;
			g_uartParity = UART_PARITY_EVEN;
		} else {
			int dataBits = io_pins::g_uartDataBits + (io_pins::g_uartParity == 0 ? 0 : 1);

			g_uartBaudRate = io_pins::g_uartBaudRate;
			g_uartWordLength = dataBits == 7 ? UART_WORDLENGTH_7B : dataBits == 8 ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;
			g_uartStopBits = io_pins::g_uartStopBits == 1 ? UART_STOPBITS_1 : UART_STOPBITS_2;
			g_uartParity = io_pins::g_uartParity == 0 ? UART_PARITY_NONE : io_pins::g_uartParity == 1 ? UART_PARITY_EVEN : UART_PARITY_ODD;
		}

        if (g_mcuRevision >= MCU_REVISION_R3B3) {
            MX_UART4_Init();
        } else {
        	MX_UART7_Init();
        }
#endif

		g_inputBuffer.reset();
		g_RxXferCount = sizeof(g_buffer);

		g_error = false;
		g_abortCplt = false;
		g_abortReceiveCplt = false;

#ifdef EEZ_PLATFORM_STM32
		auto result = HAL_UART_Receive_DMA(PHUART, g_buffer, sizeof(g_buffer));
		if (result != HAL_OK) {
			DebugTrace("HAL_UART_Receive_DMA error: %d\n", (int)result);
		}
#endif
		g_dmaStarted = true;

        g_initialized = true;
	}
}

void disable() {
    if (g_initialized) {
#ifdef EEZ_PLATFORM_STM32
		if (g_dmaStarted) {
			HAL_UART_DMAStop(PHUART);
			g_dmaStarted = false;
		}
		HAL_UART_DeInit(PHUART);
#endif
		g_initialized = false;
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

bool transmit(uint8_t *data, uint16_t size, uint32_t timeout, int *err) {
	if (!g_initialized || bp3c::flash_slave::g_bootloaderMode || io_pins::g_uartMode != UART_MODE_BUFFER) {
		if (*err) {
			*err = SCPI_ERROR_EXECUTION_ERROR;
		}
		return false;
	}

#ifdef EEZ_PLATFORM_STM32
    return HAL_UART_Transmit(PHUART, data, size, timeout) == HAL_OK;
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
    return HAL_ERROR;
#endif
}

bool receive(uint8_t *data, uint16_t size, uint16_t &n, int *err) {
	if (!g_initialized || bp3c::flash_slave::g_bootloaderMode || io_pins::g_uartMode != UART_MODE_BUFFER) {
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

void initScpi() {
    if (!g_scpiInitialized) {
        init(g_scpiContext, g_scpiPsuContext, &g_scpiInterface, g_scpiInputBuffer, SCPI_PARSER_INPUT_BUFFER_LENGTH, g_errorQueueData, SCPI_PARSER_ERROR_QUEUE_SIZE + 1);
        g_scpiInitialized = true;
    }
}

#ifdef EEZ_PLATFORM_SIMULATOR
void simulatorPut(const char *text, size_t textLen) {
	for (size_t i = 0; i < textLen; i++) {
		g_inputBuffer.put(text[i]);
	}
}
#endif

} // uart
} // eez

#ifdef EEZ_PLATFORM_STM32

extern "C" void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {
	using namespace eez::uart;
	if (huart == PHUART) {
		flushRxBuffer(sizeof(g_buffer) / 2);
    }
}

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	using namespace eez::uart;
	if (huart == PHUART) {
		flushRxBuffer(0);
    }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	using namespace eez::uart;
	if (huart == PHUART) {
		g_error = true;
	}
}

extern "C" void HAL_UART_AbortCpltCallback(UART_HandleTypeDef *huart) {
	using namespace eez::uart;
	if (huart == PHUART) {
		g_abortCplt = true;
	}
}

extern "C" void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef *huart) {
	using namespace eez::uart;
	if (huart == PHUART) {
		g_abortReceiveCplt = true;
	}
}

#endif
