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

static void printCalibrationValue(scpi_t *context, char *buffer, calibration::Value &value) {
    const char *prefix;
    void (*strcat_value)(char *str, float value);
    if (value.voltOrCurr) {
        prefix = "u";
        strcat_value = strcatVoltage;
    } else {
        prefix = "i";
        strcat_value = strcatCurrent;
    }

    if (value.min_set) {
        strcpy(buffer, prefix);
        strcat(buffer, "_min=");
        strcat_value(buffer, value.min_val);
        SCPI_ResultText(context, buffer);
    }
    if (value.mid_set) {
        strcpy(buffer, prefix);
        strcat(buffer, "_mid=");
        strcat_value(buffer, value.mid_val);
        SCPI_ResultText(context, buffer);
    }
    if (value.max_set) {
        strcpy(buffer, prefix);
        strcat(buffer, "_max=");
        strcat_value(buffer, value.max_val);
        SCPI_ResultText(context, buffer);
    }

    strcpy(buffer, prefix);
    strcat(buffer, "_level=");
    switch (value.level) {
    case calibration::LEVEL_NONE:
        strcat(buffer, "none");
        break;
    case calibration::LEVEL_MIN:
        strcat(buffer, "min");
        break;
    case calibration::LEVEL_MID:
        strcat(buffer, "mid");
        break;
    case calibration::LEVEL_MAX:
        strcat(buffer, "max");
        break;
    }
    SCPI_ResultText(context, buffer);

    if (value.level != calibration::LEVEL_NONE) {
        strcpy(buffer, prefix);
        strcat(buffer, "_level_value=");
        strcat_value(buffer, value.getLevelValue());
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_adc=");
        strcat_value(buffer, value.getAdcValue());
        SCPI_ResultText(context, buffer);
    }
}

void printCalibrationParameters(scpi_t *context, Unit unit, uint8_t currentRange,
                                bool calParamsExists,
                                Channel::CalibrationValueConfiguration &calibrationValue,
                                char *buffer) {
    const char *prefix;
    void (*strcat_value)(char *str, float value);
    if (unit == UNIT_VOLT) {
        prefix = "u";
        strcat_value = strcatVoltage;
    } else {
        if (currentRange == 0) {
            prefix = "i_5A";
        } else if (currentRange == 1) {
            prefix = "i_50mA";
        } else {
            prefix = "i";
        }
        strcat_value = strcatCurrent;
    }

    strcpy(buffer, prefix);
    strcat(buffer, "_cal_params_exists=");
    strcatInt(buffer, calParamsExists);
    SCPI_ResultText(context, buffer);

    if (calParamsExists) {
        strcpy(buffer, prefix);
        strcat(buffer, "_min_level=");
        strcat_value(buffer, calibrationValue.min.dac);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_min_data=");
        strcat_value(buffer, calibrationValue.min.val);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_min_adc=");
        strcat_value(buffer, calibrationValue.min.adc);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_mid_level=");
        strcat_value(buffer, calibrationValue.mid.dac);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_mid_data=");
        strcat_value(buffer, calibrationValue.mid.val);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_mid_adc=");
        strcat_value(buffer, calibrationValue.mid.adc);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_max_level=");
        strcat_value(buffer, calibrationValue.max.dac);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_max_data=");
        strcat_value(buffer, calibrationValue.max.val);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_max_adc=");
        strcat_value(buffer, calibrationValue.max.adc);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_min_range=");
        strcat_value(buffer, calibrationValue.minPossible);
        SCPI_ResultText(context, buffer);
        strcpy(buffer, prefix);
        strcat(buffer, "_max_range=");
        strcat_value(buffer, calibrationValue.maxPossible);
        SCPI_ResultText(context, buffer);
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

    char buffer[128] = { 0 };

    if (calibration::isEnabled()) {
        if (calibration::isRemarkSet()) {
            sprintf(buffer, "remark=%s", calibration::getRemark());
            SCPI_ResultText(context, buffer);
        }
        printCalibrationValue(context, buffer, calibration::getVoltage());
        printCalibrationValue(context, buffer, calibration::getCurrent());
    } else {
        sprintf(buffer, "remark=%s %s", channel->cal_conf.calibration_date,
                channel->cal_conf.calibration_remark);
        SCPI_ResultText(context, buffer);

        printCalibrationParameters(context, UNIT_VOLT, -1,
                                   channel->cal_conf.flags.u_cal_params_exists, channel->cal_conf.u,
                                   buffer);
        if (channel->hasSupportForCurrentDualRange()) {
            printCalibrationParameters(context, UNIT_AMPER, 0,
                                       channel->cal_conf.flags.i_cal_params_exists_range_high,
                                       channel->cal_conf.i[0], buffer);
            printCalibrationParameters(context, UNIT_AMPER, 1,
                                       channel->cal_conf.flags.i_cal_params_exists_range_low,
                                       channel->cal_conf.i[1], buffer);
        } else {
            printCalibrationParameters(context, UNIT_AMPER, -1,
                                       channel->cal_conf.flags.i_cal_params_exists_range_high,
                                       channel->cal_conf.i[0], buffer);
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
