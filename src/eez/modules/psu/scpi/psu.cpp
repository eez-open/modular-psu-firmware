/* / mcu / sound.h
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

#include <stdio.h>

#include <eez/sound.h>
#include <eez/system.h>

#include <eez/scpi/commands.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/serial_psu.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

#define SCPI_COMMAND(P, C) scpi_result_t C(scpi_t *context);
SCPI_COMMANDS
#undef SCPI_COMMAND

#define SCPI_COMMAND(P, C) { P, C },
static const scpi_command_t scpi_commands[] = { SCPI_COMMANDS SCPI_CMD_LIST_END };

////////////////////////////////////////////////////////////////////////////////

bool g_messageAvailable = false;
bool g_stbQueryExecuted = false;

////////////////////////////////////////////////////////////////////////////////

void init(scpi_t &scpi_context, scpi_psu_t &scpi_psu_context, scpi_interface_t *interface,
          char *input_buffer, size_t input_buffer_length, scpi_error_t *error_queue_data,
          int16_t error_queue_size) {
    SCPI_Init(&scpi_context, scpi_commands, interface, scpi_units_def, IDN_MANUFACTURER, IDN_MODEL,
              getSerialNumber(), MCU_FIRMWARE, input_buffer, input_buffer_length,
              error_queue_data, error_queue_size);


    if (CH_NUM > 0) {
        auto &channel = Channel::get(0);
        scpi_psu_context.selectedChannels.numChannels = 1;
        scpi_psu_context.selectedChannels.channels[0].slotIndex = channel.slotIndex;
        scpi_psu_context.selectedChannels.channels[0].subchannelIndex = channel.subchannelIndex;
    } else {
        scpi_psu_context.selectedChannels.numChannels = 0;
    }

    scpi_psu_context.currentDirectory[0] = 0;
    scpi_psu_context.isBufferOverrun = false;
    scpi_psu_context.bufferOverrunTime = 0;

    scpi_context.user_context = &scpi_psu_context;

    emptyBuffer(scpi_context);
}

void emptyBuffer(scpi_t &context) {
    SCPI_Input(&context, 0, 0);
}

void onBufferOverrun(scpi_t &context) {
    emptyBuffer(context);
    scpi_psu_t *psu_context = (scpi_psu_t *)context.user_context;
    psu_context->isBufferOverrun = true;
    psu_context->bufferOverrunTime = micros();
}

void input(scpi_t &context, const char *str, size_t size) {
    scpi_psu_t *psu_context = (scpi_psu_t *)context.user_context;
    if (psu_context->isBufferOverrun) {
        // wait for 500ms of idle input to declare buffer overrun finished
        uint32_t tickCount = micros();
        int32_t diff = tickCount - psu_context->bufferOverrunTime;
        if (diff > 500000) {
            psu_context->isBufferOverrun = false;
        } else {
            psu_context->bufferOverrunTime = tickCount;
            return;
        }
    }

    g_messageAvailable = false;
    g_stbQueryExecuted = false;

    int result = SCPI_Input(&context, str, size);
    if (result == -1) {
        onBufferOverrun(context);
    } else {
        if (g_messageAvailable && !g_stbQueryExecuted) {
            // set MAV (Message AVailable) bit
            SCPI_RegSet(&context, SCPI_REG_STB, SCPI_RegGet(&context, SCPI_REG_STB) | STB_MAV);
        }
    }
}

int printError(scpi_t *context, int_fast16_t err, OutputBufferWriter &outputBufferWriter) {
    if (err != 0) {
        sound::playBeep();

        char errorOutputBuffer[512];

        outputBufferWriter.write("**ERROR", sizeof("**ERROR"));

        char datetime_buffer[32] = { 0 };
        if (datetime::getDateTimeAsString(datetime_buffer)) {
            sprintf(errorOutputBuffer, " [%s]", datetime_buffer);
            outputBufferWriter.write(errorOutputBuffer, strlen(errorOutputBuffer));
        }

        sprintf(errorOutputBuffer, ": %d,\"%s\"\r\n", (int16_t)err, SCPI_ErrorTranslate(err));
        outputBufferWriter.write(errorOutputBuffer, strlen(errorOutputBuffer));

        if (err == SCPI_ERROR_INPUT_BUFFER_OVERRUN) {
            scpi::onBufferOverrun(*context);
        }
    }

    return 0;
}

void resultChoiceName(scpi_t *context, scpi_choice_def_t *choice, int tag) {
    for (; choice->name; ++choice) {
        if (choice->tag == tag) {
            char text[64];

            // copy choice name while upper case letter or digit, examples: IMMediate -> IMM, PIN1
            // -> PIN1
            const char *src = choice->name;
            char *dst = text;
            while (*src && (isUperCaseLetter(*src) || isDigit(*src))) {
                *dst++ = *src++;
            }
            *dst = 0;

            SCPI_ResultText(context, text);
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

OutputBufferWriter::OutputBufferWriter(char *buffer, size_t maxBufferSize, size_t(*writeFunc)(const char *data, size_t len))
    : m_buffer(buffer)
    , m_maxBufferSize(maxBufferSize)
    , m_writeFunc(writeFunc)
{
}

size_t OutputBufferWriter::write(const char *data, size_t len) {
    g_messageAvailable = true;

    const char *restData = nullptr;
    size_t restLen = 0;

    const char *endOfLine = strstr(data, "\r\n");
    if (endOfLine) {
        restData = endOfLine + 2;
        size_t n = restData - data;
        restLen = len - n;
        len = n;
    }

    if (m_bufferSize + len < m_maxBufferSize) {
        memcpy(m_buffer + m_bufferSize, data, len);
        m_bufferSize += len;
        len = 0;
    } else {
        int n = m_maxBufferSize - m_bufferSize;
        memcpy(m_buffer + m_bufferSize, data, n);
        m_bufferSize += n;
        data += n;
        len -= n;
    }

    if ((m_bufferSize >= 2 && m_buffer[m_bufferSize - 2] == '\r' && m_buffer[m_bufferSize - 1] == '\n') || m_bufferSize == m_maxBufferSize) {
        m_writeFunc(m_buffer, m_bufferSize);
        m_bufferSize = 0;
    }

    if (len > 0) {
        write(data, len);
    }

    if (restLen > 0) {
        write(restData, restLen);
    }

    return len;
}

} // namespace scpi
} // namespace psu
} // namespace eez