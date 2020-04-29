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

#include <eez/system.h>
#include <eez/index.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/calibration.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/devices.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/temperature.h>

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

static void printCalibrationValue(scpi_t *context, calibration::Value &value) {
    const char *prefix;
    if (value.type == calibration::CALIBRATION_VALUE_U) {
        prefix = "u";
    } else {
        if (calibration::getCalibrationChannel().hasSupportForCurrentDualRange()) {
            if (value.type == calibration::CALIBRATION_VALUE_I_HI_RANGE) {
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
            sprintf(buffer, "%s_point%d_dac=%f", prefix, i + 1, value.configuration.points[i].dac);
            SCPI_ResultText(context, buffer);

            sprintf(buffer, "%s_point%d_data=%f", prefix, i + 1, value.configuration.points[i].value);
            SCPI_ResultText(context, buffer);

            sprintf(buffer, "%s_point%d_adc=%f", prefix, i + 1, value.configuration.points[i].adc);
            SCPI_ResultText(context, buffer);
        }
    }
}

void printCalibrationParameters(scpi_t *context, Unit unit, uint8_t currentRange, bool calParamsExists, Channel::CalibrationValueConfiguration &calibrationValue) {
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

    sprintf(buffer, "%s_cal_params_exists=%d", prefix, calParamsExists);
    SCPI_ResultText(context, buffer);

    if (calParamsExists) {
        for (unsigned int i = 0; i < calibrationValue.numPoints; i++) {
            sprintf(buffer, "%s_point%d_dac=%f", prefix, i + 1, calibrationValue.points[i].dac);
            SCPI_ResultText(context, buffer);

            sprintf(buffer, "%s_point%d_data=%f", prefix, i + 1, calibrationValue.points[i].value);
            SCPI_ResultText(context, buffer);

            sprintf(buffer, "%s_point%d_adc=%f", prefix, i + 1, calibrationValue.points[i].adc);
            SCPI_ResultText(context, buffer);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_diagnosticInformationAdcQ(scpi_t *context) {
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (!measureAllAdcValuesOnChannel(channel->channelIndex)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char buffer[64] = { 0 };

    strcpy(buffer, "U_SET=");
    strcatVoltage(buffer, channel->u.mon_dac_last);
    SCPI_ResultText(context, buffer);

    strcpy(buffer, "U_MON=");
    strcatVoltage(buffer, channel->u.mon_last);
    SCPI_ResultText(context, buffer);

    strcpy(buffer, "I_SET=");
    strcatCurrent(buffer, channel->i.mon_dac_last);
    SCPI_ResultText(context, buffer);

    strcpy(buffer, "I_MON=");
    strcatCurrent(buffer, channel->i.mon_last);
    SCPI_ResultText(context, buffer);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_diagnosticInformationCalibrationQ(scpi_t *context) {
    Channel *channel = param_channel(context);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    if (calibration::isEnabled()) {
        if (calibration::isRemarkSet()) {
            char buffer[128] = { 0 };
            sprintf(buffer, "remark=%s", calibration::getRemark());
            SCPI_ResultText(context, buffer);
        }
        printCalibrationValue(context, calibration::getVoltage());
        printCalibrationValue(context, calibration::getCurrent());
    } else {
        char buffer[128] = { 0 };

        int year, month, day, hour, minute, second;
        datetime::breakTime(channel->cal_conf.calibrationDate, year, month, day, hour, minute, second);

        sprintf(buffer, "remark=%04d-%02d-%02d %s", year, month, day, channel->cal_conf.calibrationRemark);
        SCPI_ResultText(context, buffer);

        printCalibrationParameters(context, UNIT_VOLT, -1, channel->isVoltageCalibrationExists(), channel->cal_conf.u);
        if (channel->hasSupportForCurrentDualRange()) {
            printCalibrationParameters(context, UNIT_AMPER, 0, channel->isCurrentCalibrationExists(0), channel->cal_conf.i[0]);
            printCalibrationParameters(context, UNIT_AMPER, 1, channel->isCurrentCalibrationExists(1), channel->cal_conf.i[1]);
        } else {
            printCalibrationParameters(context, UNIT_AMPER, -1, channel->isCurrentCalibrationExists(0), channel->cal_conf.i[0]);
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
        sprintf(buffer, "CH%d u_tripped=%d", channelIndex, (int)channel->ovp.flags.tripped);
        SCPI_ResultText(context, buffer);
        sprintf(buffer, "CH%d u_state=%d", channelIndex, (int)channel->prot_conf.flags.u_state);
        SCPI_ResultText(context, buffer);
        sprintf(buffer, "CH%d u_type=%d", channelIndex, (int)channel->prot_conf.flags.u_type);
        SCPI_ResultText(context, buffer);
        sprintf(buffer, "CH%d u_delay=", channelIndex);
        strcatDuration(buffer, channel->prot_conf.u_delay);
        SCPI_ResultText(context, buffer);

        sprintf(buffer, "CH%d u_level=", channelIndex);
        strcatVoltage(buffer, channel->prot_conf.u_level);
        SCPI_ResultText(context, buffer);

        // current
        sprintf(buffer, "CH%d i_tripped=%d", channelIndex, (int)channel->ocp.flags.tripped);
        SCPI_ResultText(context, buffer);
        sprintf(buffer, "CH%d i_state=%d", channelIndex, (int)channel->prot_conf.flags.i_state);
        SCPI_ResultText(context, buffer);
        sprintf(buffer, "CH%d i_delay=", channelIndex);
        strcatDuration(buffer, channel->prot_conf.i_delay);
        SCPI_ResultText(context, buffer);

        // power
        sprintf(buffer, "CH%d p_tripped=%d", channelIndex, (int)channel->opp.flags.tripped);
        SCPI_ResultText(context, buffer);
        sprintf(buffer, "CH%d p_state=%d", channelIndex, (int)channel->prot_conf.flags.p_state);
        SCPI_ResultText(context, buffer);
        sprintf(buffer, "CH%d p_delay=", channelIndex);
        strcatDuration(buffer, channel->prot_conf.p_delay);
        SCPI_ResultText(context, buffer);

        sprintf(buffer, "CH%d p_level=", channelIndex);
        strcatPower(buffer, channel->prot_conf.p_level);
        SCPI_ResultText(context, buffer);
    }

    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        temp_sensor::TempSensor &sensor = temp_sensor::sensors[i];
        if (sensor.isInstalled()) {
            temperature::TempSensorTemperature &sensorTemperature = temperature::sensors[i];

            sprintf(buffer, "temp_%s_tripped=%d", sensor.name, (int)sensorTemperature.isTripped());
            SCPI_ResultText(context, buffer);

            sprintf(buffer, "temp_%s_state=%d", sensor.name, (int)sensorTemperature.prot_conf.state);
            SCPI_ResultText(context, buffer);

            sprintf(buffer, "temp_%s_delay=", sensor.name);
            strcatDuration(buffer, sensorTemperature.prot_conf.delay);
            SCPI_ResultText(context, buffer);

            sprintf(buffer, "temp_%s_level=", sensor.name);
            strcatFloat(buffer, sensorTemperature.prot_conf.level);
            strcat(buffer, " oC");
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
            sprintf(buffer, "%d, %s, %s, %s",
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
    for (int i = 0; i < CH_MAX; i++) {
        Channel& channel = Channel::get(i);
        if (channel.isInstalled()) {
            channel.channelInterface->readAllRegisters(channel.subchannelIndex, &g_ioexpRegisters[i][0], &g_adcRegisters[i][0]);
        }
    }
}

scpi_result_t scpi_cmd_diagnosticInformationRegsQ(scpi_t *context) {
    g_diagCallback = diagCallback;
    while (g_diagCallback) {
    	osDelay(1);
    }

    char buffer[2048];

    buffer[0] = 0;

    for (int i = 0; i < CH_MAX; i++) {
        Channel& channel = Channel::get(i);
        if (channel.isInstalled()) {
            if (g_slots[channel.slotIndex].moduleInfo->moduleType == MODULE_TYPE_DCP405 || g_slots[channel.slotIndex].moduleInfo->moduleType == MODULE_TYPE_DCP405B) {
                sprintf(buffer + strlen(buffer), "CH%d:\n", i + 1);

                sprintf(buffer + strlen(buffer), "\tIOEXP:\n");
                sprintf(buffer + strlen(buffer), "\t\tIODIRA   %X\n", (int)g_ioexpRegisters[i][0]);
                sprintf(buffer + strlen(buffer), "\t\tIODIRB   %X\n", (int)g_ioexpRegisters[i][1]);
                sprintf(buffer + strlen(buffer), "\t\tIPOLA    %X\n", (int)g_ioexpRegisters[i][2]);
                sprintf(buffer + strlen(buffer), "\t\tIPOLB    %X\n", (int)g_ioexpRegisters[i][3]);
                sprintf(buffer + strlen(buffer), "\t\tGPINTENA %X\n", (int)g_ioexpRegisters[i][4]);
                sprintf(buffer + strlen(buffer), "\t\tGPINTENB %X\n", (int)g_ioexpRegisters[i][5]);
                sprintf(buffer + strlen(buffer), "\t\tDEFVALA  %X\n", (int)g_ioexpRegisters[i][6]);
                sprintf(buffer + strlen(buffer), "\t\tDEFVALB  %X\n", (int)g_ioexpRegisters[i][7]);
                sprintf(buffer + strlen(buffer), "\t\tINTCONA  %X\n", (int)g_ioexpRegisters[i][8]);
                sprintf(buffer + strlen(buffer), "\t\tINTCONB  %X\n", (int)g_ioexpRegisters[i][9]);
                sprintf(buffer + strlen(buffer), "\t\tIOCON    %X\n", (int)g_ioexpRegisters[i][10]);
                sprintf(buffer + strlen(buffer), "\t\tIOCON    %X\n", (int)g_ioexpRegisters[i][11]);
                sprintf(buffer + strlen(buffer), "\t\tGPPUA    %X\n", (int)g_ioexpRegisters[i][12]);
                sprintf(buffer + strlen(buffer), "\t\tGPPUB    %X\n", (int)g_ioexpRegisters[i][13]);
                sprintf(buffer + strlen(buffer), "\t\tINTFA    %X\n", (int)g_ioexpRegisters[i][14]);
                sprintf(buffer + strlen(buffer), "\t\tINTFB    %X\n", (int)g_ioexpRegisters[i][15]);
                sprintf(buffer + strlen(buffer), "\t\tINTCAPA  %X\n", (int)g_ioexpRegisters[i][16]);
                sprintf(buffer + strlen(buffer), "\t\tINTCAPB  %X\n", (int)g_ioexpRegisters[i][17]);
                sprintf(buffer + strlen(buffer), "\t\tGPIOA    %X\n", (int)g_ioexpRegisters[i][18]);
                sprintf(buffer + strlen(buffer), "\t\tGPIOB    %X\n", (int)g_ioexpRegisters[i][19]);
                sprintf(buffer + strlen(buffer), "\t\tOLATA    %X\n", (int)g_ioexpRegisters[i][20]);
                sprintf(buffer + strlen(buffer), "\t\tOLATB    %X\n", (int)g_ioexpRegisters[i][21]);

                sprintf(buffer + strlen(buffer), "\tADC:\n");
                sprintf(buffer + strlen(buffer), "\t\tREG0     %X\n", (int)g_adcRegisters[i][0]);
                sprintf(buffer + strlen(buffer), "\t\tREG1     %X\n", (int)g_adcRegisters[i][1]);
                sprintf(buffer + strlen(buffer), "\t\tREG2     %X\n", (int)g_adcRegisters[i][2]);
                sprintf(buffer + strlen(buffer), "\t\tREG3     %X\n", (int)g_adcRegisters[i][3]);
            }
        }
    }

    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
