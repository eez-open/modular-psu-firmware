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

#include <stdio.h>

#include <bb3/system.h>
#include <bb3/index.h>

#include <bb3/psu/psu.h>
#include <bb3/psu/calibration.h>
#include <bb3/psu/datetime.h>
#include <bb3/psu/devices.h>
#include <bb3/psu/scpi/psu.h>
#include <bb3/psu/temperature.h>

#if OPTION_FAN
#include <bb3/aux_ps/fan.h>
#endif

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

static void printCalibrationValue(scpi_t *context, calibration::CalibrationBase &calibrationBase, calibration::Value &value) {
    const char *prefix;
    if (value.type == CALIBRATION_VALUE_U) {
        prefix = "u";
    } else {
        if (calibrationBase.hasSupportForCurrentDualRange()) {
            if (value.type == CALIBRATION_VALUE_I_HI_RANGE) {
                prefix = "i_5A";
            } else {
                prefix = "i_50mA";
            }
        } else {
            prefix = "i";
        }
    }

    char buffer[128] = { 0 };

    for (unsigned int i = 0; i < value.configuration.numPoints; i++) {
        if (value.isPointSet[i]) {
            snprintf(buffer, sizeof(buffer), "%s_point%d_dac=%f", prefix, i + 1, value.configuration.points[i].dac);
            SCPI_ResultText(context, buffer);

            snprintf(buffer, sizeof(buffer), "%s_point%d_data=%f", prefix, i + 1, value.configuration.points[i].value);
            SCPI_ResultText(context, buffer);

            snprintf(buffer, sizeof(buffer), "%s_point%d_adc=%f", prefix, i + 1, value.configuration.points[i].adc);
            SCPI_ResultText(context, buffer);
        }
    }
}

