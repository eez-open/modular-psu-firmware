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

#include <eez/system.h>

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/init.h>
#include <eez/modules/psu/serial_psu.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/scpi/psu.h>

#include <eez/modules/bp3c/relays.h>

#pragma warning( push )
#pragma warning( disable : 4200)

extern "C" {
#include "py/compile.h"
#include "py/runtime.h"

void do_str(const char *src, mp_parse_input_kind_t input_kind) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, strlen(src), 0);
        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, input_kind);
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
        mp_call_function_0(module_fun);
        nlr_pop();
    } else {
        // uncaught exception
        mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
    }
}

}

#pragma warning( pop ) 

namespace eez {
namespace psu {

#ifdef DEBUG
using namespace debug;
#endif // DEBUG

namespace scpi {

scpi_result_t scpi_cmd_debug(scpi_t *context) {
    // TODO migrate to generic firmware
#ifdef DEBUG
    scpi_number_t param;
    if (SCPI_ParamNumber(context, 0, &param, false)) {
        delay((uint32_t)round(param.content.value * 1000));
    } else {
        delay(1000);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugQ(scpi_t *context) {
    // TODO migrate to generic firmware
#ifdef DEBUG
    static char buffer[2048];

    for (int i = 0; i < CH_NUM; i++) {
        if (!measureAllAdcValuesOnChannel(i)) {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
    }

    debug::dumpVariables(buffer);

    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugWdog(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_debugWdogQ(scpi_t *context) {
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_debugOntimeQ(scpi_t *context) {
    // TODO migrate to generic firmware
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
    // TODO migrate to generic firmware
#ifdef DEBUG
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    uint32_t value;
    if (!SCPI_ParamUInt32(context, &value, true)) {
        return SCPI_RES_ERR;
    }

    channel->channelInterface->setDacVoltage(channel->subchannelIndex, (uint16_t)value);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugCurrent(scpi_t *context) {
    // TODO migrate to generic firmware
#ifdef DEBUG
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    uint32_t value;
    if (!SCPI_ParamUInt32(context, &value, true)) {
        return SCPI_RES_ERR;
    }

    channel->channelInterface->setDacCurrent(channel->subchannelIndex, (uint16_t)value);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugMeasureVoltage(scpi_t *context) {
    // TODO migrate to generic firmware
#ifdef DEBUG
    if (serial::g_testResult != TEST_OK) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    while (true) {
        uint32_t tickCount = micros();
        temperature::tick(tickCount);

#if OPTION_FAN
        aux_ps::fan::tick(tickCount);
#endif

        channel->channelInterface->adcMeasureUMon(channel->subchannelIndex);

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
    // TODO migrate to generic firmware
#ifdef DEBUG
    if (serial::g_testResult != TEST_OK) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    while (true) {
        uint32_t tickCount = micros();
        temperature::tick(tickCount);

#if OPTION_FAN
        aux_ps::fan::tick(tickCount);
#endif

        channel->channelInterface->adcMeasureIMon(channel->subchannelIndex);

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
#if OPTION_FAN
    // TODO migrate to generic firmware
    int32_t fanSpeed;
    if (!SCPI_ParamInt(context, &fanSpeed, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (fanSpeed < 0) {
        persist_conf::setFanSettings(FAN_MODE_AUTO, 100);
    } else {
        persist_conf::setFanSettings(FAN_MODE_MANUAL, (uint8_t)MIN(fanSpeed, 100));
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_debugFanQ(scpi_t *context) {
#if OPTION_FAN
    // TODO migrate to generic firmware
    SCPI_ResultInt(context, persist_conf::devConf.fanMode == FAN_MODE_MANUAL ? persist_conf::devConf.fanSpeed : -1);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_debugFanPid(scpi_t *context) {
#if OPTION_FAN
    // TODO migrate to generic firmware
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
    // TODO migrate to generic firmware
    double Kp[4] = { aux_ps::fan::g_Kp, aux_ps::fan::g_Ki, aux_ps::fan::g_Kd, aux_ps::fan::g_POn * 1.0f };

    SCPI_ResultArrayDouble(context, Kp, 4, SCPI_FORMAT_ASCII);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_debugCsvQ(scpi_t *context) {
    // TODO migrate to generic firmware
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
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    Channel *channel = &Channel::get(psu_context->selected_channel_index);
    if (!channel->isInstalled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
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

    if (direction != channel->channelInterface->getIoExpBitDirection(channel->subchannelIndex, bit)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    bool state;
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    channel->channelInterface->changeIoExpBit(channel->subchannelIndex, bit, state);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugIoexpQ(scpi_t *context) {
#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    Channel *channel = &Channel::get(psu_context->selected_channel_index);
    if (!channel->isInstalled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
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

    if (direction != channel->channelInterface->getIoExpBitDirection(channel->subchannelIndex, bit)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    bool state = channel->channelInterface->testIoExpBit(channel->subchannelIndex, bit);

    SCPI_ResultBool(context, state);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

scpi_result_t scpi_cmd_debugPythonQ(scpi_t *context) {
#ifdef DEBUG
/*

- Usporediti ovo https://github.com/numworks/epsilon sa onim što mi imamo i sa latest verzijom MP-a
- Pokrenuti one python make skripte i vidjeti što se dobije i kako se to razlikuje od onoga što je već buildano
- Dodati jedan modul
- Probati primiti str parameter i vratiti int ili string
- Delay funkcija
- Python thread
- Vidjeti kako zaglaviti i odglaviti
- scpi funkcija bi trebala poslati scpi komandu u scpi thread kao string i čekati rezultat
- python može zaglaviti u delay i scpi komandi

- dlog specificira X os (vrijeme, U) i Y osi (U, I, P). Za svaku os MIN, MAX, MIN_MEAS, MAX_MEAS. 
  Za x os i STEP ili PERIOD. Ako step nije zadan onda se i vrijednosti i za X osi nalaze u podacima.

MAGIC1
MAGIC2
VERSION
TAG PARAMS
TAG PARAMS
0xFF
DATA

Tag 0x01 X-os unit min max min_meas max_meas
Tag 0x02 start time

Tag 0x10 Y1-os min max min_meas max_meas
Tag 0x11 Y2-os min max min_meas max_meas
Tag 0x12 Y3-os min max min_meas max_meas
Tag 0x1F Y16-os min max min_meas max_meas

Unit: 0x00 time, 0x01 VOLT, 0x02 AMPER, 0x03 WATT

- DLOG view dock legend


U1 DIV OFFSET |--------------------------------|
   CURSOR     |                                |
I1 DIV OFFSET |                                |
   CURSOR     |                                |
U2 DIV OFFSET |                                |
   CURSOR     |                                |
I2 DIV OFFSET |                                |
   CURSOR     |--------------------------------|

Width: 40 + 80 * 2 = 200px
Height: 8 x 30 = 240px

Ostaje za graf: 280px x 240px
7 x 6 divisiona

from eez import scpi, dlog, start, delay

scpi("inst ch1");

U_MAX = 40.0
U_STEP = 0.01
TIME_STEP = 0.01
MEASURE_DELAY = 0.005
I_SET = 0.02

scpi("curr " % I_SET);

i_list = []

t = start()

u_set = 0
while True:
    scpi("volt " % u_set);

    delay(t, MEASURE_DELAY)

    if scpi("outp:mode?") == "CC":
        break

    i_list.add(scpi("curr?"))

    u_set = u_set + U_STEP
    if u_set > U_MAX:
        break

    delay(t, TIME_STEP - MEASURE_DELAY)

FILE_NAME_MAX_CHARS = 20
fileName = scpi('query text, "Diode model", ' % FILE_NAME_MAX_CHARS)
dlog(fileName, TIME_STEP, i_list)

https://docs.micropython.org/en/latest/develop/cmodules.html

*/

    mp_init();
    do_str("print('hello world!', list(x+1 for x in range(10)), end='eol\\n')", MP_PARSE_SINGLE_INPUT);
    do_str("for i in range(10):\n  print(i)", MP_PARSE_FILE_INPUT);
    do_str("print(1)\nprint(2)\nprint(3)", MP_PARSE_FILE_INPUT);
    do_str("from eez import test\nprint(test(1, 2, 3))\n", MP_PARSE_FILE_INPUT);
    mp_deinit();

    SCPI_ResultText(context, "1");

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

scpi_result_t scpi_cmd_debugBoot(scpi_t *context) {
#if defined(DEBUG) && defined(EEZ_PLATFORM_STM32)
    bp3c::relays::toggleBootloader(2);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif // DEBUG
}

} // namespace scpi
} // namespace psu
} // namespace eez
