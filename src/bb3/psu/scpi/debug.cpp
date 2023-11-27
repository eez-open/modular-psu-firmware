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

#include <eez/gui/gui.h>

#include <bb3/firmware.h>
#include <bb3/system.h>
#include <eez/core/alloc.h>

#if OPTION_FAN
#include <bb3/aux_ps/fan.h>
#endif

#include <bb3/psu/psu.h>
#include <bb3/psu/serial_psu.h>
#include <bb3/psu/temperature.h>
#include <bb3/psu/ontime.h>
#include <bb3/psu/scpi/psu.h>
#include <bb3/psu/event_queue.h>
#include <bb3/psu/gui/psu.h>

#include <eez/core/eeprom.h>

#include <bb3/bp3c/flash_slave.h>
#include <bb3/bp3c/io_exp.h>
#include <bb3/bp3c/eeprom.h>

#include <bb3/dib-dcp405/dib-dcp405.h>

#include <bb3/fpga/prog.h>

#include <bb3/memory.h>

extern bool g_supervisorWatchdogEnabled;

namespace eez {
namespace psu {

#ifdef DEBUG
using namespace debug;
#endif // DEBUG

namespace scpi {

uint32_t g_startTime;

scpi_result_t scpi_cmd_debug(scpi_t *context) {
#ifdef DEBUG
    int32_t cmd;
    if (SCPI_ParamInt32(context, &cmd, false)) {
        if (cmd == 23) {
#if defined(EEZ_PLATFORM_STM32)
            taskENTER_CRITICAL();
#endif

            persist_conf::resetAllExceptOnTimeCountersMCU();

#if defined(EEZ_PLATFORM_STM32)
            taskEXIT_CRITICAL();
            NVIC_SystemReset();
#endif
        } else if (cmd == 24) {
        	DebugTrace("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam tristique, nisl vitae interdum molestie, tortor nulla condimentum ligula, et egestas risus tortor sodales augue. Proin a congue arcu. Morbi in odio eu eros tincidunt dictum et at metus. In at quam erat. Mauris est ligula, consequat vehicula felis sit amet, blandit sollicitudin augue. Sed ornare purus ut nisi euismod ultrices. Sed rhoncus eros sapien, ac ullamcorper risus blandit ac. Nulla ac aliquam sapien, nec euismod nibh. Fusce volutpat fermentum libero sit amet iaculis. Donec in augue sapien. Vivamus vitae urna sodales, dapibus nisi sed, rhoncus urna.");
        } else if (cmd == 25) {
            if (psu::gui::getActivePageId() != PAGE_ID_TOUCH_TEST) {
                psu::gui::showPage(PAGE_ID_TOUCH_TEST);
            } else {
                psu::gui::showPage(PAGE_ID_MAIN);
            }
        } else if (cmd == 27) {
            int32_t relay;
            if (SCPI_ParamInt32(context, &relay, true)) {
                bp3c::io_exp::switchChannelCoupling(relay);
            }
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

            persist_conf::resetAllExceptOnTimeCountersBP3C(slotIndex);

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
#if defined(EEZ_PLATFORM_STM32)        
        else if (cmd == 32) {
            bool enable;
            if (!SCPI_ParamBool(context, &enable, TRUE)) {
                return SCPI_RES_ERR;
            }
            g_supervisorWatchdogEnabled = enable;
        } 
#endif
        else if (cmd == 33) {
            persist_conf::clearMcuRevision();
            restart();
        }
		else if (cmd == 34) {
			g_startTime = millis();
		} 
#if defined(GUI_CALC_FPS) && defined(STYLE_ID_FPS_GRAPH)
        else if (cmd == 35) {
			using namespace eez::gui;
			using namespace eez::gui::display;
			g_drawFpsGraphEnabled = !g_drawFpsGraphEnabled;
            g_calcFpsEnabled = g_drawFpsGraphEnabled;
			refreshScreen();
			return SCPI_RES_OK;
		} 
#endif
        else if (cmd == 35) {
            SCPI_ResultText(context, "test");
            return SCPI_RES_OK;
        }
        else {
            SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
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

static char *g_buffer = (char *)DLOG_RECORD_BUFFER;

void dumpFreq(scpi_t *context) {
	g_buffer[0] = 0;
	stringAppendString(g_buffer, DLOG_RECORD_BUFFER_SIZE, "frequencies\r\n");
    for (int i = 0; i < 290; i++) {
		char buffer[100];
        snprintf(buffer, sizeof(buffer), "%g\r\n", 240000000.0 + i * 2487889.0);
        stringAppendString(g_buffer, DLOG_RECORD_BUFFER_SIZE, buffer);
    }
    SCPI_ResultCharacters(context, g_buffer, strlen(g_buffer));
}

void dumpTrace1(scpi_t *context) {
	g_buffer[0] = 0;
    for (int i = 0; i < 290; i++) {
		char buffer[100];
        snprintf(buffer, sizeof(buffer), "trace 1 value %d %g\r\n", i, i * 2.0);
        stringAppendString(g_buffer, DLOG_RECORD_BUFFER_SIZE, buffer);
    }
    SCPI_ResultCharacters(context, g_buffer, strlen(g_buffer));
}

void dumpTrace2(scpi_t *context) {
	g_buffer[0] = 0;
    for (int i = 0; i < 290; i++) {
		char buffer[100];
        snprintf(buffer, sizeof(buffer), "trace 2 value %d %g\r\n", i, i * 3.0);
        stringAppendString(g_buffer, DLOG_RECORD_BUFFER_SIZE, buffer);
    }
    SCPI_ResultCharacters(context, g_buffer, strlen(g_buffer));
}

void dumpTrace3(scpi_t *context) {
	g_buffer[0] = 0;
    for (int i = 0; i < 290; i++) {
		char buffer[100];
        snprintf(buffer, sizeof(buffer), "trace 3 value %d %g\r\n", i, 500 + 500 * sin(i * 2 * 3.14 / 290));
        stringAppendString(g_buffer, DLOG_RECORD_BUFFER_SIZE, buffer);
    }
    SCPI_ResultCharacters(context, g_buffer, strlen(g_buffer));
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
		} else if (cmd == 34) {
			SCPI_ResultUInt32(context, millis() - g_startTime);
			return SCPI_RES_OK;
		} else if (cmd == 35) {
			dumpAlloc(context);
			return SCPI_RES_OK;
		} else if (cmd == 101) {
            dumpFreq(context);
            return SCPI_RES_OK;
        } else if (cmd == 102) {
            dumpTrace1(context);
            return SCPI_RES_OK;
        } else if (cmd == 103) {
            dumpTrace2(context);
            return SCPI_RES_OK;
        } else if (cmd == 104) {
            dumpTrace3(context);
            return SCPI_RES_OK;
        } else if (cmd == 105) {
            int32_t delay;
            SCPI_ParamInt32(context, &delay, false);
            osDelay(delay);
        	char message[128];
        	snprintf(message, sizeof(message), "delayed %d ms", (int)delay);
            SCPI_ResultText(context, message);
            return SCPI_RES_OK;
        } else if (cmd == 106) {
            const char *data = "+2.49815724E+01,+2.49797297E+01,+2.49834151E+01,+2.49917072E+01,+2.49953926E+01,+2.49935499E+01,+2.49788084E+01,+2.49714376E+01,+2.50380860E+01,+2.50362433E+01,+2.49705163E+01,+2.49158454E+01,+2.49871005E+01,+2.50009206E+01,+2.49871005E+01,+2.49843364E+01,+2.49871005E+01,+2.50629622E+01,+2.49723590E+01,+2.50344006E+01,+2.50380860E+01,+2.50380860E+01,+2.49843364E+01,+2.49566962E+01,+2.49502468E+01,+2.50095244E+01,+2.50122884E+01,+2.50095244E+01,+2.49957042E+01,+2.49244492E+01,+2.49318199E+01,+2.50021536E+01,+2.49428760E+01,+2.50058390E+01,+2.49484041E+01,+2.50095244E+01,+2.50168951E+01,+2.50159738E+01,+2.49437974E+01,+2.49502468E+01,+2.50150524E+01,+2.50242658E+01,+2.50297939E+01,+2.50316366E+01,+2.49714376E+01,+2.50417714E+01,+2.50918355E+01,+2.50196591E+01,+2.50215018E+01,+2.50215018E+01,+2.50095244E+01,+2.50021536E+01,+2.49864908E+01,+2.49087863E+01,+2.49717493E+01,+2.49717493E+01,+2.49524011E+01,+2.49413450E+01,+2.49358169E+01,+2.49192328E+01,+2.49238395E+01,+2.49109407E+01,+2.48971205E+01,+2.48906711E+01,+2.49536341E+01,+2.49600835E+01,+2.49072553E+01,+2.48470564E+01,+2.49849598E+01,+2.49858811E+01,+2.49904878E+01,+2.49247608E+01,+2.49950945E+01,+2.49219968E+01,+2.49766677E+01,+2.49118620E+01,+2.49646902E+01,+2.49600835E+01,+2.49508701E+01";
            SCPI_ResultArbitraryBlock(context, data, strlen(data));
            return SCPI_RES_OK;
        } else if (cmd == 107) {
            return SCPI_RES_OK;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
            return SCPI_RES_ERR;
        }
    }

#ifndef __EMSCRIPTEN__
    for (int i = 0; i < CH_NUM; i++) {
        if (!measureAllAdcValuesOnChannel(i)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
    }
#endif

    for (int i = 0; i < CH_NUM; i++) {
        Channel &channel = Channel::get(i);
        channel.dumpDebugVariables(context);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugOntimeQ(scpi_t *context) {
#ifdef DEBUG
    char buffer[512] = { 0 };

    snprintf(buffer, sizeof(buffer), "power active: %d\n", int(ontime::g_mcuCounter.isActive() ? 1 : 0));

    for (int i = 0; i < CH_NUM; ++i) {
        Channel &channel = Channel::get(i);

        auto n = strlen(buffer);
        snprintf(buffer + n, sizeof(buffer) - n, "CH%d active: %d\n", channel.channelIndex + 1, int(ontime::g_mcuCounter.isActive() ? 1 : 0));
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
	snprintf(text, sizeof(text), "TODO");
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

    bool ate = false;
    if (!SCPI_ParamBool(context, &ate, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
    }

    if (bp3c::flash_slave::g_bootloaderMode) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    bp3c::flash_slave::start(slotIndex - 1, hexFilePath, ate);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugDownloadFirmwareQ(scpi_t *context) {
#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    SCPI_ResultBool(context, bp3c::flash_slave::g_bootloaderMode || bp3c::flash_slave::g_ate);
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