void printCalibrationParameters(scpi_t *context, Unit unit, uint8_t currentRange, bool calParamsExists, CalibrationValueConfiguration &calibrationValue) {
    const char *prefix;
    if (unit == UNIT_VOLT) {
        prefix = "u";
    } else {
        if (currentRange == 0) {
            prefix = "i_5A";
        } else if (currentRange == 1) {
            prefix = "i_50mA";
        } else {
            prefix = "i";
        }
    }

    char buffer[128] = { 0 };

    snprintf(buffer, sizeof(buffer), "%s_cal_params_exists=%d", prefix, calParamsExists);
    SCPI_ResultText(context, buffer);

    if (calParamsExists) {
        for (unsigned int i = 0; i < calibrationValue.numPoints; i++) {
            snprintf(buffer, sizeof(buffer), "%s_point%d_dac=%f", prefix, i + 1, calibrationValue.points[i].dac);
            SCPI_ResultText(context, buffer);

            snprintf(buffer, sizeof(buffer), "%s_point%d_data=%f", prefix, i + 1, calibrationValue.points[i].value);
            SCPI_ResultText(context, buffer);

            if (!isNaN(calibrationValue.points[i].adc)) {
                snprintf(buffer, sizeof(buffer), "%s_point%d_adc=%f", prefix, i + 1, calibrationValue.points[i].adc);
                SCPI_ResultText(context, buffer);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_diagnosticInformationAdcQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (!measureAllAdcValuesOnChannel(channel->channelIndex)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char buffer[64] = { 0 };

    stringCopy(buffer, sizeof(buffer), "U_SET=");
    stringAppendVoltage(buffer, sizeof(buffer), channel->u.mon_dac_last);
    SCPI_ResultText(context, buffer);

    stringCopy(buffer, sizeof(buffer), "U_MON=");
    stringAppendVoltage(buffer, sizeof(buffer), channel->u.mon_adc);
    SCPI_ResultText(context, buffer);

    stringCopy(buffer, sizeof(buffer), "I_SET=");
    stringAppendCurrent(buffer, sizeof(buffer), channel->i.mon_dac_last);
    SCPI_ResultText(context, buffer);

    stringCopy(buffer, sizeof(buffer), "I_MON=");
    stringAppendCurrent(buffer, sizeof(buffer), channel->i.mon_adc);
    SCPI_ResultText(context, buffer);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_diagnosticInformationCalibrationQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    Channel *channel = Channel::getBySlotIndex(slotAndSubchannelIndex.slotIndex, slotAndSubchannelIndex.subchannelIndex);

    if (calibration::isChannelCalibrating(*channel)) {
        if (calibration::g_editor.isRemarkSet()) {
            char buffer[128] = { 0 };
            snprintf(buffer, sizeof(buffer), "remark=%s", calibration::g_editor.getRemark());
            SCPI_ResultText(context, buffer);
        }
        printCalibrationValue(context, calibration::g_editor, calibration::g_editor.getVoltage());
        printCalibrationValue(context, calibration::g_editor, calibration::g_editor.getCurrent());
    } else {
        char buffer[128] = { 0 };

        if (channel) {
            int year, month, day, hour, minute, second;
            datetime::breakTime(channel->cal_conf.calibrationDate, year, month, day, hour, minute, second);

            snprintf(buffer, sizeof(buffer), "remark=%04d-%02d-%02d %s", year, month, day, channel->cal_conf.calibrationRemark);
            SCPI_ResultText(context, buffer);

            printCalibrationParameters(context, UNIT_VOLT, -1, channel->isVoltageCalibrationExists(), channel->cal_conf.u);
            if (channel->hasSupportForCurrentDualRange()) {
                printCalibrationParameters(context, UNIT_AMPER, 0, channel->isCurrentCalibrationExists(0), channel->cal_conf.i[0]);
                printCalibrationParameters(context, UNIT_AMPER, 1, channel->isCurrentCalibrationExists(1), channel->cal_conf.i[1]);
            } else {
                printCalibrationParameters(context, UNIT_AMPER, -1, channel->isCurrentCalibrationExists(0), channel->cal_conf.i[0]);
            }
        } else {
            CalibrationConfiguration calConf;
            int err;
            if (!g_slots[slotAndSubchannelIndex.slotIndex]->getCalibrationConfiguration(slotAndSubchannelIndex.subchannelIndex, calConf, &err)) {
                SCPI_ErrorPush(context, err);
                return SCPI_RES_ERR;
            }

            int year, month, day, hour, minute, second;
            datetime::breakTime(calConf.calibrationDate, year, month, day, hour, minute, second);

            snprintf(buffer, sizeof(buffer), "remark=%04d-%02d-%02d %s", year, month, day, calConf.calibrationRemark);
            SCPI_ResultText(context, buffer);

            g_slots[slotAndSubchannelIndex.slotIndex]->isVoltageCalibrationExists(slotAndSubchannelIndex.subchannelIndex);

            printCalibrationParameters(context, UNIT_VOLT, -1, g_slots[slotAndSubchannelIndex.slotIndex]->isVoltageCalibrationExists(slotAndSubchannelIndex.subchannelIndex), calConf.u);
            printCalibrationParameters(context, UNIT_AMPER, -1, g_slots[slotAndSubchannelIndex.slotIndex]->isCurrentCalibrationExists(slotAndSubchannelIndex.subchannelIndex), calConf.i[0]);
        }
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_diagnosticInformationProtectionQ(scpi_t *context) {
    char buffer[256] = { 0 };

    for (int i = 0; i < CH_NUM; ++i) {
        Channel *channel = &Channel::get(i);

        int channelIndex = channel->channelIndex + 1;

        // voltage
        snprintf(buffer, sizeof(buffer), "CH%d u_tripped=%d", channelIndex, (int)channel->ovp.flags.tripped);
        SCPI_ResultText(context, buffer);
        snprintf(buffer, sizeof(buffer), "CH%d u_state=%d", channelIndex, (int)channel->prot_conf.flags.u_state);
        SCPI_ResultText(context, buffer);
        snprintf(buffer, sizeof(buffer), "CH%d u_type=%d", channelIndex, (int)channel->prot_conf.flags.u_type);
        SCPI_ResultText(context, buffer);
        snprintf(buffer, sizeof(buffer), "CH%d u_delay=", channelIndex);
        stringAppendDuration(buffer, sizeof(buffer), channel->prot_conf.u_delay);
        SCPI_ResultText(context, buffer);

        snprintf(buffer, sizeof(buffer), "CH%d u_level=", channelIndex);
        stringAppendVoltage(buffer, sizeof(buffer), channel->prot_conf.u_level);
        SCPI_ResultText(context, buffer);

        // current
        snprintf(buffer, sizeof(buffer), "CH%d i_tripped=%d", channelIndex, (int)channel->ocp.flags.tripped);
        SCPI_ResultText(context, buffer);
        snprintf(buffer, sizeof(buffer), "CH%d i_state=%d", channelIndex, (int)channel->prot_conf.flags.i_state);
        SCPI_ResultText(context, buffer);
        snprintf(buffer, sizeof(buffer), "CH%d i_delay=", channelIndex);
        stringAppendDuration(buffer, sizeof(buffer), channel->prot_conf.i_delay);
        SCPI_ResultText(context, buffer);

        // power
        snprintf(buffer, sizeof(buffer), "CH%d p_tripped=%d", channelIndex, (int)channel->opp.flags.tripped);
        SCPI_ResultText(context, buffer);
        snprintf(buffer, sizeof(buffer), "CH%d p_state=%d", channelIndex, (int)channel->prot_conf.flags.p_state);
        SCPI_ResultText(context, buffer);
        snprintf(buffer, sizeof(buffer), "CH%d p_delay=", channelIndex);
        stringAppendDuration(buffer, sizeof(buffer), channel->prot_conf.p_delay);
        SCPI_ResultText(context, buffer);

        snprintf(buffer, sizeof(buffer), "CH%d p_level=", channelIndex);
        stringAppendPower(buffer, sizeof(buffer), channel->prot_conf.p_level);
        SCPI_ResultText(context, buffer);
    }

    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        temp_sensor::TempSensor &sensor = temp_sensor::sensors[i];
        if (sensor.isInstalled()) {
            temperature::TempSensorTemperature &sensorTemperature = temperature::sensors[i];

            snprintf(buffer, sizeof(buffer), "temp_%s_tripped=%d", sensor.name, (int)sensorTemperature.isTripped());
            SCPI_ResultText(context, buffer);

            snprintf(buffer, sizeof(buffer), "temp_%s_state=%d", sensor.name, (int)sensorTemperature.prot_conf.state);
            SCPI_ResultText(context, buffer);

            snprintf(buffer, sizeof(buffer), "temp_%s_delay=", sensor.name);
            stringAppendDuration(buffer, sizeof(buffer), sensorTemperature.prot_conf.delay);
            SCPI_ResultText(context, buffer);

            snprintf(buffer, sizeof(buffer), "temp_%s_level=", sensor.name);
            stringAppendFloat(buffer, sizeof(buffer), sensorTemperature.prot_conf.level);
            stringAppendString(buffer, sizeof(buffer), " oC");
            SCPI_ResultText(context, buffer);
        }
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_diagnosticInformationTestQ(scpi_t *context) {
    int32_t deviceId = -1;
    if (!SCPI_ParamChoice(context, devices::g_deviceChoice, &deviceId, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
    }

    if (deviceId == -1) {
        char buffer[128] = { 0 };
        devices::Device device;
        for (int i = 0; devices::getDevice(i, device); ++i) {
            snprintf(buffer, sizeof(buffer), "%d, %s, %s, %s",
                device.testResult ? (int)device.testResult : TEST_SKIPPED, device.name,
                devices::getInstalledString(device.installed),
                devices::getTestResultString(device.testResult));
            SCPI_ResultText(context, buffer);
        }
    } else {
        devices::Device device;
        for (int i = 0; devices::getDevice(i, device); ++i) {
            if (device.id == (devices::DeviceId)deviceId) {
                SCPI_ResultInt(context, device.testResult);
                return SCPI_RES_OK;
            }
        }
        SCPI_ResultInt(context, TEST_NONE);
    }

    return SCPI_RES_OK;
}

static uint8_t g_ioexpRegisters[CH_MAX][32];
static uint8_t g_adcRegisters[CH_MAX][4];

void diagCallback() {
    for (int i = 0; i < CH_NUM; i++) {
        Channel& channel = Channel::get(i);
        channel.readAllRegisters(&g_ioexpRegisters[i][0], &g_adcRegisters[i][0]);
    }
}

scpi_result_t scpi_cmd_diagnosticInformationRegsQ(scpi_t *context) {
    g_diagCallback = diagCallback;
    while (g_diagCallback) {
    	osDelay(1);
    }

    char buffer[2048];

    buffer[0] = 0;

    for (int i = 0; i < CH_NUM; i++) {
        Channel& channel = Channel::get(i);
        if (g_slots[channel.slotIndex]->moduleType == MODULE_TYPE_DCP405) {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "CH%d:\n", i + 1);

            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\tIOEXP:\n");
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tIODIRA   %X\n", (int)g_ioexpRegisters[i][0]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tIODIRB   %X\n", (int)g_ioexpRegisters[i][1]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tIPOLA    %X\n", (int)g_ioexpRegisters[i][2]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tIPOLB    %X\n", (int)g_ioexpRegisters[i][3]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tGPINTENA %X\n", (int)g_ioexpRegisters[i][4]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tGPINTENB %X\n", (int)g_ioexpRegisters[i][5]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tDEFVALA  %X\n", (int)g_ioexpRegisters[i][6]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tDEFVALB  %X\n", (int)g_ioexpRegisters[i][7]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tINTCONA  %X\n", (int)g_ioexpRegisters[i][8]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tINTCONB  %X\n", (int)g_ioexpRegisters[i][9]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tIOCON    %X\n", (int)g_ioexpRegisters[i][10]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tIOCON    %X\n", (int)g_ioexpRegisters[i][11]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tGPPUA    %X\n", (int)g_ioexpRegisters[i][12]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tGPPUB    %X\n", (int)g_ioexpRegisters[i][13]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tINTFA    %X\n", (int)g_ioexpRegisters[i][14]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tINTFB    %X\n", (int)g_ioexpRegisters[i][15]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tINTCAPA  %X\n", (int)g_ioexpRegisters[i][16]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tINTCAPB  %X\n", (int)g_ioexpRegisters[i][17]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tGPIOA    %X\n", (int)g_ioexpRegisters[i][18]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tGPIOB    %X\n", (int)g_ioexpRegisters[i][19]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tOLATA    %X\n", (int)g_ioexpRegisters[i][20]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tOLATB    %X\n", (int)g_ioexpRegisters[i][21]);

            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\tADC:\n");
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tREG0     %X\n", (int)g_adcRegisters[i][0]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tREG1     %X\n", (int)g_adcRegisters[i][1]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tREG2     %X\n", (int)g_adcRegisters[i][2]);
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "\t\tREG3     %X\n", (int)g_adcRegisters[i][3]);
        }
    }

    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
