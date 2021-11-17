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

#include <stdio.h>

#include <bb3/psu/psu.h>

#include <bb3/psu/scpi/psu.h>
#include <bb3/psu/dlog_record.h>

namespace eez {
namespace psu {
namespace scpi {

scpi_result_t scpi_cmd_abortDlog(scpi_t *context) {
    dlog_record::abort();
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_initiateDlog(scpi_t *context) {
    char filePath[MAX_PATH_LENGTH + 1];
    if (!getFilePath(context, filePath, true)) {
        return SCPI_RES_ERR;
    }

    int slotIndex = dlog_record::getModuleLocalRecordingSlotIndex();
    if (slotIndex != -1) {
        if (filePath[0] >= '0' && filePath[0] <= '9' && filePath[1] == ':') {
            int diskDrive = filePath[0] - '0';
            if (diskDrive != slotIndex + 1) {
                SCPI_ErrorPush(context, SCPI_ERROR_FILE_NAME_ERROR);
                return SCPI_RES_ERR;
            }
            stringCopy(dlog_record::g_recordingParameters.filePath, sizeof(dlog_record::g_recordingParameters.filePath), filePath);
        } else {
            // prepend <drive>:
            if (snprintf(dlog_record::g_recordingParameters.filePath, MAX_PATH_LENGTH + 1, "%d:%s", slotIndex + 1, filePath) >= MAX_PATH_LENGTH + 1) {
                SCPI_ErrorPush(context, SCPI_ERROR_CHARACTER_DATA_TOO_LONG);
                return SCPI_RES_ERR;
            }
        }
    } else {
        stringCopy(dlog_record::g_recordingParameters.filePath, sizeof(dlog_record::g_recordingParameters.filePath), filePath);
    }

    int result = dlog_record::initiate();
    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogFunctionVoltage(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    auto err = dlog_record::g_recordingParameters.enableDlogItem(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, DLOG_RESOURCE_TYPE_U, enable);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogFunctionVoltageQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, dlog_record::g_recordingParameters.isDlogItemEnabled(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, DLOG_RESOURCE_TYPE_U));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogFunctionCurrent(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    auto err = dlog_record::g_recordingParameters.enableDlogItem(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, DLOG_RESOURCE_TYPE_I, enable);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogFunctionCurrentQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, dlog_record::g_recordingParameters.isDlogItemEnabled(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, DLOG_RESOURCE_TYPE_I));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogFunctionPower(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    auto err = dlog_record::g_recordingParameters.enableDlogItem(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, DLOG_RESOURCE_TYPE_P, enable);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogFunctionPowerQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, dlog_record::g_recordingParameters.isDlogItemEnabled(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, DLOG_RESOURCE_TYPE_P));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogFunctionDigitalInput(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }
	if (pin < 1) {
		SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
		return SCPI_RES_ERR;
	}
	pin--;

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    auto err = dlog_record::g_recordingParameters.enableDlogItem(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, pin, enable);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogFunctionDigitalInputQ(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }
	if (pin < 1) {
		SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
		return SCPI_RES_ERR;
	}
	pin--;

    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, dlog_record::g_recordingParameters.isDlogItemEnabled(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex, pin));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogPeriod(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float period;

	float min = dlog_view::PERIOD_MIN;
	float max = MIN(dlog_record::g_recordingParameters.duration, dlog_view::PERIOD_MAX);

    if (param.special) {
        if (param.content.tag == SCPI_NUM_MIN) {
            period = min;
        } else if (param.content.tag == SCPI_NUM_MAX) {
            period = max;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            period = dlog_view::PERIOD_DEFAULT;
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

	if (period < min || period > max) {
		SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
		return SCPI_RES_ERR;
	}

    dlog_record::g_recordingParameters.setPeriod(period);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogPeriodQ(scpi_t *context) {
    SCPI_ResultFloat(context, dlog_record::g_recordingParameters.period);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTime(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    float duration;

	float min = MAX(dlog_view::DURATION_MIN, dlog_record::g_recordingParameters.period);
	float max = dlog_view::DURATION_MAX;

    if (param.special) {
        if (param.content.tag == SCPI_NUM_MIN) {
			duration = min;
        } else if (param.content.tag == SCPI_NUM_MAX) {
			duration = max;
        } else if (param.content.tag == SCPI_NUM_DEF) {
			duration = dlog_view::DURATION_DEFAULT;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_SECOND) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

		duration = (float)param.content.value;
    }

	if (duration < min || duration > max) {
		SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
		return SCPI_RES_ERR;
	}

    dlog_record::g_recordingParameters.duration = duration;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTimeQ(scpi_t *context) {
    SCPI_ResultFloat(context, dlog_record::g_recordingParameters.duration);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceRemark(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    const char *comment;
    size_t len;
    if (!SCPI_ParamCharacters(context, &comment, &len, true)) {
        return SCPI_RES_ERR;
    }

    if (len > dlog_file::MAX_COMMENT_LENGTH) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    memcpy(dlog_record::g_recordingParameters.comment, comment, len);
    dlog_record::g_recordingParameters.comment[len] = 0;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceRemarkQ(scpi_t *context) {
    SCPI_ResultText(context, dlog_record::g_recordingParameters.comment);
    return SCPI_RES_OK;
}


scpi_choice_def_t unitChoice[] = {
    { "UNKNown", UNIT_UNKNOWN },
    { "VOLT", UNIT_VOLT },
    { "AMPEr", UNIT_AMPER },
    { "WATT", UNIT_WATT },
    { "JOULe", UNIT_JOULE },
    { "SECOnd", UNIT_SECOND },
    { "OHM", UNIT_OHM },
    { "FARAd", UNIT_FARAD },
    { "HERTz", UNIT_HERTZ },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_senseDlogTraceXUnit(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t unit;
    if (!SCPI_ParamChoice(context, unitChoice, &unit, true)) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_recordingParameters.xAxis.unit = (Unit)unit;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXUnitQ(scpi_t *context) {
    resultChoiceName(context, unitChoice, dlog_record::g_recordingParameters.xAxis.unit);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXStep(scpi_t *context) {
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
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_recordingParameters.xAxis.unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        step = (float)param.content.value;

        if (step <= 0) {
            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
            return SCPI_RES_ERR;
        }
    }

    dlog_record::g_recordingParameters.xAxis.step = step;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXStepQ(scpi_t *context) {
    SCPI_ResultFloat(context, dlog_record::g_recordingParameters.xAxis.step);
    return SCPI_RES_OK;
}

scpi_choice_def_t scaleChoice[] = {
    { "LINear", dlog_file::SCALE_LINEAR },
    { "LOGarithmic", dlog_file::SCALE_LOGARITHMIC },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_senseDlogTraceXScale(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t scaleType;
    if (!SCPI_ParamChoice(context, scaleChoice, &scaleType, true)) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_recordingParameters.xAxis.scaleType = (dlog_file::ScaleType)scaleType;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXScaleQ(scpi_t *context) {
    resultChoiceName(context, scaleChoice, dlog_record::g_recordingParameters.xAxis.scaleType);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXLabel(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    const char *label;
    size_t len;
    if (!SCPI_ParamCharacters(context, &label, &len, true)) {
        return SCPI_RES_ERR;
    }

    if (len > dlog_file::MAX_LABEL_LENGTH) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    memcpy(dlog_record::g_recordingParameters.xAxis.label, label, len);
    dlog_record::g_recordingParameters.xAxis.label[len] = 0;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXLabelQ(scpi_t *context) {
    SCPI_ResultText(context, dlog_record::g_recordingParameters.xAxis.label);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXRangeMin(scpi_t *context) {
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
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_recordingParameters.xAxis.unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        min = (float)param.content.value;
    }

    dlog_record::g_recordingParameters.xAxis.range.min = min;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXRangeMinQ(scpi_t *context) {
    SCPI_ResultFloat(context, dlog_record::g_recordingParameters.xAxis.range.min);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXRangeMax(scpi_t *context) {
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
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_recordingParameters.xAxis.unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        max = (float)param.content.value;
    }

    dlog_record::g_recordingParameters.xAxis.range.max = max;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceXRangeMaxQ(scpi_t *context) {
    SCPI_ResultFloat(context, dlog_record::g_recordingParameters.xAxis.range.max);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYUnit(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_file::MAX_NUM_OF_Y_AXES) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    int32_t unit;
    if (!SCPI_ParamChoice(context, unitChoice, &unit, true)) {
        return SCPI_RES_ERR;
    }

    if (yAxisIndex >= dlog_record::g_recordingParameters.numYAxes) {
        dlog_record::g_recordingParameters.numYAxes = yAxisIndex + 1;
        dlog_record::g_recordingParameters.initYAxis(yAxisIndex);
    }

    if (yAxisIndex == -1) {
        dlog_record::g_recordingParameters.yAxis.unit = (Unit)unit;
        for (yAxisIndex = 0; yAxisIndex < dlog_record::g_recordingParameters.numYAxes; yAxisIndex++) {
            dlog_record::g_recordingParameters.yAxes[yAxisIndex].unit = (Unit)unit;    
        }
    } else {
        dlog_record::g_recordingParameters.yAxes[yAxisIndex].unit = (Unit)unit;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYUnitQ(scpi_t *context) {
    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_record::g_recordingParameters.numYAxes) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex == -1) {
        resultChoiceName(context, unitChoice, dlog_record::g_recordingParameters.yAxis.unit);
    } else {
        resultChoiceName(context, unitChoice, dlog_record::g_recordingParameters.yAxes[yAxisIndex].unit);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYLabel(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_file::MAX_NUM_OF_Y_AXES) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    const char *label;
    size_t len;
    if (!SCPI_ParamCharacters(context, &label, &len, true)) {
        return SCPI_RES_ERR;
    }

    if (len > dlog_file::MAX_LABEL_LENGTH) {
        SCPI_ErrorPush(context, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex >= dlog_record::g_recordingParameters.numYAxes) {
        dlog_record::g_recordingParameters.numYAxes = yAxisIndex + 1;
        dlog_record::g_recordingParameters.initYAxis(yAxisIndex);
    }

    if (yAxisIndex == -1) {
        memcpy(dlog_record::g_recordingParameters.yAxis.label, label, len);
        dlog_record::g_recordingParameters.yAxis.label[len] = 0;
    } else {
        memcpy(dlog_record::g_recordingParameters.yAxes[yAxisIndex].label, label, len);
        dlog_record::g_recordingParameters.yAxes[yAxisIndex].label[len] = 0;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYLabelQ(scpi_t *context) {
    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_record::g_recordingParameters.numYAxes) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex == -1) {
        SCPI_ResultText(context, dlog_record::g_recordingParameters.yAxis.label);
    } else {
        SCPI_ResultText(context, dlog_record::g_recordingParameters.yAxes[yAxisIndex].label);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYRangeMin(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_file::MAX_NUM_OF_Y_AXES) {
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
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_recordingParameters.yAxes[yAxisIndex].unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        min = (float)param.content.value;
    }

    if (yAxisIndex >= dlog_record::g_recordingParameters.numYAxes) {
        dlog_record::g_recordingParameters.numYAxes = yAxisIndex + 1;
        dlog_record::g_recordingParameters.initYAxis(yAxisIndex);
    }

    if (yAxisIndex == -1) {
        dlog_record::g_recordingParameters.yAxis.range.min = min;
        for (yAxisIndex = 0; yAxisIndex < dlog_record::g_recordingParameters.numYAxes; yAxisIndex++) {
            dlog_record::g_recordingParameters.yAxes[yAxisIndex].range.min = min;
        }
    } else {
        dlog_record::g_recordingParameters.yAxes[yAxisIndex].range.min = min;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYRangeMinQ(scpi_t *context) {
    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_record::g_recordingParameters.numYAxes) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex == -1) {
        SCPI_ResultFloat(context, dlog_record::g_recordingParameters.yAxis.range.min);
    } else {
        SCPI_ResultFloat(context, dlog_record::g_recordingParameters.yAxes[yAxisIndex].range.min);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYRangeMax(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_file::MAX_NUM_OF_Y_AXES) {
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
        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_recordingParameters.yAxes[yAxisIndex].unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        max = (float)param.content.value;
    }

    if (yAxisIndex >= dlog_record::g_recordingParameters.numYAxes) {
        dlog_record::g_recordingParameters.numYAxes = yAxisIndex + 1;
        dlog_record::g_recordingParameters.initYAxis(yAxisIndex);
    }

    if (yAxisIndex == -1) {
        dlog_record::g_recordingParameters.yAxis.range.max = max;
        for (yAxisIndex = 0; yAxisIndex < dlog_record::g_recordingParameters.numYAxes; yAxisIndex++) {
            dlog_record::g_recordingParameters.yAxes[yAxisIndex].range.max = max;
        }
    } else {
        dlog_record::g_recordingParameters.yAxes[yAxisIndex].range.max = max;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYRangeMaxQ(scpi_t *context) {
    int32_t yAxisIndex = 0;
    SCPI_CommandNumbers(context, &yAxisIndex, 1, 0);
    yAxisIndex--;

    if (yAxisIndex < -1 || yAxisIndex >= dlog_record::g_recordingParameters.numYAxes) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    if (yAxisIndex == -1) {
        SCPI_ResultFloat(context, dlog_record::g_recordingParameters.yAxis.range.max);
    } else {
        SCPI_ResultFloat(context, dlog_record::g_recordingParameters.yAxes[yAxisIndex].range.max);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYScale(scpi_t *context) {
    if (!dlog_record::isIdle()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_CHANGE_TRANSIENT_TRIGGER);
        return SCPI_RES_ERR;
    }

    int32_t scaleType;
    if (!SCPI_ParamChoice(context, scaleChoice, &scaleType, true)) {
        return SCPI_RES_ERR;
    }

    dlog_record::g_recordingParameters.yAxisScaleType = (dlog_file::ScaleType)scaleType;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceYScaleQ(scpi_t *context) {
    resultChoiceName(context, scaleChoice, dlog_record::g_recordingParameters.yAxisScaleType);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_initiateDlogTrace(scpi_t *context) {
    char filePath[MAX_PATH_LENGTH + 1];
    if (!getFilePath(context, filePath, true)) {
        return SCPI_RES_ERR;
    }

    stringCopy(dlog_record::g_recordingParameters.filePath, sizeof(dlog_record::g_recordingParameters.filePath), filePath);

    int result = dlog_record::initiateTrace();
    if (result != SCPI_RES_OK) {
        SCPI_ErrorPush(context, result);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceData(scpi_t *context) {
    if (!dlog_record::isTraceExecuting()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    float values[dlog_file::MAX_NUM_OF_Y_AXES];
    for (int yAxisIndex = 0; yAxisIndex < dlog_record::g_activeRecording.parameters.numYAxes; yAxisIndex++) {
        scpi_number_t param;
        if (!SCPI_ParamNumber(context, 0, &param, true)) {
            break;
        }

        if (param.unit != SCPI_UNIT_NONE && param.unit != getScpiUnit(dlog_record::g_activeRecording.parameters.yAxes[yAxisIndex].unit)) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        values[yAxisIndex] = (float)param.content.value;
    }

    dlog_record::log(values);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogClear(scpi_t *context) {
    dlog_record::reset();

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_senseDlogTraceBookmark(scpi_t *context) {
    if (!dlog_record::isExecuting()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *text;
    size_t textLen;
    if (!SCPI_ParamCharacters(context, &text, &textLen, true)) {
        return SCPI_RES_ERR;
    }

    if (textLen > dlog_file::MAX_BOOKMARK_TEXT_LEN) {
        SCPI_ErrorPush(context, SCPI_ERROR_CHARACTER_DATA_TOO_LONG);
        return SCPI_RES_ERR;
    }

    dlog_record::logBookmark(text, textLen);

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
