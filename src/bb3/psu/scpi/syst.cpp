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

#include <eez/firmware.h>
#include <eez/system.h>
#include <bb3/uart.h>
#include <bb3/usb.h>

#include <bb3/psu/psu.h>

#include <stdio.h>

#include <bb3/psu/scpi/psu.h>
#include <bb3/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <bb3/psu/ethernet_scpi.h>
#include <bb3/psu/ntp.h>
#include <bb3/mqtt.h>
#include <bb3/mcu/ethernet.h>
#endif
#include <bb3/psu/channel_dispatcher.h>
#include <bb3/psu/datetime.h>
#include <bb3/psu/profile.h>
#include <eez/sound.h>
#if OPTION_DISPLAY
#include <bb3/psu/gui/psu.h>
#endif
#include <bb3/psu/io_pins.h>
#include <bb3/psu/ontime.h>
#include <bb3/psu/temperature.h>

#include <bb3/aux_ps/fan.h>
#include <bb3/mcu/battery.h>

namespace eez {
namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_systemCapabilityQ(scpi_t *context) {
    char text[sizeof(STR_SYST_CAP)];
    stringCopy(text, sizeof(text), STR_SYST_CAP);
    SCPI_ResultText(context, text);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemErrorNextQ(scpi_t *context) {
    return SCPI_SystemErrorNextQ(context);
}

scpi_result_t scpi_cmd_systemErrorCountQ(scpi_t *context) {
    return SCPI_SystemErrorCountQ(context);
}

scpi_result_t scpi_cmd_systemVersionQ(scpi_t *context) {
    return SCPI_SystemVersionQ(context);
}

scpi_result_t scpi_cmd_systemPower(scpi_t *context) {
    bool up;
    if (!SCPI_ParamBool(context, &up, TRUE)) {
        return SCPI_RES_ERR;
    }

#if OPTION_AUX_TEMP_SENSOR
    if (temperature::sensors[temp_sensor::AUX].isTripped()) {
        SCPI_ErrorPush(context, SCPI_ERROR_CANNOT_EXECUTE_BEFORE_CLEARING_PROTECTION);
        return SCPI_RES_ERR;
    }
#endif

    changePowerState(up);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPowerQ(scpi_t *context) {
    SCPI_ResultBool(context, isPowerUp());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDate(scpi_t *context) {
    int32_t year;
    if (!SCPI_ParamInt(context, &year, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t month;
    if (!SCPI_ParamInt(context, &month, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t day;
    if (!SCPI_ParamInt(context, &day, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (year < 2000 || year > 2099) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }
    year = year - 2000;

    if (!datetime::isValidDate((uint8_t)year, (uint8_t)month, (uint8_t)day)) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    if (!datetime::setDate((uint8_t)year, (uint8_t)month, (uint8_t)day, 2)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDateClear(scpi_t *context) {
    persist_conf::setDateValid(0);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDateQ(scpi_t *context) {
    uint8_t year, month, day;
    if (!datetime::getDate(year, month, day)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char buffer[16] = { 0 };
    snprintf(buffer, sizeof(buffer), "%d, %d, %d", (int)(year + 2000), (int)month, (int)day);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTime(scpi_t *context) {
    int32_t hour;
    if (!SCPI_ParamInt(context, &hour, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t minute;
    if (!SCPI_ParamInt(context, &minute, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t second;
    if (!SCPI_ParamInt(context, &second, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (!datetime::isValidTime((uint8_t)hour, (uint8_t)minute, (uint8_t)second)) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    if (!datetime::setTime((uint8_t)hour, (uint8_t)minute, (uint8_t)second, 2)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeClear(scpi_t *context) {
    persist_conf::setTimeValid(0);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeQ(scpi_t *context) {
    uint8_t hour, minute, second;
    if (!datetime::getTime(hour, minute, second)) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char buffer[16] = { 0 };
    snprintf(buffer, sizeof(buffer), "%d, %d, %d", (int)hour, (int)minute, (int)second);
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

// NONE|ODD|EVEN
static scpi_choice_def_t dstChoice[] = { { "OFF", datetime::DST_RULE_OFF },
                                         { "EU", datetime::DST_RULE_EUROPE },
                                         { "USA", datetime::DST_RULE_USA },
                                         { "AUS", datetime::DST_RULE_AUSTRALIA },
                                         SCPI_CHOICE_LIST_END };

scpi_result_t scpi_cmd_systemTimeDst(scpi_t *context) {
    int32_t dstRule;
    if (!SCPI_ParamChoice(context, dstChoice, &dstRule, true)) {
        return SCPI_RES_ERR;
    }

    persist_conf::setDstRule((uint8_t)dstRule);

#if OPTION_ETHERNET
    ntp::reset();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeDstQ(scpi_t *context) {
    resultChoiceName(context, dstChoice, persist_conf::devConf.dstRule);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeZone(scpi_t *context) {
    const char *timeZoneStr;
    size_t timeZoneStrLength;

    if (!SCPI_ParamCharacters(context, &timeZoneStr, &timeZoneStrLength, true)) {
        return SCPI_RES_ERR;
    }

    int16_t timeZone;
    if (!parseTimeZone(timeZoneStr, timeZoneStrLength, timeZone)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setTimeZone(timeZone);

#if OPTION_ETHERNET
    ntp::reset();
#endif

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTimeZoneQ(scpi_t *context) {
    char timeZoneStr[32];
    formatTimeZone(persist_conf::devConf.timeZone, timeZoneStr, 32);
    SCPI_ResultText(context, timeZoneStr);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperImmediate(scpi_t *context) {
    sound::playBeep(true);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperState(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (enable != persist_conf::isSoundEnabled()) {
        persist_conf::enableSound(enable);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperStateQ(scpi_t *context) {
    SCPI_ResultBool(context, persist_conf::isSoundEnabled());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperKeyState(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (enable != persist_conf::isClickSoundEnabled()) {
        persist_conf::enableClickSound(enable);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemBeeperKeyStateQ(scpi_t *context) {
    SCPI_ResultBool(context, persist_conf::isClickSoundEnabled());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighClear(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::clearOtpProtection(sensor);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighLevel(scpi_t *context) {
    float level;
    if (!get_temperature_param(context, level, OTP_AUX_MIN_LEVEL, OTP_AUX_MAX_LEVEL,
                               OTP_AUX_DEFAULT_LEVEL)) {
        return SCPI_RES_ERR;
    }

    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setOtpLevel(sensor, level);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighLevelQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    return result_float(context, temperature::sensors[sensor].prot_conf.level, UNIT_CELSIUS);
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighState(scpi_t *context) {
    bool state;
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setOtpState(sensor, state);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighStateQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, temperature::sensors[sensor].prot_conf.state);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighDelayTime(scpi_t *context) {
    float delay;
    if (!get_duration_param(context, delay, OTP_AUX_MIN_DELAY, OTP_AUX_MAX_DELAY,
                            OTP_AUX_DEFAULT_DELAY)) {
        return SCPI_RES_ERR;
    }

    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    channel_dispatcher::setOtpDelay(sensor, delay);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighDelayTimeQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, temperature::sensors[sensor].prot_conf.delay);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemTemperatureProtectionHighTrippedQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, temperature::sensors[sensor].isTripped());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotCountQ(scpi_t *context) {
    SCPI_ResultInt(context, NUM_SLOTS);

    return SCPI_RES_OK;
}

Module *getModuleFromSlotIndexParam(scpi_t *context) {
    int32_t slotIndex;
    if (!SCPI_ParamInt(context, &slotIndex, TRUE)) {
        return nullptr;
    }
    if (slotIndex < 1 || slotIndex > NUM_SLOTS) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return nullptr;
    }
    return g_slots[slotIndex - 1];
}

scpi_result_t scpi_cmd_systemSlotModelQ(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    if (module->moduleType != MODULE_TYPE_NONE) {
        SCPI_ResultText(context, module->moduleName);
    } else {
        SCPI_ResultText(context, "");
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotVersionQ(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    if (module->moduleType != MODULE_TYPE_NONE) {
        char text[50];
        snprintf(text, sizeof(text), "R%dB%d", (int)(module->moduleRevision >> 8), (int)(module->moduleRevision & 0xFF));
        SCPI_ResultText(context, text);
    } else {
        SCPI_ResultText(context, "");
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotFirmwareQ(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    if (module->moduleType != MODULE_TYPE_NONE) {
        char text[50];
        snprintf(text, sizeof(text), "%d.%d", (int)(module->firmwareMajorVersion), (int)(module->firmwareMinorVersion));
        SCPI_ResultText(context, text);
    } else {
        SCPI_ResultText(context, "");
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotState(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    bool enabled;
    if (!SCPI_ParamBool(context, &enabled, TRUE)) {
        return SCPI_RES_ERR;
    }

    sendMessageToPsu(PSU_MESSAGE_SLOT_SET_ENABLED, (module->slotIndex << 8) | (enabled ? 1 : 0));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotStateQ(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, module->enabled);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotSno(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    const char *serialStr;
    size_t serialStrLen;
    if (!SCPI_ParamCharacters(context, &serialStr, &serialStrLen, true)) {
        return SCPI_RES_ERR;
    }

    int err;
    if (!setModuleSerialInfo(module->slotIndex, serialStr, serialStrLen, &err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotSnoQ(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    char text[50];
    getModuleSerialInfo(module->slotIndex, text);
    SCPI_ResultText(context, text);

    return SCPI_RES_OK;
}


scpi_result_t scpi_cmd_systemChannelCountQ(scpi_t *context) {
    SCPI_ResultInt(context, CH_NUM);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationCurrentQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, channel_dispatcher::getIMax(*channel));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationPowerQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, channel->params.PTOT);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelOptionQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    uint16_t features = channel->params.features;

    if (features & CH_FEATURE_VOLT) {
        SCPI_ResultText(context, "Volt");
    }

    if (features & CH_FEATURE_CURRENT) {
        SCPI_ResultText(context, "Current");
    }

    if (features & CH_FEATURE_POWER) {
        SCPI_ResultText(context, "Power");
    }

    if (features & CH_FEATURE_OE) {
        SCPI_ResultText(context, "OE");
    }

    if (features & CH_FEATURE_DPROG) {
        SCPI_ResultText(context, "DProg");
    }

    if (features & CH_FEATURE_RPROG) {
        SCPI_ResultText(context, "Rprog");
    }

    if (features & CH_FEATURE_RPOL) {
        SCPI_ResultText(context, "RPol");
    }

    if (features & CH_FEATURE_COUPLING) {
        SCPI_ResultText(context, "Coupling");
    }

    if (features & CH_FEATURE_DINPUT) {
        SCPI_ResultText(context, "DInput");
    }

    if (features & CH_FEATURE_DOUTPUT) {
        SCPI_ResultText(context, "DOutput");
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationVoltageQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, channel_dispatcher::getUMax(*channel));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationOntimeTotalQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    outputOnTime(context, ontime::g_moduleCounters[channel->slotIndex].getTotalTime());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelInformationOntimeLastQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    outputOnTime(context, ontime::g_moduleCounters[channel->slotIndex].getLastTime());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelModelQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultText(context, g_slots[channel->slotIndex]->moduleName);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelVersionQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    auto &slot = *g_slots[channel->slotIndex];
    char text[50];
    snprintf(text, sizeof(text), "R%dB%d", (int)(slot.moduleRevision >> 8), (int)(slot.moduleRevision & 0xFF));
    SCPI_ResultText(context, text);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelSlotQ(scpi_t *context) {
    Channel *channel = getPowerChannelFromParam(context, false, true);
    if (!channel) {
        return SCPI_RES_ERR;
    }

    SCPI_ResultInt(context, channel->slotIndex + 1);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuInformationOntimeTotalQ(scpi_t *context) {
    outputOnTime(context, ontime::g_mcuCounter.getTotalTime());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuInformationOntimeLastQ(scpi_t *context) {
    outputOnTime(context, ontime::g_mcuCounter.getLastTime());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuModelQ(scpi_t *context) {
#if defined(EEZ_PLATFORM_SIMULATOR)
    SCPI_ResultText(context, "Simulator");
#elif defined(EEZ_PLATFORM_STM32)
    SCPI_ResultText(context, "STM32F7");
#endif
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuVersionQ(scpi_t *context) {
    char revision[32];
    snprintf(revision, sizeof(revision), "R%dB%d", g_mcuRevision >> 8, g_mcuRevision & 0xFF);
    SCPI_ResultText(context, revision);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuFirmwareQ(scpi_t *context) {
    SCPI_ResultText(context, MCU_FIRMWARE);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuOptionQ(scpi_t *context) {
    char strFeatures[128] = { 0 };

#if OPTION_BP
    if (strFeatures[0]) {
        stringAppendString(strFeatures, sizeof(strFeatures), ", ");
    }
    stringAppendString(strFeatures, sizeof(strFeatures), "BPost");
#endif

#if OPTION_EXT_EEPROM
    if (strFeatures[0]) {
        stringAppendString(strFeatures, sizeof(strFeatures), ", ");
    }
    stringAppendString(strFeatures, sizeof(strFeatures), "EEPROM");
#endif

#if OPTION_EXT_RTC
    if (strFeatures[0]) {
        stringAppendString(strFeatures, sizeof(strFeatures), ", ");
    }
    stringAppendString(strFeatures, sizeof(strFeatures), "RTC");
#endif

    if (strFeatures[0]) {
        stringAppendString(strFeatures, sizeof(strFeatures), ", ");
    }
    stringAppendString(strFeatures, sizeof(strFeatures), "SDcard");

#if OPTION_ETHERNET
    if (strFeatures[0]) {
        stringAppendString(strFeatures, sizeof(strFeatures), ", ");
    }
    stringAppendString(strFeatures, sizeof(strFeatures), "Ethernet");
#endif

#if OPTION_DISPLAY
    if (strFeatures[0]) {
        stringAppendString(strFeatures, sizeof(strFeatures), ", ");
    }
    stringAppendString(strFeatures, sizeof(strFeatures), "Display");
#endif

    SCPI_ResultText(context, strFeatures);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCpuSnoQ(scpi_t *context) {
    SCPI_ResultText(context, getSerialNumber());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPowerProtectionTrip(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    persist_conf::enableShutdownWhenProtectionTripped(enable);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPowerProtectionTripQ(scpi_t *context) {
    SCPI_ResultBool(context, persist_conf::isShutdownWhenProtectionTrippedEnabled());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPonOutputDisable(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    persist_conf::enableForceDisablingAllOutputsOnPowerUp(enable);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPonOutputDisableQ(scpi_t *context) {
    SCPI_ResultBool(context, persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled());

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPasswordNew(scpi_t *context) {
    if (!checkPassword(context, persist_conf::devConf.systemPassword)) {
        return SCPI_RES_ERR;
    }

    const char *new_password;
    size_t new_password_len;

    if (!SCPI_ParamCharacters(context, &new_password, &new_password_len, true)) {
        return SCPI_RES_ERR;
    }

    int16_t err;
    if (!persist_conf::isSystemPasswordValid(new_password, new_password_len, err)) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    persist_conf::changeSystemPassword(new_password, new_password_len);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPasswordFpanelReset(scpi_t *context) {
    persist_conf::changeSystemPassword("", 0);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemPasswordCalibrationReset(scpi_t *context) {
    persist_conf::changeCalibrationPassword(CALIBRATION_PASSWORD_DEFAULT, strlen(CALIBRATION_PASSWORD_DEFAULT));
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemKlock(scpi_t *context) {
    persist_conf::lockFrontPanel(true);

    return SCPI_RES_OK;
}

scpi_choice_def_t rlStateChoice[] = {
    { "LOCal", RL_STATE_LOCAL },
    { "REMote", RL_STATE_REMOTE },
    { "RWLock", RL_STATE_RW_LOCK },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_systemCommunicateRlstate(scpi_t *context) {
    int32_t rlState;
    if (!SCPI_ParamChoice(context, rlStateChoice, &rlState, true)) {
        return SCPI_RES_ERR;
    }

    g_rlState = (RLState)rlState;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateRlstateQ(scpi_t *context) {
    resultChoiceName(context, rlStateChoice, g_rlState);
    return SCPI_RES_OK;
}

static scpi_choice_def_t usbModeChoice[] = {
    { "DEVice", USB_MODE_DEVICE }, 
    { "HOST", USB_MODE_HOST }, 
    { "OTG", USB_MODE_OTG }, 
    SCPI_CHOICE_LIST_END
};

scpi_result_t scpi_cmd_systemCommunicateUsbMode(scpi_t *context) {
    int32_t usbMode;
    if (!SCPI_ParamChoice(context, usbModeChoice, &usbMode, true)) {
        return SCPI_RES_ERR;
    }

    usb::selectUsbMode(usbMode, g_otgMode);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUsbModeQ(scpi_t *context) {
    resultChoiceName(context, usbModeChoice, g_usbMode);
    return SCPI_RES_OK;
}

static scpi_choice_def_t usbClassChoice[] = {
    { "VCOM", USB_DEVICE_CLASS_VIRTUAL_COM_PORT }, 
    { "MSTOrage", USB_DEVICE_CLASS_MASS_STORAGE_CLIENT }, 
    { "HID", USB_HOST_CLASS_HID }, 
    SCPI_CHOICE_LIST_END
};

scpi_result_t scpi_cmd_systemCommunicateUsbClass(scpi_t *context) {
    int32_t usbClass;
    if (!SCPI_ParamChoice(context, usbClassChoice, &usbClass, true)) {
        return SCPI_RES_ERR;
    }

    if (usbClass < USB_HOST_CLASS_HID) {
        usb::selectUsbDeviceClass(usbClass);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUsbClassQ(scpi_t *context) {
    resultChoiceName(context, usbClassChoice, 
        usb::isVirtualComPortActive() ? USB_DEVICE_CLASS_VIRTUAL_COM_PORT : 
        usb::isMassStorageActive() ? USB_DEVICE_CLASS_MASS_STORAGE_CLIENT :
        USB_HOST_CLASS_HID);
        
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemLocal(scpi_t *context) {
    g_rlState = RL_STATE_LOCAL;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemRemote(scpi_t *context) {
    g_rlState = RL_STATE_REMOTE;

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemRwlock(scpi_t *context) {
    g_rlState = RL_STATE_RW_LOCK;

    return SCPI_RES_OK;
}

static scpi_choice_def_t commInterfaceChoice[] = {
    { "USB", 1 }, 
    { "ETHernet", 2 }, 
    { "NTP", 3 }, 
    { "MQTT", 4 }, 
    SCPI_CHOICE_LIST_END
};

scpi_result_t scpi_cmd_systemCommunicateEnable(scpi_t *context) {
    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    int32_t commInterface;
    if (!SCPI_ParamChoice(context, commInterfaceChoice, &commInterface, true)) {
        return SCPI_RES_ERR;
    }

    if (commInterface == 1) {
        if (enable) {
            if (g_usbMode == USB_MODE_DISABLED) {
                usb::selectUsbMode(USB_MODE_DEVICE, g_otgMode);
            }
        } else {
            if (g_usbMode != USB_MODE_DISABLED) {
                usb::selectUsbMode(USB_MODE_DISABLED, g_otgMode);
            }
        }
    } else if (commInterface == 2) {
#if OPTION_ETHERNET
        persist_conf::enableEthernet(enable);
#else
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
#endif
    } else if (commInterface == 3) {
        persist_conf::enableNtp(enable);
    } else if (commInterface == 4) {
        persist_conf::enableMqtt(enable);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateEnableQ(scpi_t *context) {
    int32_t commInterface;
    if (!SCPI_ParamChoice(context, commInterfaceChoice, &commInterface, true)) {
        return SCPI_RES_ERR;
    }

    if (commInterface == 1) {
        SCPI_ResultBool(context, g_usbMode != USB_MODE_DISABLED);
    } else if (commInterface == 2) {
#if OPTION_ETHERNET
        SCPI_ResultBool(context, persist_conf::isEthernetEnabled());
#else
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
#endif
    } else if (commInterface == 3) {
        SCPI_ResultBool(context, persist_conf::isNtpEnabled());
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateEthernetDhcp(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    bool enable;
    if (!SCPI_ParamBool(context, &enable, TRUE)) {
        return SCPI_RES_ERR;
    }

    persist_conf::enableEthernetDhcp(enable);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetDhcpQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, persist_conf::isEthernetDhcpEnabled());
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetAddress(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled() || persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *ipAddressStr;
    size_t ipAddressStrLength;

    if (!SCPI_ParamCharacters(context, &ipAddressStr, &ipAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint32_t ipAddress;
    if (!parseIpAddress(ipAddressStr, ipAddressStrLength, ipAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetIpAddress(ipAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetAddressQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char ipAddressStr[16];
    if (persist_conf::devConf.ethernetDhcpEnabled) {
        ipAddressToString(ethernet::getIpAddress(), ipAddressStr, sizeof(ipAddressStr));
    } else {
        ipAddressToString(persist_conf::devConf.ethernetIpAddress, ipAddressStr, sizeof(ipAddressStr));
    }
    SCPI_ResultText(context, ipAddressStr);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetDns(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled() || persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *ipAddressStr;
    size_t ipAddressStrLength;

    if (!SCPI_ParamCharacters(context, &ipAddressStr, &ipAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint32_t ipAddress;
    if (!parseIpAddress(ipAddressStr, ipAddressStrLength, ipAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetDns(ipAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetDnsQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    if (persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ResultText(context, "unknown");
    } else {
        char ipAddressStr[16];
        ipAddressToString(persist_conf::devConf.ethernetDns, ipAddressStr, sizeof(ipAddressStr));
        SCPI_ResultText(context, ipAddressStr);
    }
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetGateway(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled() || persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *ipAddressStr;
    size_t ipAddressStrLength;

    if (!SCPI_ParamCharacters(context, &ipAddressStr, &ipAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint32_t ipAddress;
    if (!parseIpAddress(ipAddressStr, ipAddressStrLength, ipAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetGateway(ipAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetGatewayQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    if (persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ResultText(context, "unknown");
    } else {
        char ipAddressStr[16];
        ipAddressToString(persist_conf::devConf.ethernetGateway, ipAddressStr, sizeof(ipAddressStr));
        SCPI_ResultText(context, ipAddressStr);
    }
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetHostname(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *hostNameStr;
    size_t hostNameStrLength;

    if (!SCPI_ParamCharacters(context, &hostNameStr, &hostNameStrLength, true)) {
        return SCPI_RES_ERR;
    }

    if (hostNameStrLength > ETHERNET_HOST_NAME_SIZE) {
        SCPI_ErrorPush(context, SCPI_ERROR_CHARACTER_DATA_TOO_LONG);
        return SCPI_RES_ERR;
    }

    char hostName[ETHERNET_HOST_NAME_SIZE + 1];
    memcpy(hostName, hostNameStr, hostNameStrLength);
    hostName[hostNameStrLength] = 1;

    if (!persist_conf::validateEthernetHostName(hostName)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;        
    }

    persist_conf::setEthernetHostName(hostName);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetHostnameQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }
    SCPI_ResultText(context, persist_conf::devConf.ethernetHostName);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetSmask(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled() || persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *ipAddressStr;
    size_t ipAddressStrLength;

    if (!SCPI_ParamCharacters(context, &ipAddressStr, &ipAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint32_t ipAddress;
    if (!parseIpAddress(ipAddressStr, ipAddressStrLength, ipAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetSubnetMask(ipAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetSmaskQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    if (persist_conf::devConf.ethernetDhcpEnabled) {
        SCPI_ResultText(context, "unknown");
    } else {
        char ipAddressStr[16];
        ipAddressToString(persist_conf::devConf.ethernetSubnetMask, ipAddressStr, sizeof(ipAddressStr));
        SCPI_ResultText(context, ipAddressStr);
    }
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetPort(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    int32_t port;
    if (!SCPI_ParamInt(context, &port, TRUE)) {
        return SCPI_RES_ERR;
    }

    if ((port < 0 && port > 65535) || port == DEBUGGER_TCP_PORT) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetScpiPort((uint16_t)port);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetPortQ(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    SCPI_ResultInt(context, persist_conf::devConf.ethernetScpiPort);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetMac(scpi_t *context) {
#if OPTION_ETHERNET
    if (!persist_conf::isEthernetEnabled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    const char *macAddressStr;
    size_t macAddressStrLength;

    if (!SCPI_ParamCharacters(context, &macAddressStr, &macAddressStrLength, true)) {
        return SCPI_RES_ERR;
    }

    uint8_t macAddress[6];
    if (!parseMacAddress(macAddressStr, macAddressStrLength, macAddress)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    persist_conf::setEthernetMacAddress(macAddress);

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateEthernetMacQ(scpi_t *context) {
#if OPTION_ETHERNET
    char macAddressStr[18];
    macAddressToString(persist_conf::devConf.ethernetMacAddress, macAddressStr);
    SCPI_ResultText(context, macAddressStr);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateNtp(scpi_t *context) {
#if OPTION_ETHERNET
    const char *ntpServer;
    size_t ntpServerLength;

    if (!SCPI_ParamCharacters(context, &ntpServer, &ntpServerLength, true)) {
        return SCPI_RES_ERR;
    }

    if (ntpServerLength > 32) {
        SCPI_ErrorPush(context, SCPI_ERROR_CHARACTER_DATA_TOO_LONG);
        return SCPI_RES_ERR;
    }

    persist_conf::setNtpServer(ntpServer, ntpServerLength);
    ntp::reset();

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateNtpQ(scpi_t *context) {
#if OPTION_ETHERNET
    SCPI_ResultText(context, persist_conf::devConf.ntpServer);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateNtpFrequency(scpi_t *context) {
#if OPTION_ETHERNET
    uint32_t freq;
    if (!SCPI_ParamUInt32(context, &freq, false)) {
        return SCPI_RES_ERR;
    }

    if (freq < NTP_REFRESH_FREQUENCY_MIN || freq > NTP_REFRESH_FREQUENCY_MAX) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    persist_conf::setNtpRefreshFrequency(freq);
    ntp::reset();

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateNtpFrequencyQ(scpi_t *context) {
#if OPTION_ETHERNET
	SCPI_ResultUInt32(context, persist_conf::devConf.ntpRefreshFrequency);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemInhibitQ(scpi_t *context) {
    SCPI_ResultBool(context, io_pins::isInhibited());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalInputDataQ(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 1 && pin != 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (io_pins::g_ioPins[pin].function != io_pins::FUNCTION_INPUT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    SCPI_ResultInt(context, io_pins::getPinState(pin) ? 1 : 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalOutputData(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 3 && pin != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (io_pins::g_ioPins[pin].function != io_pins::FUNCTION_OUTPUT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    bool state;
    if (!SCPI_ParamBool(context, &state, TRUE)) {
        return SCPI_RES_ERR;
    }

    io_pins::setPinState(pin, state);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalOutputDataQ(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 3 && pin != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (io_pins::g_ioPins[pin].function != io_pins::FUNCTION_OUTPUT) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    SCPI_ResultBool(context, io_pins::getPinState(pin));

    return SCPI_RES_OK;
}

static scpi_choice_def_t functionChoice[] = { { "NONE", io_pins::FUNCTION_NONE },
                                              { "DINPut", io_pins::FUNCTION_INPUT },
                                              { "DOUTput", io_pins::FUNCTION_OUTPUT },
                                              { "FAULt", io_pins::FUNCTION_FAULT },
                                              { "INHibit", io_pins::FUNCTION_INHIBIT },
                                              { "ONCouple", io_pins::FUNCTION_ON_COUPLE },
                                              { "SYSTrig", io_pins::FUNCTION_SYSTRIG },
                                              { "TOUTput", io_pins::FUNCTION_TOUTPUT },
                                              { "PWM", io_pins::FUNCTION_PWM },
                                              { "DLOGtrig", io_pins::FUNCTION_DLOGTRIG },
                                              { "UART", io_pins::FUNCTION_UART },
                                              SCPI_CHOICE_LIST_END };

scpi_result_t scpi_cmd_systemDigitalPinFunction(scpi_t *context) {
    int32_t pin;
    SCPI_CommandNumbers(context, &pin, 1, 1);
    if (pin < 1 || pin > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    int32_t function;
    if (!SCPI_ParamChoice(context, functionChoice, &function, true)) {
        return SCPI_RES_ERR;
    }

    pin--;

    if (pin == DIN1) {
        if (
            function != io_pins::FUNCTION_NONE &&
            function != io_pins::FUNCTION_INPUT &&
            function != io_pins::FUNCTION_INHIBIT &&
            function != io_pins::FUNCTION_SYSTRIG &&
            function != io_pins::FUNCTION_DLOGTRIG &&
            function != io_pins::FUNCTION_UART
        ) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else if (pin == DIN2) {
        if (
            function != io_pins::FUNCTION_NONE &&
            function != io_pins::FUNCTION_INPUT &&
            function != io_pins::FUNCTION_INHIBIT &&
            function != io_pins::FUNCTION_SYSTRIG &&
            function != io_pins::FUNCTION_DLOGTRIG
        ) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else if (pin == DOUT1) {
        if (
            function != io_pins::FUNCTION_NONE &&
            function != io_pins::FUNCTION_OUTPUT &&
            function != io_pins::FUNCTION_FAULT &&
            function != io_pins::FUNCTION_ON_COUPLE &&
            function != io_pins::FUNCTION_UART
        ) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }else {
        if (
            function != io_pins::FUNCTION_NONE &&
            function != io_pins::FUNCTION_OUTPUT &&
            function != io_pins::FUNCTION_FAULT &&
            function != io_pins::FUNCTION_ON_COUPLE &&
            function != io_pins::FUNCTION_TOUTPUT &&
            function != io_pins::FUNCTION_PWM
        ) {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    }

    io_pins::setPinFunction(pin, function);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalPinFunctionQ(scpi_t *context) {
    int32_t pin;
    SCPI_CommandNumbers(context, &pin, 1, 1);
    if (pin < 1 || pin > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    pin--;

    resultChoiceName(context, functionChoice, io_pins::g_ioPins[pin].function);

    return SCPI_RES_OK;
}

static scpi_choice_def_t polarityChoice[] = { { "POSitive", io_pins::POLARITY_POSITIVE },
                                              { "NEGative", io_pins::POLARITY_NEGATIVE },
                                              SCPI_CHOICE_LIST_END };

scpi_result_t scpi_cmd_systemDigitalPinPolarity(scpi_t *context) {
    int32_t pin;
    SCPI_CommandNumbers(context, &pin, 1, 1);
    if (pin < 1 || pin > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    pin--;

    int32_t polarity;
    if (!SCPI_ParamChoice(context, polarityChoice, &polarity, true)) {
        return SCPI_RES_ERR;
    }

    io_pins::setPinPolarity(pin, polarity);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalPinPolarityQ(scpi_t *context) {
    int32_t pin;
    SCPI_CommandNumbers(context, &pin, 1, 1);
    if (pin < 1 || pin > 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return SCPI_RES_ERR;
    }

    pin--;

    resultChoiceName(context, polarityChoice, io_pins::g_ioPins[pin].polarity);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalOutputPwmFrequency(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (io_pins::g_ioPins[pin].function != io_pins::FUNCTION_PWM) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    float frequency;
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    if (param.special) {
        if (param.content.tag == SCPI_NUM_MIN) {
            frequency = io_pins::PWM_MIN_FREQUENCY;
        } else if (param.content.tag == SCPI_NUM_MAX) {
            frequency = io_pins::PWM_MAX_FREQUENCY;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            frequency = io_pins::PWM_DEFAULT_FREQUENCY;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE && param.unit != SCPI_UNIT_HERTZ) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        frequency = (float)param.content.value;
    }
    
    if (frequency != 0 && (frequency < io_pins::PWM_MIN_FREQUENCY || frequency > io_pins::PWM_MAX_FREQUENCY)) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    io_pins::setPwmFrequency(pin, frequency);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalOutputPwmFrequencyQ(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (io_pins::g_ioPins[pin].function != io_pins::FUNCTION_PWM) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, io_pins::getPwmFrequency(pin));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalOutputPwmDuty(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (io_pins::g_ioPins[pin].function != io_pins::FUNCTION_PWM) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    float duty;
    scpi_number_t param;
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        return SCPI_RES_ERR;
    }

    if (param.special) {
        if (param.content.tag == SCPI_NUM_MIN) {
            duty = io_pins::PWM_MIN_DUTY;
        } else if (param.content.tag == SCPI_NUM_MAX) {
            duty = io_pins::PWM_MAX_DUTY;
        } else if (param.content.tag == SCPI_NUM_DEF) {
            duty = io_pins::PWM_DEFAULT_DUTY;
        } else {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else {
        if (param.unit != SCPI_UNIT_NONE) {
            SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
            return SCPI_RES_ERR;
        }

        duty = (float)param.content.value;
    }

    if (duty < io_pins::PWM_MIN_DUTY || duty > io_pins::PWM_MAX_DUTY) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    io_pins::setPwmDuty(pin, duty);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemDigitalOutputPwmDutyQ(scpi_t *context) {
    int32_t pin;
    if (!SCPI_ParamInt(context, &pin, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (pin != 4) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    pin--;

    if (io_pins::g_ioPins[pin].function != io_pins::FUNCTION_PWM) {
        SCPI_ErrorPush(context, SCPI_ERROR_DIGITAL_PIN_FUNCTION_MISMATCH);
        return SCPI_RES_ERR;
    }

    SCPI_ResultFloat(context, io_pins::getPwmDuty(pin));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemRestart(scpi_t *context) {
	restart();
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateMqttSettings(scpi_t *context) {
#if OPTION_ETHERNET
    const char *addr;
    size_t addrLen;
    if (!SCPI_ParamCharacters(context, &addr, &addrLen, true)) {
        return SCPI_RES_ERR;
    }

    int32_t port;
    if (!SCPI_ParamInt(context, &port, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (port < 0 && port > 65535) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    const char *user;
    size_t userLen;
    if (!SCPI_ParamCharacters(context, &user, &userLen, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
        user = nullptr;
        userLen = 0;
    }

    const char *pass;
    size_t passLen;
    if (!SCPI_ParamCharacters(context, &pass, &passLen, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
        pass = nullptr;
        passLen = 0;
    }

    float period;
    scpi_number_t param;
    if (SCPI_ParamNumber(context, scpi_special_numbers_def, &param, true)) {
        if (param.special) {
            if (param.content.tag == SCPI_NUM_MIN) {
                period = mqtt::PERIOD_MIN;
            } else if (param.content.tag == SCPI_NUM_MAX) {
                period = mqtt::PERIOD_MAX;
            } else if (param.content.tag == SCPI_NUM_DEF) {
                period = mqtt::PERIOD_DEFAULT;
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
    } else {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
        period = 1.0f;
    }

    char savedPastAddrChar = *(addr + addrLen);
    *((char *)addr + addrLen) = 0;

    char savedPastUserChar = 0;
    if (user && userLen) {
        savedPastUserChar = *(user + userLen);
        *((char *)user + userLen) = 0;
    }

    char savedPastPassChar = 0;
    if (pass && passLen) {
        savedPastPassChar = *(pass + passLen);
        *((char *)pass + passLen) = 0;
    }

    persist_conf::setMqttSettings(persist_conf::devConf.mqttEnabled, addr, port, user, pass, period);

    *((char *)addr + addrLen) = savedPastAddrChar;
    if (savedPastUserChar) {
        *((char *)user + userLen) = savedPastUserChar;
    }
    if (savedPastPassChar) {
        *((char *)pass + passLen) = savedPastPassChar;
    }

    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemCommunicateMqttStateQ(scpi_t *context) {
#if OPTION_ETHERNET
    SCPI_ResultInt(context, mqtt::g_connectionState);
    return SCPI_RES_OK;
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_choice_def_t dateFormatChoice[] = {
    { "DMY", 1 },
    { "MDY", 2 },
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_result_t scpi_cmd_systemFormatDate(scpi_t *context) {
    int32_t dateFormat;
    if (!SCPI_ParamChoice(context, dateFormatChoice, &dateFormat, TRUE)) {
        return SCPI_RES_ERR;
    }

    using namespace datetime;

    if (persist_conf::devConf.dateTimeFormat == FORMAT_DMY_12 || persist_conf::devConf.dateTimeFormat == FORMAT_MDY_12) {
        persist_conf::setDateTimeFormat(dateFormat == 1 ? FORMAT_DMY_12 : FORMAT_MDY_12);
    } else {
        persist_conf::setDateTimeFormat(dateFormat == 1 ? FORMAT_DMY_24 : FORMAT_MDY_24);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemFormatDateQ(scpi_t *context) {
    using namespace datetime;
    resultChoiceName(context, dateFormatChoice, persist_conf::devConf.dateTimeFormat == FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == FORMAT_DMY_12 ? 1 : 2);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemFormatTime(scpi_t *context) {
    int32_t timeFormat;
    if (!SCPI_ParamInt(context, &timeFormat, TRUE)) {
        return SCPI_RES_ERR;
    }

    if (timeFormat != 12 && timeFormat != 24) {
        SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    }

    using namespace datetime;
    if (persist_conf::devConf.dateTimeFormat == FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == FORMAT_DMY_12) {
        persist_conf::setDateTimeFormat(timeFormat == 24 ? FORMAT_DMY_24 : FORMAT_DMY_12);
    } else {
        persist_conf::setDateTimeFormat(timeFormat == 24 ? FORMAT_MDY_24 : FORMAT_MDY_12);
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemFormatTimeQ(scpi_t *context) {
    using namespace datetime;
    SCPI_ResultInt(context, persist_conf::devConf.dateTimeFormat == FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == FORMAT_MDY_24 ? 24 : 12);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemFanStatusQ(scpi_t *context) {
    SCPI_ResultInt(context, aux_ps::fan::getStatus());
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemFanSpeedQ(scpi_t *context) {
    SCPI_ResultInt(context, aux_ps::fan::g_rpm);
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemMeasureScalarTemperatureThermistorDcQ(scpi_t *context) {
    int32_t sensor;
    if (!param_temp_sensor(context, sensor)) {
        return SCPI_RES_ERR;
    }

    if (!temperature::sensors[sensor].isInstalled()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
        return SCPI_RES_ERR;
    }

    if (!temperature::sensors[sensor].isTestOK()) {
        SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_ERROR);
        return SCPI_RES_ERR;
    }

    Channel *channel = temperature::sensors[sensor].getChannel();
    if (channel && (!isPowerUp() || !channel->isPowerOk())) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }

    char buffer[256] = { 0 };
    stringAppendFloat(buffer, sizeof(buffer), temperature::sensors[sensor].measure());
    SCPI_ResultCharacters(context, buffer, strlen(buffer));

    return SCPI_RES_OK;
}

#if defined(EEZ_PLATFORM_STM32)
static scpi_choice_def_t systemMeasureVoltageChoice[] = { 
    { "RTC", 1 },
    SCPI_CHOICE_LIST_END 
};
#endif


scpi_result_t scpi_cmd_systemMeasureScalarVoltageDcQ(scpi_t *context) {
#if defined(EEZ_PLATFORM_STM32)
    int32_t choice = 1;
    if (!SCPI_ParamChoice(context, systemMeasureVoltageChoice, &choice, false)) {
        if (SCPI_ParamErrorOccurred(context)) {
            return SCPI_RES_ERR;
        }
    }

    // only choice for now is RTC
    return result_float(context, mcu::battery::g_battery, UNIT_VOLT);
#else
    SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
    return SCPI_RES_ERR;
#endif
}

scpi_result_t scpi_cmd_systemDelay(scpi_t *context) {
    uint32_t delayMs;
    if (!SCPI_ParamUInt32(context, &delayMs, false)) {
        return SCPI_RES_ERR;
    }

    if (delayMs < 1 || delayMs > 10000) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    osDelay(delayMs);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemRelayCyclesQ(scpi_t *context) {
    ChannelList channelList;
    param_channels(context, channelList);
    if (channelList.numChannels == 0) {
        return SCPI_RES_ERR;
    }

    int err;

    for (int i = 0; i < channelList.numChannels; i++) {
        uint32_t relayCycles;
        if (!g_slots[channelList.channels[i].slotIndex]->getRelayCycles(channelList.channels[i].subchannelIndex, relayCycles, &err)) {
            SCPI_ErrorPush(context, err);
            return SCPI_RES_ERR;
        }
        SCPI_ResultUInt32(context, relayCycles);
    }

    return SCPI_RES_OK;
}

bool getLabelFromParam(scpi_t *context, const char *&label, size_t &labelLength, size_t maxLength) {
    if (!SCPI_ParamCharacters(context, &label, &labelLength, true)) {
        return false;
    }

    if (labelLength < 0 || labelLength > maxLength) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return false;
    }

    return true;
}

scpi_result_t scpi_cmd_systemSlotLabel(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    const char *label;
    size_t labelLength;
    if (!getLabelFromParam(context, label, labelLength, SLOT_LABEL_MAX_LENGTH)) {
        return SCPI_RES_ERR;
    }

    auto err = module->setLabel(label, labelLength);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotLabelQ(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    const char *label;
    auto err = module->getLabel(label);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    if (*label) {
        SCPI_ResultText(context, label);
    } else {
        SCPI_ResultText(context, module->getDefaultLabel());
    }

    return SCPI_RES_OK;
}

bool getColorFromParam(scpi_t *context, uint8_t &color) {
    int32_t colorIndex;
    if (!SCPI_ParamInt(context, &colorIndex, TRUE)) {
        return false;
    }
    if (colorIndex < 0 || colorIndex > 24) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return false;
    }
    color = (uint8_t)colorIndex;
    return true;
}

scpi_result_t scpi_cmd_systemSlotColor(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    uint8_t color;
    if (!getColorFromParam(context, color)) {
        return SCPI_RES_ERR;
    }

    auto err = module->setColor(color);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemSlotColorQ(scpi_t *context) {
    auto module = getModuleFromSlotIndexParam(context);
    if (!module) {
        return SCPI_RES_ERR;
    }

    uint8_t color;
    auto err = module->getColor(color);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt8(context, color);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelLabel(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    const char *label;
    size_t labelLength;
    if (!getLabelFromParam(context, label, labelLength, g_slots[slotAndSubchannelIndex.slotIndex]->getChannelLabelMaxLength(slotAndSubchannelIndex.subchannelIndex))) {
        return SCPI_RES_ERR;
    }

    auto err = g_slots[slotAndSubchannelIndex.slotIndex]->setChannelLabel(slotAndSubchannelIndex.subchannelIndex, label, labelLength);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelLabelQ(scpi_t *context) {
    ChannelList channelList;
    param_channels(context, channelList);
    if (channelList.numChannels == 0) {
        return SCPI_RES_ERR;
    }

    for (int i = 0; i < channelList.numChannels; i++) {
        const char *label;
        auto err = g_slots[channelList.channels[i].slotIndex]->getChannelLabel(channelList.channels[i].subchannelIndex, label);
        if (err != SCPI_RES_OK) {
            SCPI_ErrorPush(context, err);
            return SCPI_RES_ERR;
        }

        if (*label) {
            SCPI_ResultText(context, label);
        } else {
            SCPI_ResultText(context, g_slots[channelList.channels[i].slotIndex]->getDefaultChannelLabel(channelList.channels[i].subchannelIndex));
        }
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelColor(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    uint8_t color;
    if (!getColorFromParam(context, color)) {
        return SCPI_RES_ERR;
    }

    auto err = g_slots[slotAndSubchannelIndex.slotIndex]->setChannelColor(slotAndSubchannelIndex.subchannelIndex, color);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelColorQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
        return SCPI_RES_ERR;
    }

    uint8_t color;
    auto err = g_slots[slotAndSubchannelIndex.slotIndex]->getChannelColor(slotAndSubchannelIndex.subchannelIndex, color);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    SCPI_ResultUInt8(context, color);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelPinLabel(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
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

    const char *label;
    size_t labelLength;
    if (!getLabelFromParam(context, label, labelLength, g_slots[slotAndSubchannelIndex.slotIndex]->getChannelPinLabelMaxLength(slotAndSubchannelIndex.subchannelIndex, pin))) {
        return SCPI_RES_ERR;
    }

    auto err = g_slots[slotAndSubchannelIndex.slotIndex]->setChannelPinLabel(slotAndSubchannelIndex.subchannelIndex, pin, label, labelLength);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemChannelPinLabelQ(scpi_t *context) {
    SlotAndSubchannelIndex slotAndSubchannelIndex;
    if (!getChannelFromParam(context, slotAndSubchannelIndex)) {
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

    const char *label;
    auto err = g_slots[slotAndSubchannelIndex.slotIndex]->getChannelPinLabel(slotAndSubchannelIndex.subchannelIndex, pin, label);
    if (err != SCPI_RES_OK) {
        SCPI_ErrorPush(context, err);
        return SCPI_RES_ERR;
    }

    if (*label) {
        SCPI_ResultText(context, label);
    } else {
        SCPI_ResultText(context, g_slots[slotAndSubchannelIndex.slotIndex]->getDefaultChannelPinLabel(slotAndSubchannelIndex.subchannelIndex, pin));
    }

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemLfrequency(scpi_t *context) {
	uint32_t powerLineFrequency;
	if (!SCPI_ParamUInt32(context, &powerLineFrequency, false)) {
		return SCPI_RES_ERR;
	}

	if (powerLineFrequency != 50 && powerLineFrequency != 60) {
		SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
		return SCPI_RES_ERR;
	}

	persist_conf::setPowerLineFrequency(powerLineFrequency);

	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemLfrequencyQ(scpi_t *context) {
	SCPI_ResultUInt32(context, persist_conf::getPowerLineFrequency());
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartTransmit(scpi_t *context) {
    const char *uartStr;
    size_t uartStrLen;
    if (!SCPI_ParamCharacters(context, &uartStr, &uartStrLen, true)) {
        return SCPI_RES_ERR;
    }

    //uart::reinit();

    int err;

    if (!uart::transmit((uint8_t *)uartStr, (uint16_t)uartStrLen, 100, &err)) {
        SCPI_ErrorPush(context, err);
		return SCPI_RES_ERR;
    }
    
    if (!uart::transmit((uint8_t *)"\n", 1, 100, &err)) {
        SCPI_ErrorPush(context, err);
		return SCPI_RES_ERR;
    }

	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartReceiveQ(scpi_t *context) {
    char text[256 + 1];
	uint16_t n;
    int err;
	if (!uart::receive((uint8_t *)text, sizeof(text) - 1, n, &err)) {
        SCPI_ErrorPush(context, err);
		return SCPI_RES_ERR;
	}

	text[n] = 0;

    SCPI_ResultText(context, text);
	return SCPI_RES_OK;
}

static scpi_choice_def_t g_uartModeChoice[] = {
    { "BUFFer", uart::UART_MODE_BUFFER },
    { "SCPI", uart::UART_MODE_SCPI },
    { "BOOKmark", uart::UART_MODE_BOOKMARK },
    SCPI_CHOICE_LIST_END
};

scpi_result_t scpi_cmd_systemCommunicateUartMode(scpi_t *context) {
    int32_t uartMode;
    if (!SCPI_ParamChoice(context, g_uartModeChoice, &uartMode, true)) {
        return SCPI_RES_ERR;
    }

    io_pins::g_uartMode = (uart::UartMode)uartMode;
    uart::reinit();
    
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartModeQ(scpi_t *context) {
    resultChoiceName(context, g_uartModeChoice, io_pins::g_uartMode);
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartBaud(scpi_t *context) {
    uint32_t uartBaudRate;
    if (!SCPI_ParamUInt32(context, &uartBaudRate, true)) {
        return SCPI_RES_ERR;
    }

    if (uartBaudRate != 9600 && uartBaudRate != 14400 && uartBaudRate != 19200 && uartBaudRate != 38400 && uartBaudRate != 57600 && uartBaudRate != 115200) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    io_pins::g_uartBaudRate = uartBaudRate;
    uart::reinit();
    
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartBaudQ(scpi_t *context) {
    SCPI_ResultUInt32(context, io_pins::g_uartBaudRate);
	return SCPI_RES_OK;
}

static scpi_choice_def_t g_uartParityChoice[] = {
    { "NONe", 0 },
    { "EVEN", 1 },
    { "ODD", 2 },
    SCPI_CHOICE_LIST_END
};

scpi_result_t scpi_cmd_systemCommunicateUartParity(scpi_t *context) {
    int32_t uartParity;
    if (!SCPI_ParamChoice(context, g_uartParityChoice, &uartParity, true)) {
        return SCPI_RES_ERR;
    }

    io_pins::g_uartParity = uartParity;
    uart::reinit();
    
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartParityQ(scpi_t *context) {
    resultChoiceName(context, g_uartParityChoice, io_pins::g_uartParity);
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartData(scpi_t *context) {
    uint32_t uartDataBits;
    if (!SCPI_ParamUInt32(context, &uartDataBits, true)) {
        return SCPI_RES_ERR;
    }

    if (uartDataBits < 6 || uartDataBits > 8) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    io_pins::g_uartDataBits = uartDataBits;
    uart::reinit();
    
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartDataQ(scpi_t *context) {
    SCPI_ResultUInt32(context, io_pins::g_uartDataBits);
	return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartStop(scpi_t *context) {
    uint32_t uartStopBits;
    if (!SCPI_ParamUInt32(context, &uartStopBits, true)) {
        return SCPI_RES_ERR;
    }

    if (uartStopBits < 1 || uartStopBits > 2) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
        return SCPI_RES_ERR;
    }

    io_pins::g_uartStopBits = uartStopBits;
    uart::reinit();
    
    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_systemCommunicateUartStopQ(scpi_t *context) {
    SCPI_ResultUInt32(context, io_pins::g_uartStopBits);
	return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
