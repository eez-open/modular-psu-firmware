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

#include <math.h>
#include <stdio.h>

#if EEZ_PLATFORM_STM32
#include <main.h>
#endif

#include <eez/firmware.h>
#include <eez/system.h>

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/event_queue.h>
#if OPTION_DISPLAY
#include <eez/modules/psu/gui/psu.h>
#endif

#include <eez/modules/mcu/eeprom.h>

#include <eez/modules/bp3c/flash_slave.h>
#include <eez/modules/bp3c/io_exp.h>
#include <eez/modules/bp3c/eeprom.h>

#include <eez/modules/dib-dcp405/dib-dcp405.h>

#include <eez/modules/fpga/prog.h>

#ifdef MASTER_MCU_REVISION_R3B3_OR_NEWER
extern bool g_supervisorWatchdogEnabled;
#endif

namespace eez {
namespace psu {

#ifdef DEBUG
using namespace debug;
#endif // DEBUG

namespace scpi {

scpi_result_t scpi_cmd_debug(scpi_t *context) {
#ifdef DEBUG
    int32_t cmd;
    if (SCPI_ParamInt32(context, &cmd, false)) {
        if (cmd == 23) {
#if defined(EEZ_PLATFORM_STM32)
            taskENTER_CRITICAL();
#endif

            mcu::eeprom::resetAllExceptOnTimeCounters();

#if defined(EEZ_PLATFORM_STM32)
            taskEXIT_CRITICAL();
            restart();
#endif
        } else if (cmd == 24) {
        	DebugTrace("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam tristique, nisl vitae interdum molestie, tortor nulla condimentum ligula, et egestas risus tortor sodales augue. Proin a congue arcu. Morbi in odio eu eros tincidunt dictum et at metus. In at quam erat. Mauris est ligula, consequat vehicula felis sit amet, blandit sollicitudin augue. Sed ornare purus ut nisi euismod ultrices. Sed rhoncus eros sapien, ac ullamcorper risus blandit ac. Nulla ac aliquam sapien, nec euismod nibh. Fusce volutpat fermentum libero sit amet iaculis. Donec in augue sapien. Vivamus vitae urna sodales, dapibus nisi sed, rhoncus urna.");
        } else if (cmd == 25) {
            if (psu::gui::getActivePageId() != PAGE_ID_TOUCH_TEST) {
                psu::gui::showPage(PAGE_ID_TOUCH_TEST);
            } else {
                psu::gui::showPage(PAGE_ID_MAIN);
            }
        } else if (cmd == 26) {
        	psu::gui::showPage(PAGE_ID_DEBUG_VARIABLES);
        } else if (cmd == 27) {
            int32_t relay;
            if (SCPI_ParamInt32(context, &relay, true)) {
                bp3c::io_exp::switchChannelCoupling(relay);
            }
        } else if (cmd == 28) {
        	psu::gui::showPage(PAGE_ID_DEBUG_POWER_CHANNELS);
        } else if (cmd == 29) {
            int32_t slotIndex;
            if (!SCPI_ParamInt32(context, &slotIndex, false)) {
                return SCPI_RES_ERR;
            }

            if (slotIndex < 1 || slotIndex > 3) {
                SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
                return SCPI_RES_ERR;
            }

            slotIndex--;

#if defined(EEZ_PLATFORM_STM32)
            taskENTER_CRITICAL();
#endif

            bp3c::eeprom::resetAllExceptOnTimeCounters(slotIndex);

#if defined(EEZ_PLATFORM_STM32)
            taskEXIT_CRITICAL();
            restart();
#endif
        } else if (cmd == 30) {
#if defined(EEZ_PLATFORM_STM32)
            taskENTER_CRITICAL();
#endif
            while(1);
        } else if (cmd == 31) {
            char filePath[MAX_PATH_LENGTH + 1];
            if (!getFilePath(context, filePath, true)) {
                return SCPI_RES_ERR;
            }
            return fpga::prog(filePath);
        } 
#ifdef MASTER_MCU_REVISION_R3B3_OR_NEWER
        else if (cmd == 32) {
            bool enable;
            if (!SCPI_ParamBool(context, &enable, TRUE)) {
                return SCPI_RES_ERR;
            }
            g_supervisorWatchdogEnabled = enable;
        }
#endif
        else {
            SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
            return SCPI_RES_ERR;
        }
    } else {
        // do nothing
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugQ(scpi_t *context) {
#ifdef DEBUG
    int32_t cmd;
    if (SCPI_ParamInt32(context, &cmd, false)) {
        if (cmd == 23) {
            bp3c::io_exp::init();
            bp3c::io_exp::test();
            SCPI_ResultBool(context, bp3c::io_exp::g_testResult == TEST_OK);
            return SCPI_RES_OK;
        } else if (cmd == 24) {
            SCPI_ResultText(context, MCU_FIRMWARE_BUILD_DATE " " MCU_FIRMWARE_BUILD_TIME);
            return SCPI_RES_OK;
        } else if (cmd == 25) {
#if EEZ_PLATFORM_STM32
        	char idCode[20];
        	snprintf(idCode, sizeof(idCode), "0x%08X", (unsigned int)DBGMCU->IDCODE);
            SCPI_ResultText(context, idCode);
#endif
            return SCPI_RES_OK;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
            return SCPI_RES_ERR;
        }
    }

    static char buffer[2048];

#ifndef __EMSCRIPTEN__
    for (int i = 0; i < CH_NUM; i++) {
        if (!measureAllAdcValuesOnChannel(i)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
    }
#endif

    debug::dumpVariables(buffer);

    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugOntimeQ(scpi_t *context) {
#ifdef DEBUG
    char buffer[512] = { 0 };
    char *p = buffer;

    sprintf(p, "power active: %d\n", int(ontime::g_mcuCounter.isActive() ? 1 : 0));
    p += strlen(p);

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        sprintf(p, "CH%d active: %d\n", channel.channelIndex + 1, int(ontime::g_mcuCounter.isActive() ? 1 : 0));
        p += strlen(p);
    }

    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugVoltage(scpi_t *context) {
#ifdef DEBUG
    Channel *channel = getPowerChannelFromParam(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    uint32_t value;
    if (!SCPI_ParamUInt32(context, &value, true)) {
        return SCPI_RES_ERR;
    }

    channel->setDacVoltage((uint16_t)value);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugCurrent(scpi_t *context) {
#ifdef DEBUG
    Channel *channel = getPowerChannelFromParam(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    uint32_t value;
    if (!SCPI_ParamUInt32(context, &value, true)) {
        return SCPI_RES_ERR;
    }

    channel->setDacCurrent((uint16_t)value);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugMeasureVoltage(scpi_t *context) {
#ifdef DEBUG
    if (serial::g_testResult != TEST_OK) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    Channel *channel = getPowerChannelFromParam(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    while (true) {
        uint32_t tickCount = micros();
        temperature::tick(tickCount);

#if OPTION_FAN
        aux_ps::fan::tick(tickCount);
#endif

        channel->adcMeasureUMon();

        Serial.print((int)debug::g_uMon[channel->channelIndex].get());
        Serial.print(" ");
        Serial.print(channel->u.mon_last, 5);
        Serial.println("V");

        int32_t diff = micros() - tickCount;
        if (diff < 48000L) {
            delayMicroseconds(48000L - diff);
        }
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugMeasureCurrent(scpi_t *context) {
#ifdef DEBUG
    if (serial::g_testResult != TEST_OK) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    Channel *channel = getPowerChannelFromParam(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    while (true) {
        uint32_t tickCount = micros();
        temperature::tick(tickCount);

#if OPTION_FAN
        aux_ps::fan::tick(tickCount);
#endif

        channel->adcMeasureIMon();

        Serial.print((int)debug::g_iMon[channel->channelIndex].get());
        Serial.print(" ");
        Serial.print(channel->i.mon_last, 5);
        Serial.println("A");

        int32_t diff = micros() - tickCount;
        if (diff < 48000L) {
            delayMicroseconds(48000L - diff);
        }
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugFan(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_debugFanQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_debugFanPid(scpi_t *context) {
#if OPTION_FAN
    double Kp;
    if (!SCPI_ParamDouble(context, &Kp, TRUE)) {
        return SCPI_RES_ERR;
    }

    double Ki;
    if (!SCPI_ParamDouble(context, &Ki, TRUE)) {
        return SCPI_RES_ERR;
    }

    double Kd;
    if (!SCPI_ParamDouble(context, &Kd, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t POn;
    if (!SCPI_ParamInt(context, &POn, TRUE)) {
        return SCPI_RES_ERR;
    }

    aux_ps::fan::setPidTunings(Kp, Ki, Kd, POn);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_debugFanPidQ(scpi_t *context) {
#if OPTION_FAN
    double Kp[4] = { aux_ps::fan::g_Kp, aux_ps::fan::g_Ki, aux_ps::fan::g_Kd, aux_ps::fan::g_POn * 1.0f };

    SCPI_ResultArrayDouble(context, Kp, 4, SCPI_FORMAT_ASCII);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_debugCsvQ(scpi_t *context) {
    const int count = 1000;
    double *arr = (double *)malloc(count * sizeof(double));
    for (int i = 0; i < count; ++i) {
        arr[i] = 1.0 * rand() / RAND_MAX;
	}
    SCPI_ResultArrayDouble(context, arr, count, SCPI_FORMAT_ASCII);
    free(arr);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_debugIoexp(scpi_t *context) {
#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    int32_t bit;
    if (!SCPI_ParamInt(context, &bit, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (bit < 0 || bit > 15) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    int32_t direction;
    if (!SCPI_ParamInt(context, &direction, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (direction != channel->getIoExpBitDirection(bit)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    bool state;
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    channel->changeIoExpBit(bit, state);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugIoexpQ(scpi_t *context) {
#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    Channel *channel = getSelectedPowerChannel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    int32_t bit;
    if (!SCPI_ParamInt(context, &bit, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (bit < 0 || bit > 15) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    int32_t direction;
    if (!SCPI_ParamInt(context, &direction, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (direction != channel->getIoExpBitDirection(bit)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    bool state = channel->testIoExpBit(bit);

    SCPI_ResultBool(context, state);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugDcm220Q(scpi_t *context) {
#if defined(EEZ_PLATFORM_STM32)
	char text[100];
	sprintf(text, "TODO");
	SCPI_ResultText(context, text);
	return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugDownloadFirmware(scpi_t *context) {
#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    int32_t slotIndex;
    if (!SCPI_ParamInt32(context, &slotIndex, true)) {
        return SCPI_RES_ERR;
    }

    if (slotIndex < 1 || slotIndex > 3) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    char hexFilePath[MAX_PATH_LENGTH + 1];
    if (!getFilePath(context, hexFilePath, true)) {
        return SCPI_RES_ERR;
    }

    bp3c::flash_slave::start(slotIndex - 1, hexFilePath);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugEvent(scpi_t *context) {
    int32_t eventId;
    if (!SCPI_ParamInt(context, &eventId, TRUE)) {
        return SCPI_RES_ERR;
    }

    event_queue::pushEvent(eventId);

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
