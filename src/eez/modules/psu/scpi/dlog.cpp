/*
* EEZ PSU Firmware
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

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/scpi/psu.h>
#if OPTION_SD_CARD
#include <eez/modules/psu/dlog_record.h>
#endif

namespace eez {
namespace psu {
namespace scpi {

scpi_result_t scpi_cmd_abortDlog(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    dlog_record::abort();
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_initiateDlog(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    if (!getFilePath(context, filePath, true)) {
        return SCPI_RES_ERR;
    }

    strcpy(dlog_record::g_parameters.filePath, filePath);

    int result = dlog_record::initiate();
    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogFunctionVoltage(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_parameters.logVoltage[channel->channelIndex] = enable;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogFunctionVoltageQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, dlog_record::g_parameters.logVoltage[channel->channelIndex]);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogFunctionCurrent(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_parameters.logCurrent[channel->channelIndex] = enable;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogFunctionCurrentQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, dlog_record::g_parameters.logCurrent[channel->channelIndex]);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogFunctionPower(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_parameters.logPower[channel->channelIndex] = enable;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogFunctionPowerQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, dlog_record::g_parameters.logPower[channel->channelIndex]);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogPeriod(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float period;

    if (param.special) {
        if (param.content.tag == SCPI_NUM_MIN) {
            period = dlog_record::PERIOD_MIN;
        } else if (param.content.tag == SCPI_NUM_MAX) {
            period = dlog_record::PERIOD_MAX;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            period = dlog_record::PERIOD_DEFAULT;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_SECOND) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        period = (float)param.content.value;
    }

    dlog_record::g_parameters.period = period;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogPeriodQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    SCPI_ResultFloat(context, dlog_record::g_parameters.period);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTime(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float time;

    if (param.special) {
        if (param.content.tag == SCPI_NUM_MIN) {
            time = dlog_record::TIME_MIN;
        } else if (param.content.tag == SCPI_NUM_MAX) {
            time = dlog_record::TIME_MAX;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            time = dlog_record::TIME_DEFAULT;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_SECOND) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        time = (float)param.content.value;
    }

    dlog_record::g_parameters.time = time;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTimeQ(scpi_t *context) {
    // TODO migrate to generic firmware
#if OPTION_SD_CARD
    SCPI_ResultFloat(context, dlog_record::g_parameters.time);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_choice_def_t unitChoice[] = {
    { "UNKNown", UNIT_UNKNOWN },
    { "VOLT", UNIT_VOLT },
    { "AMPEr", UNIT_AMPER },
    { "WATT", UNIT_WATT },
    { "JOULe", UNIT_JOULE },
    { "SECOnd", UNIT_SECOND },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_senseDlogTraceXUnit(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t unit;
    if (!SCPI_ParamChoice(context, unitChoice, &unit, true)) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_parameters.xAxis.unit = (Unit)unit;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXUnitQ(scpi_t *context) {
#if OPTION_SD_CARD
    resultChoiceName(context, unitChoice, dlog_record::g_parameters.xAxis.unit);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXStep(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float step;

    if (param.special) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_parameters.xAxis.unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        step = (float)param.content.value;

        if (step <= 0) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return SCPI_RES_ERR;
        }
    }

    dlog_record::g_parameters.xAxis.step = step;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXStepQ(scpi_t *context) {
#if OPTION_SD_CARD
    SCPI_ResultFloat(context, dlog_record::g_parameters.xAxis.step);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_choice_def_t scaleChoice[] = {
    { "LINear", dlog_view::SCALE_LINEAR },
    { "LOGarithmic", dlog_view::SCALE_LOGARITHMIC },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_senseDlogTraceXScale(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t scale;
    if (!SCPI_ParamChoice(context, scaleChoice, &scale, true)) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_parameters.xAxis.scale = (dlog_view::Scale)scale;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXScaleQ(scpi_t *context) {
#if OPTION_SD_CARD
    resultChoiceName(context, scaleChoice, dlog_record::g_parameters.xAxis.scale);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXLabel(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    const char *label;
    size_t len;
    if (!SCPI_ParamCharacters(context, &label, &len, true)) {
        return SCPI_RES_ERR;
    }

    if (len > dlog_view::MAX_LABEL_LENGTH) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    strncpy(dlog_record::g_parameters.xAxis.label, label, len);
    dlog_record::g_parameters.xAxis.label[len] = 0;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXLabelQ(scpi_t *context) {
#if OPTION_SD_CARD
    SCPI_ResultText(context, dlog_record::g_parameters.xAxis.label);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXRangeMin(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float min;

    if (param.special) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_parameters.xAxis.unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        min = (float)param.content.value;
    }

    dlog_record::g_parameters.xAxis.range.min = min;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXRangeMinQ(scpi_t *context) {
#if OPTION_SD_CARD
    SCPI_ResultFloat(context, dlog_record::g_parameters.xAxis.range.min);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXRangeMax(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float max;

    if (param.special) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_parameters.xAxis.unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        max = (float)param.content.value;
    }

    dlog_record::g_parameters.xAxis.range.max = max;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceXRangeMaxQ(scpi_t *context) {
#if OPTION_SD_CARD
    SCPI_ResultFloat(context, dlog_record::g_parameters.xAxis.range.max);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYUnit(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_view::MAX_NUM_OF_Y_AXES) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    int32_t unit;
    if (!SCPI_ParamChoice(context, unitChoice, &unit, true)) {
        return SCPI_RES_ERR;
    }

    if (yAxisIndex >= dlog_record::g_parameters.numYAxes) {
        dlog_record::g_parameters.numYAxes = yAxisIndex + 1;
        dlog_view::initYAxis(dlog_record::g_parameters, yAxisIndex);
    }

    if (yAxisIndex == -1) {
        dlog_record::g_parameters.yAxis.unit = (Unit)unit;
        for (yAxisIndex = 0; yAxisIndex < dlog_record::g_parameters.numYAxes; yAxisIndex++) {
            dlog_record::g_parameters.yAxes[yAxisIndex].unit = (Unit)unit;    
        }
    } else {
        dlog_record::g_parameters.yAxes[yAxisIndex].unit = (Unit)unit;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYUnitQ(scpi_t *context) {
#if OPTION_SD_CARD
    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_record::g_parameters.numYAxes) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex == -1) {
        resultChoiceName(context, unitChoice, dlog_record::g_parameters.yAxis.unit);
    } else {
        resultChoiceName(context, unitChoice, dlog_record::g_parameters.yAxes[yAxisIndex].unit);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYLabel(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_view::MAX_NUM_OF_Y_AXES) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    const char *label;
    size_t len;
    if (!SCPI_ParamCharacters(context, &label, &len, true)) {
        return SCPI_RES_ERR;
    }

    if (len > dlog_view::MAX_LABEL_LENGTH) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex >= dlog_record::g_parameters.numYAxes) {
        dlog_record::g_parameters.numYAxes = yAxisIndex + 1;
        dlog_view::initYAxis(dlog_record::g_parameters, yAxisIndex);
    }

    if (yAxisIndex == -1) {
        strncpy(dlog_record::g_parameters.yAxis.label, label, len);
        dlog_record::g_parameters.yAxis.label[len] = 0;
    } else {
        strncpy(dlog_record::g_parameters.yAxes[yAxisIndex].label, label, len);
        dlog_record::g_parameters.yAxes[yAxisIndex].label[len] = 0;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYLabelQ(scpi_t *context) {
#if OPTION_SD_CARD
    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_record::g_parameters.numYAxes) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex == -1) {
        SCPI_ResultText(context, dlog_record::g_parameters.yAxis.label);
    } else {
        SCPI_ResultText(context, dlog_record::g_parameters.yAxes[yAxisIndex].label);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYRangeMin(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_view::MAX_NUM_OF_Y_AXES) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float min;

    if (param.special) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_parameters.yAxes[yAxisIndex].unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        min = (float)param.content.value;
    }

    if (yAxisIndex >= dlog_record::g_parameters.numYAxes) {
        dlog_record::g_parameters.numYAxes = yAxisIndex + 1;
        dlog_view::initYAxis(dlog_record::g_parameters, yAxisIndex);
    }

    if (yAxisIndex == -1) {
        dlog_record::g_parameters.yAxis.range.min = min;
        for (yAxisIndex = 0; yAxisIndex < dlog_record::g_parameters.numYAxes; yAxisIndex++) {
            dlog_record::g_parameters.yAxes[yAxisIndex].range.min = min;
        }
    } else {
        dlog_record::g_parameters.yAxes[yAxisIndex].range.min = min;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYRangeMinQ(scpi_t *context) {
#if OPTION_SD_CARD
    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_record::g_parameters.numYAxes) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex == -1) {
        SCPI_ResultFloat(context, dlog_record::g_parameters.yAxis.range.min);
    } else {
        SCPI_ResultFloat(context, dlog_record::g_parameters.yAxes[yAxisIndex].range.min);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYRangeMax(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_view::MAX_NUM_OF_Y_AXES) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float max;

    if (param.special) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_parameters.yAxes[yAxisIndex].unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        max = (float)param.content.value;
    }

    if (yAxisIndex >= dlog_record::g_parameters.numYAxes) {
        dlog_record::g_parameters.numYAxes = yAxisIndex + 1;
        dlog_view::initYAxis(dlog_record::g_parameters, yAxisIndex);
    }

    if (yAxisIndex == -1) {
        dlog_record::g_parameters.yAxis.range.max = max;
        for (yAxisIndex = 0; yAxisIndex < dlog_record::g_parameters.numYAxes; yAxisIndex++) {
            dlog_record::g_parameters.yAxes[yAxisIndex].range.max = max;
        }
    } else {
        dlog_record::g_parameters.yAxes[yAxisIndex].range.max = max;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYRangeMaxQ(scpi_t *context) {
#if OPTION_SD_CARD
    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_record::g_parameters.numYAxes) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex == -1) {
        SCPI_ResultFloat(context, dlog_record::g_parameters.yAxis.range.max);
    } else {
        SCPI_ResultFloat(context, dlog_record::g_parameters.yAxes[yAxisIndex].range.max);
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYScale(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t scale;
    if (!SCPI_ParamChoice(context, scaleChoice, &scale, true)) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_parameters.yAxisScale = (dlog_view::Scale)scale;

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceYScaleQ(scpi_t *context) {
#if OPTION_SD_CARD
    resultChoiceName(context, scaleChoice, dlog_record::g_parameters.yAxisScale);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_initiateDlogTrace(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    char filePath[MAX_PATH_LENGTH + 1];
    if (!getFilePath(context, filePath, true)) {
        return SCPI_RES_ERR;
    }

    strcpy(dlog_record::g_parameters.filePath, filePath);

    int result = dlog_record::initiateTrace();
    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_senseDlogTraceData(scpi_t *context) {
#if OPTION_SD_CARD
    if (!dlog_record::isTraceExecuting()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    float values[dlog_view::MAX_NUM_OF_Y_AXES];
    for (int yAxisIndex = 0; yAxisIndex < dlog_record::g_recording.parameters.numYAxes; yAxisIndex++) {
        scpi_number_t param;
        if (!SCPI_ParamNumber(context, 0, &param, true)) {
            break;
        }

        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_recording.parameters.yAxes[yAxisIndex].unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        values[yAxisIndex] = (float)param.content.value;
    }

    dlog_record::log(values);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}


} // namespace scpi
} // namespace psu
} // namespace eez