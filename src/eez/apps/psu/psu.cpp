/* / mcu / sound.h
 * EEZ PSU Firmware
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

#include <eez/apps/psu/psu.h>

#include <eez/system.h>

#include <eez/apps/psu/serial_psu.h>

#if OPTION_ETHERNET
#include <eez/apps/psu/ethernet.h>
#include <eez/apps/psu/ntp.h>
#endif

#include <eez/apps/psu/board.h>
#include <eez/apps/psu/dac.h>
#include <eez/apps/psu/datetime.h>
#include <eez/apps/psu/eeprom.h>
#include <eez/apps/psu/ioexp.h>
#include <eez/apps/psu/persist_conf.h>
#include <eez/apps/psu/rtc.h>
#include <eez/apps/psu/temperature.h>
#include <eez/modules/psu/adc.h>
#include <eez/sound.h>

#if OPTION_SD_CARD
#include <eez/apps/psu/dlog.h>
#include <eez/apps/psu/sd_card.h>
#endif

#include <eez/apps/psu/calibration.h>
#include <eez/apps/psu/profile.h>

#if OPTION_DISPLAY
#include <eez/apps/psu/gui/psu.h>
#endif

#if EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R3B4 ||                                          \
    EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R5B12
#if OPTION_WATCHDOG
#include <eez/apps/psu/watchdog.h>
#endif
#include <eez/apps/psu/fan.h>
#endif

#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/idle.h>
#include <eez/apps/psu/io_pins.h>
#include <eez/apps/psu/list_program.h>
#include <eez/apps/psu/trigger.h>

#include <eez/modules/bp3c/relays.h>
#include <eez/modules/dcpX05/eeprom.h>
#include <eez/index.h>

// TODO move this to some other place
#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

namespace eez {
namespace psu {

using namespace scpi;

bool g_isBooted = false;
static bool g_bootTestSuccess;
static bool g_powerIsUp = false;
static bool g_testPowerUpDelay = false;
static uint32_t g_powerDownTime;

static MaxCurrentLimitCause g_maxCurrentLimitCause;

static bool g_powerDownOnNextTick;

RLState g_rlState = RL_STATE_LOCAL;

bool g_rprogAlarm = false;

static uint32_t g_mainLoopCounter;

ontime::Counter g_powerOnTimeCounter(ontime::ON_TIME_COUNTER_POWER);

////////////////////////////////////////////////////////////////////////////////

static bool testShield();

static void psuRegSet(scpi_psu_reg_name_t name, scpi_reg_val_t val);

////////////////////////////////////////////////////////////////////////////////

void loadConf() {
    // loads global configuration parameters
    persist_conf::loadDevice();

    // loads global configuration parameters block 2
    persist_conf::loadDevice2();

    // load channels calibration parameters
    for (int i = 0; i < CH_NUM; ++i) {
        persist_conf::loadChannelCalibration(Channel::get(i));
    }
}

////////////////////////////////////////////////////////////////////////////////

extern Channel channels[CH_MAX];

void init() {
    for (int i = 0; i < CH_MAX; i++) {
    	channels[i].index = i + 1;
    }

    sound::init();

    eeprom::init();
    eeprom::test();

#if OPTION_SD_CARD
    sd_card::init();
#endif

    bp3c::relays::init();

    int channelIndex = 0;
    for (int i = 0; i < CH_MAX; i++) {
        uint16_t value;
        if (!dcpX05::eeprom::read(i, (uint8_t *)&value, 2, (uint16_t)0)) {
            g_slots[i].moduleType = MODULE_TYPE_NONE;
        } else if (value == 405) {
            g_slots[i].moduleType = MODULE_TYPE_DCP405;
            channels[channelIndex++].set(i, CH_BOARD_REVISION_DCP405_R1B1, CH_PARAMS_40V_5A);
        } else if (value == 505) {
            g_slots[i].moduleType = MODULE_TYPE_DCP505;
            channels[channelIndex++].set(i, CH_BOARD_REVISION_DCP505_R1B3, CH_PARAMS_50V_5A);
        } else {
            g_slots[i].moduleType = MODULE_TYPE_NONE;
        }
    }
    CH_NUM = channelIndex;

    g_powerOnTimeCounter.init();
    for (int i = 0; i < CH_NUM; i++) {
    	Channel::get(i).onTimeCounter.setType(i + 1);
        Channel::get(i).onTimeCounter.init();
    }

    loadConf();

    serial::init();

    rtc::init();
    datetime::init();

    event_queue::init();

    list::init();

#if OPTION_ETHERNET
    ethernet::init();
    ntp::init();
#else
    DebugTrace("Ethernet initialization skipped!");
#endif

#if EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R3B4 ||                                          \
    EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R5B12
    fan::init();
#endif

    temperature::init();

    trigger::init();

    // TODO move this to some other place
#if OPTION_ENCODER
    mcu::encoder::init();
#endif
}

////////////////////////////////////////////////////////////////////////////////

static bool testChannels() {
   if (!g_powerIsUp) {
       // test is skipped
       return true;
   }

    bool result = true;

    for (int i = 0; i < CH_NUM; ++i) {
        result &= Channel::get(i).test();
    }

    return result;
}

static bool testShield() {
    bool result = true;

    result &= rtc::test();
    result &= datetime::test();
    result &= eeprom::test();

#if OPTION_SD_CARD
    result &= sd_card::test();
#endif

#if OPTION_ETHERNET
    result &= ethernet::test();
#endif

    result &= temperature::test();

    return result;
}

bool test() {
    bool testResult = true;

#if EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R3B4 ||                                          \
    EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R5B12
    fan::test_start();
#endif

    testResult &= testShield();

    // test BP3c
    testResult &= bp3c::relays::test();

    testResult &= testChannels();

#if EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R3B4 ||                                          \
    EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R5B12
    testResult &= fan::test();
#endif

    if (!testResult) {
        sound::playBeep();
    }

    return testResult;
}

////////////////////////////////////////////////////////////////////////////////

void regSet(scpi_reg_name_t name, scpi_reg_val_t val) {
    if (serial::g_testResult == TEST_OK) {
        SCPI_RegSet(&serial::g_scpiContext, name, val);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        SCPI_RegSet(&ethernet::g_scpiContext, name, val);
    }
#endif
}

static bool psuReset() {
    // *ESE 0
    regSet(SCPI_REG_ESE, 0);

    // *SRE 0
    regSet(SCPI_REG_SRE, 0);

    // *STB 0
    regSet(SCPI_REG_STB, 0);

    // *ESR 0
    regSet(SCPI_REG_ESR, 0);

    // STAT:OPER[:EVEN] 0
    regSet(SCPI_REG_OPER, 0);

    // STAT:OPER:COND 0
    psuRegSet(SCPI_PSU_REG_OPER_COND, 0);

    // STAT:OPER:ENAB 0
    regSet(SCPI_REG_OPERE, 0);

    // STAT:OPER:INST[:EVEN] 0
    psuRegSet(SCPI_PSU_REG_OPER_INST_EVENT, 0);

    // STAT:OPER:INST:COND 0
    psuRegSet(SCPI_PSU_REG_OPER_INST_COND, 0);

    // STAT:OPER:INST:ENAB 0
    psuRegSet(SCPI_PSU_REG_OPER_INST_ENABLE, 0);

    // STAT:OPER:INST:ISUM[:EVEN] 0
    psuRegSet(SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1, 0);
    psuRegSet(SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT2, 0);

    // STAT:OPER:INST:ISUM:COND 0
    psuRegSet(SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1, 0);
    psuRegSet(SCPI_PSU_CH_REG_OPER_INST_ISUM_COND2, 0);

    // STAT:OPER:INST:ISUM:ENAB 0
    psuRegSet(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1, 0);
    psuRegSet(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE2, 0);

    // STAT:QUES[:EVEN] 0
    regSet(SCPI_REG_QUES, 0);

    // STAT:QUES:COND 0
    psuRegSet(SCPI_PSU_REG_QUES_COND, 0);

    // STAT:QUES:ENAB 0
    regSet(SCPI_REG_QUESE, 0);

    // STAT:QUES:INST[:EVEN] 0
    psuRegSet(SCPI_PSU_REG_QUES_INST_EVENT, 0);

    // STAT:QUES:INST:COND 0
    psuRegSet(SCPI_PSU_REG_QUES_INST_COND, 0);

    // STAT:QUES:INST:ENAB 0
    psuRegSet(SCPI_PSU_REG_QUES_INST_ENABLE, 0);

    // STAT:QUES:INST:ISUM[:EVEN] 0
    psuRegSet(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1, 0);
    psuRegSet(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT2, 0);

    // STAT:QUES:INST:ISUM:COND 0
    psuRegSet(SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1, 0);
    psuRegSet(SCPI_PSU_CH_REG_QUES_INST_ISUM_COND2, 0);

    // STAT:OPER:INST:ISUM:ENAB 0
    psuRegSet(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1, 0);
    psuRegSet(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE2, 0);

    // SYST:ERR:COUN? 0
    if (serial::g_testResult == TEST_OK) {
        scpi::resetContext(&serial::g_scpiContext);
    }

#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        scpi::resetContext(&ethernet::g_scpiContext);
    }

    ntp::reset();
#endif

    // TEMP:PROT[AUX]
    // TEMP:PROT:DEL
    // TEMP:PROT:STAT[AUX]
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        temperature::ProtectionConfiguration &temp_prot = temperature::sensors[i].prot_conf;
        temp_prot.sensor = i;
        if (temp_prot.sensor == temp_sensor::AUX) {
            temp_prot.delay = OTP_AUX_DEFAULT_DELAY;
            temp_prot.level = OTP_AUX_DEFAULT_LEVEL;
            temp_prot.state = OTP_AUX_DEFAULT_STATE;
        } else {
            temp_prot.delay = OTP_CH_DEFAULT_DELAY;
            temp_prot.level = OTP_CH_DEFAULT_LEVEL;
            temp_prot.state = OTP_CH_DEFAULT_STATE;
        }
    }

    // CAL[:MODE] OFF
    calibration::stop();

    // reset channels
    channel_dispatcher::setType(channel_dispatcher::TYPE_NONE);
    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).reset();
    }

    //
    trigger::reset();

    //
    list::reset();

    //
#if OPTION_SD_CARD
    dlog::reset();
#endif

    // SYST:POW ON
    if (powerUp()) {
    	for (int i = 0; i < CH_NUM; ++i) {
            Channel::get(i).update();
        }

    	return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

static bool loadAutoRecallProfile(profile::Parameters *profile, int *location) {
    if (persist_conf::isProfileAutoRecallEnabled()) {
        *location = persist_conf::getProfileAutoRecallLocation();
        if (profile::load(*location, profile)) {
            bool outputEnabled = false;

            for (int i = 0; i < CH_NUM; ++i) {
                if (profile->channels[i].flags.output_enabled) {
                    outputEnabled = true;
                    break;
                }
            }

            if (outputEnabled) {
                bool disableOutputs = false;

                if (persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled() ||
                    !g_bootTestSuccess) {
                    disableOutputs = true;
                } else {
                    if (*location != 0) {
                        profile::Parameters defaultProfile;
                        if (profile::load(0, &defaultProfile)) {
                            if (profile->flags.channelsCoupling !=
                                defaultProfile.flags.channelsCoupling) {
                                disableOutputs = true;
                                event_queue::pushEvent(
                                    event_queue::EVENT_WARNING_AUTO_RECALL_VALUES_MISMATCH);
                            } else {
                                for (int i = 0; i < CH_NUM; ++i) {
                                    if (!eez::equal(profile->channels[i].u_set,
                                                    defaultProfile.channels[i].u_set,
                                                    getPrecision(UNIT_VOLT)) ||
                                        !eez::equal(profile->channels[i].i_set,
                                                    defaultProfile.channels[i].i_set,
                                                    getPrecision(UNIT_AMPER))) {
                                        disableOutputs = true;
                                        event_queue::pushEvent(
                                            event_queue::EVENT_WARNING_AUTO_RECALL_VALUES_MISMATCH);
                                        break;
                                    }
                                }
                            }
                        } else {
                            disableOutputs = true;
                        }
                    }
                }

                if (disableOutputs) {
                    for (int i = 0; i < CH_NUM; ++i) {
                        profile->channels[i].flags.output_enabled = false;
                    }
                }
            }

            return true;
        }
    }

    return false;
}

static bool autoRecall() {
    profile::Parameters profile;
    int location;
    if (loadAutoRecallProfile(&profile, &location)) {
        if (profile::recallFromProfile(&profile, location)) {
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

void boot() {
    init();

    // test
    g_bootTestSuccess = true;

    g_bootTestSuccess &= testShield();

    if (!autoRecall()) {
        psuReset();
    }

    // play beep if there is an error during boot procedure
    if (!g_bootTestSuccess) {
        sound::playBeep();
    }

    g_isBooted = true;
}

////////////////////////////////////////////////////////////////////////////////

bool powerUp() {
    if (g_powerIsUp) {
        return true;
    }

    if (!temperature::isAllowedToPowerUp()) {
        return false;
    }

    g_rlState = persist_conf::devConf.flags.isFrontPanelLocked ? RL_STATE_REMOTE : RL_STATE_LOCAL;

#if OPTION_DISPLAY
    eez::gui::showWelcomePage();
#endif

#if EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R3B4 ||                                          \
    EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R5B12
    fan::test_start();
#endif

    // reset channels
    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).reset();
    }

    // turn power on
    board::powerUp();
    g_powerIsUp = true;
    g_powerOnTimeCounter.start();

    // init channels
    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).init();
    }

    bool testSuccess = true;

    if (g_isBooted) {
        testSuccess &= testShield();
    }

    // test channels
    for (int i = 0; i < CH_NUM; ++i) {
        testSuccess &= Channel::get(i).test();
    }

#if EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R3B4 ||                                          \
    EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R5B12
    testSuccess &= fan::test();
#endif

    // turn on Power On (PON) bit of ESE register
    setEsrBits(ESR_PON);

    event_queue::pushEvent(event_queue::EVENT_INFO_POWER_UP);

    // play power up tune on success
    if (testSuccess) {
        sound::playPowerUp();
    }

    g_bootTestSuccess &= testSuccess;

    return true;
}

void powerDown() {
#if OPTION_DISPLAY
    if (g_isBooted) {
        eez::gui::showEnteringStandbyPage();
    } else {
        eez::gui::showStandbyPage();
    }
#endif

    if (!g_powerIsUp)
        return;

    trigger::abort();
#if OPTION_SD_CARD
    dlog::abort();
#endif

    channel_dispatcher::setType(channel_dispatcher::TYPE_NONE);

    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).onPowerDown();
    }

    board::powerDown();

    g_powerIsUp = false;
    g_powerOnTimeCounter.stop();

    event_queue::pushEvent(event_queue::EVENT_INFO_POWER_DOWN);

#if EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R3B4 ||                                          \
    EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R5B12
    fan::g_testResult = TEST_OK;
#endif

    io_pins::tick(micros());

    sound::playPowerDown();
}

bool isPowerUp() {
    return g_powerIsUp;
}

bool changePowerState(bool up) {
    if (up == g_powerIsUp)
        return true;

    if (up) {
        // at least MIN_POWER_UP_DELAY seconds shall pass after last power down
        if (g_testPowerUpDelay) {
            if (millis() - g_powerDownTime < MIN_POWER_UP_DELAY * 1000)
                return false;
            g_testPowerUpDelay = false;
        }

        g_bootTestSuccess = true;

        if (!powerUp()) {
            return false;
        }

        // auto recall channels parameters from profile
        profile::Parameters profile;
        int location;
        if (loadAutoRecallProfile(&profile, &location)) {
            for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
                memcpy(&temperature::sensors[i].prot_conf, profile.temp_prot + i,
                       sizeof(temperature::ProtectionConfiguration));
            }
            profile::recallChannelsFromProfile(&profile, location);
        }

        profile::save();
    } else {
#if OPTION_DISPLAY
        eez::gui::showEnteringStandbyPage();
#endif
        g_powerIsUp = false;
        profile::saveImmediately();
        g_powerIsUp = true;

        profile::enableSave(false);
        powerDown();
        profile::enableSave(true);

        g_testPowerUpDelay = true;
        g_powerDownTime = millis();
    }

    return true;
}

void schedulePowerDown() {
    g_powerDownOnNextTick = true;
}

void powerDownBySensor() {
    if (g_powerIsUp) {
#if OPTION_DISPLAY
        eez::gui::showEnteringStandbyPage();
#endif

        for (int i = 0; i < CH_NUM; ++i) {
            Channel::get(i).outputEnable(false);
        }

        g_powerIsUp = false;
        profile::saveImmediately();
        g_powerIsUp = true;

        profile::enableSave(false);
        powerDown();
        profile::enableSave(true);
    }
}

////////////////////////////////////////////////////////////////////////////////

bool reset() {
    if (psuReset()) {
        profile::save();
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////

static void disableChannels() {
    for (int i = 0; i < CH_NUM; ++i) {
        if (Channel::get(i).isOutputEnabled()) {
            Channel::get(i).outputEnable(false);
        }
    }
}

void onProtectionTripped() {
    if (isPowerUp()) {
        if (persist_conf::isShutdownWhenProtectionTrippedEnabled()) {
            powerDownBySensor();
        } else {
            if (persist_conf::isOutputProtectionCoupleEnabled()) {
                disableChannels();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void tick() {
    ++g_mainLoopCounter;

    if (g_powerDownOnNextTick) {
        g_powerDownOnNextTick = false;
        powerDownBySensor();
    }

    uint32_t tick_usec = micros();

    static uint32_t lastTickList = 0;
    if (list::isActive()) {
        if (lastTickList == 0) {
            lastTickList = tick_usec;
        } else if (tick_usec - lastTickList >= 250) {
            lastTickList = tick_usec;
            list::tick(tick_usec);
            io_pins::tick(tick_usec);
        }
    } else {
        lastTickList = 0;
    }

#if OPTION_SD_CARD
    dlog::tick(tick_usec);
#endif

    static uint32_t lastTickAdc = 0;
    if (lastTickAdc == 0) {
        lastTickAdc = tick_usec;
    } else if (tick_usec - lastTickAdc >= ADC_READ_TIME_US / 2) {
        lastTickAdc = tick_usec;

        for (int i = 0; i < CH_NUM; ++i) {
            Channel::get(i).tick(tick_usec);
        }
    }

    static uint32_t lastTickIoPins = 0;
    if (lastTickIoPins == 0) {
        lastTickIoPins = tick_usec;
    } else if (tick_usec - lastTickIoPins >= 1000) {
        lastTickIoPins = tick_usec;
        io_pins::tick(tick_usec);
    }

#ifdef DEBUG
    debug::tick(tick_usec);
#endif

    g_powerOnTimeCounter.tick(tick_usec);

    temperature::tick(tick_usec);

    fan::tick(tick_usec);

    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).tick(tick_usec);
    }

    trigger::tick(tick_usec);

    list::tick(tick_usec);

    event_queue::tick(tick_usec);

    profile::tick(tick_usec);

    serial::tick(tick_usec);

    datetime::tick(tick_usec);

#if OPTION_ETHERNET
    if (g_mainLoopCounter % 2 == 0) {
        // tick ethernet every other time
        ethernet::tick(tick_usec);
    } else {
        ntp::tick(tick_usec);
    }
#endif

	sound::tick();

    idle::tick();

#if OPTION_WATCHDOG && (EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R3B4 ||                      \
                        EEZ_PSU_SELECTED_REVISION == EEZ_PSU_REVISION_R5B12)
    watchdog::tick(tick_usec);
#endif
}

////////////////////////////////////////////////////////////////////////////////

void psuRegSet(scpi_psu_reg_name_t name, scpi_reg_val_t val) {
    if (serial::g_testResult == TEST_OK) {
        reg_set(&serial::g_scpiContext, name, val);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        reg_set(&ethernet::g_scpiContext, name, val);
    }
#endif
}

void setEsrBits(int bit_mask) {
    if (serial::g_testResult == TEST_OK) {
        SCPI_RegSetBits(&serial::g_scpiContext, SCPI_REG_ESR, bit_mask);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        SCPI_RegSetBits(&ethernet::g_scpiContext, SCPI_REG_ESR, bit_mask);
    }
#endif
}

void setQuesBits(int bit_mask, bool on) {
    if (serial::g_testResult == TEST_OK) {
        reg_set_ques_bit(&serial::g_scpiContext, bit_mask, on);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        reg_set_ques_bit(&ethernet::g_scpiContext, bit_mask, on);
    }
#endif
}

void setOperBits(int bit_mask, bool on) {
    if (serial::g_testResult == TEST_OK) {
        reg_set_oper_bit(&serial::g_scpiContext, bit_mask, on);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        reg_set_oper_bit(&ethernet::g_scpiContext, bit_mask, on);
    }
#endif
}

void generateError(int16_t error) {
    if (serial::g_testResult == TEST_OK) {
        SCPI_ErrorPush(&serial::g_scpiContext, error);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        SCPI_ErrorPush(&ethernet::g_scpiContext, error);
    }
#endif
    event_queue::pushEvent(error);
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

const char *getCpuModel() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    return "Simulator, " FIRMWARE;
#elif defined(EEZ_PLATFORM_STM32)
    return "STM32, " FIRMWARE;
#endif
}

const char *getCpuType() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    return "Simulator";
#elif defined(EEZ_PLATFORM_STM32)
    return "STM32";
#endif
}

const char *getCpuEthernetType() {
#if defined(EEZ_PLATFORM_SIMULATOR)
    return "Simulator";
#elif defined(EEZ_PLATFORM_STM32)
    return "None";
#endif
}

bool isMaxCurrentLimited() {
    return g_maxCurrentLimitCause != MAX_CURRENT_LIMIT_CAUSE_NONE;
}

void limitMaxCurrent(MaxCurrentLimitCause cause) {
    if (g_maxCurrentLimitCause != cause) {
        g_maxCurrentLimitCause = cause;

        if (isMaxCurrentLimited()) {
            for (int i = 0; i < CH_NUM; ++i) {
                if (Channel::get(i).isOutputEnabled() &&
                    Channel::get(i).i.mon_last > ERR_MAX_CURRENT) {
                    Channel::get(i).setCurrent(Channel::get(i).i.min);
                }

                if (ERR_MAX_CURRENT < Channel::get(i).getCurrentLimit()) {
                    Channel::get(i).setCurrentLimit(ERR_MAX_CURRENT);
                }
            }
        }
    }
}

void unlimitMaxCurrent() {
    limitMaxCurrent(MAX_CURRENT_LIMIT_CAUSE_NONE);
}

MaxCurrentLimitCause getMaxCurrentLimitCause() {
    return g_maxCurrentLimitCause;
}

} // namespace psu
} // namespace eez
