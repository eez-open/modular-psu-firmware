/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#include <string.h>
#include <stdlib.h>

#include <eez/mp.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/trigger.h>
#include <eez/modules/psu/dlog_record.h>

#include <scpi/scpi.h>

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4200)
#endif

extern "C" {
#include "modeez.h"
#include <py/objtuple.h>
#include <py/runtime.h>
}

#ifdef _MSC_VER
#pragma warning( pop ) 
#endif

using namespace eez::mp;
using namespace eez::psu;

mp_obj_t modeez_scpi(mp_obj_t commandOrQueryText) {
    const char *resultText;
    size_t resultTextLen;
    if (!scpi(mp_obj_str_get_str(commandOrQueryText), &resultText, &resultTextLen)) {
        return mp_const_false;
    }

    if (resultTextLen == 0) {
        return mp_const_none;
    }

    if (resultTextLen >= 2 && resultText[0] == '"' && resultText[resultTextLen-1] == '"') {
        return mp_obj_new_str(resultText + 1, resultTextLen - 2);    
    }

    char *strEnd;
    long num = strtol(resultText, &strEnd, 10);
    if (*strEnd == 0) {
        return mp_obj_new_int(num);
    }

    return mp_obj_new_str(resultText, resultTextLen);
}

mp_obj_t modeez_getU(mp_obj_t channelIndexObj) {
    int channelIndex = mp_obj_get_int(channelIndexObj) - 1;
    if (channelIndex < 0 || channelIndex >= CH_NUM) {
        mp_raise_ValueError("Invalid channel index");
    }
    Channel &channel = Channel::get(channelIndex);

    return mp_obj_new_float(eez::psu::channel_dispatcher::getUMonLast(channel));
}

mp_obj_t modeez_setU(mp_obj_t channelIndexObj, mp_obj_t value) {
    int channelIndex = mp_obj_get_int(channelIndexObj) - 1;
    if (channelIndex < 0 || channelIndex >= CH_NUM) {
        mp_raise_ValueError("Invalid channel index");
    }
    Channel &channel = Channel::get(channelIndex);

    if (channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED && !trigger::isIdle()) {
        mp_raise_ValueError("Can not change transient trigger");
    }

    if (channel.isRemoteProgrammingEnabled()) {
        mp_raise_ValueError("Remote programming enabled");
    }

    float voltage = (float)mp_obj_get_float(value);

    if (voltage > channel_dispatcher::getULimit(channel)) {
        mp_raise_ValueError("Voltage limit exceeded");
    }

    int err;
    if (channel.isPowerLimitExceeded(voltage, channel_dispatcher::getISet(channel), &err)) {
        mp_raise_ValueError(SCPI_ErrorTranslate(err));
    }

    channel_dispatcher::setVoltage(channel, voltage);

    return mp_const_none;
}

mp_obj_t modeez_getI(mp_obj_t channelIndexObj) {
    int channelIndex = mp_obj_get_int(channelIndexObj) - 1;
    if (channelIndex < 0 || channelIndex >= CH_NUM) {
        mp_raise_ValueError("Invalid channel index");
    }
    Channel &channel = Channel::get(channelIndex);

    return mp_obj_new_float(eez::psu::channel_dispatcher::getIMonLast(channel));
}

mp_obj_t modeez_setI(mp_obj_t channelIndexObj, mp_obj_t value) {
    int channelIndex = mp_obj_get_int(channelIndexObj) - 1;
    if (channelIndex < 0 || channelIndex >= CH_NUM) {
        mp_raise_ValueError("Invalid channel index");
    }
    Channel &channel = Channel::get(channelIndex);

    if (channel_dispatcher::getVoltageTriggerMode(channel) != TRIGGER_MODE_FIXED && !trigger::isIdle()) {
        mp_raise_ValueError("Can not change transient trigger");
    }

    float current = (float)mp_obj_get_float(value);

    if (current > channel_dispatcher::getILimit(channel)) {
        mp_raise_ValueError("Current limit exceeded");
    }

    int err;
    if (channel.isPowerLimitExceeded(channel_dispatcher::getUSet(channel), current, &err)) {
        mp_raise_ValueError(SCPI_ErrorTranslate(err));
    }

    channel_dispatcher::setCurrent(channel, current);

    return mp_const_none;
}

mp_obj_t modeez_getOutputMode(mp_obj_t channelIndexObj) {
    int channelIndex = mp_obj_get_int(channelIndexObj) - 1;
    if (channelIndex < 0 || channelIndex >= CH_NUM) {
        mp_raise_ValueError("Invalid channel index");
    }
    Channel &channel = Channel::get(channelIndex);

    const char *modeStr = channel.getModeStr();

    return mp_obj_new_str(modeStr, strlen(modeStr));
}

mp_obj_t modeez_dlogTraceData(size_t n_args, const mp_obj_t *args) {
    if (!dlog_record::isTraceExecuting()) {
        mp_raise_ValueError("DLOG trace data not started");
    }

    if (n_args == 1 && mp_obj_is_type(args[0], &mp_type_list)) {
        mp_obj_get_array(args[0], &n_args, (mp_obj_t **)&args);
    }

    if (n_args < dlog_record::g_recording.parameters.numYAxes) {
        mp_raise_ValueError("Too few values");
    }

    if (n_args > dlog_record::g_recording.parameters.numYAxes) {
        mp_raise_ValueError("Too many values");
    }

    float values[eez::dlog_file::MAX_NUM_OF_Y_AXES];

    for (size_t i = 0; i < MIN(n_args, eez::dlog_file::MAX_NUM_OF_Y_AXES); i++) {
        if (!mp_obj_is_type(args[i], &mp_type_float)) {
            mp_raise_ValueError("Argument is not float");
        }
        values[i] = (float)mp_obj_get_float(args[i]);
    }

    dlog_record::log(values);

    return mp_const_none;
}
