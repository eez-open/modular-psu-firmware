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

#include <eez/apps/psu/psu.h>

#include <eez/system.h>

#include <eez/apps/psu/init.h>
#include <eez/apps/psu/serial_psu.h>

#if OPTION_ETHERNET
#include <eez/apps/psu/ethernet.h>
#include <eez/apps/psu/ntp.h>
#endif

#include <eez/apps/psu/board.h>
#include <eez/modules/dcpX05/dac.h>
#include <eez/apps/psu/datetime.h>
#include <eez/modules/mcu/eeprom.h>
#include <eez/modules/dcpX05/ioexp.h>
#include <eez/apps/psu/persist_conf.h>
#include <eez/apps/psu/rtc.h>
#include <eez/apps/psu/temperature.h>
#include <eez/modules/dcpX05/adc.h>
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

#if OPTION_FAN
#include <eez/modules/aux_ps/fan.h>
#endif

#include <eez/apps/psu/channel_dispatcher.h>
#include <eez/apps/psu/event_queue.h>
#include <eez/apps/psu/idle.h>
#include <eez/apps/psu/io_pins.h>
#include <eez/apps/psu/list_program.h>
#include <eez/apps/psu/trigger.h>
#include <eez/apps/psu/ontime.h>

#include <eez/modules/bp3c/relays.h>
#include <eez/modules/bp3c/eeprom.h>
#include <eez/index.h>

// TODO move this to some other place
#if OPTION_ENCODER
#include <eez/modules/mcu/encoder.h>
#endif

#include <eez/modules/mcu/battery.h>

#include <eez/scpi/scpi.h>

#if defined(EEZ_PLATFORM_SIMULATOR)

// for home directory (see getConfFilePath)
#ifdef _WIN32
#undef INPUT
#undef OUTPUT
#include <Shlobj.h>
#include <Windows.h>
#include <direct.h>
#else
#include <string.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#endif

namespace eez {

using namespace scpi;

TestResult g_masterTestResult;

#if defined(EEZ_PLATFORM_SIMULATOR)

char *getConfFilePath(const char *file_name) {
    static char file_path[1024];

    *file_path = 0;

#ifdef _WIN32
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, file_path))) {
        strcat(file_path, "\\.eez_psu_sim");
        _mkdir(file_path);
        strcat(file_path, "\\");
    }
#elif defined(__EMSCRIPTEN__)
    strcat(file_path, "/eez_modular_firmware/");
#else
    const char *home_dir = 0;
    if ((home_dir = getenv("HOME")) == NULL) {
        home_dir = getpwuid(getuid())->pw_dir;
    }
    if (home_dir) {
        strcat(file_path, home_dir);
        strcat(file_path, "/.eez_psu_sim");
        mkdir(file_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        strcat(file_path, "/");
    }
#endif

    char *q = file_path + strlen(file_path);
    const char *p = file_name;
    while (*p) {
        char ch = *p++;
#ifdef _WIN32
        if (ch == '/')
            *q++ = '\\';
#else
        if (ch == '\\')
            *q++ = '/';
#endif
        else
            *q++ = ch;
    }
    *q = 0;

    return file_path;
}

#endif

void generateError(int16_t error) {
    eez::scpi::generateError(error);
}

namespace psu {

using namespace scpi;

bool g_isBooted = false;
uint32_t g_isBootedTime;
static bool g_bootTestSuccess;
static bool g_powerIsUp = false;
static bool g_testPowerUpDelay = false;
static uint32_t g_powerDownTime;

static MaxCurrentLimitCause g_maxCurrentLimitCause;

RLState g_rlState = RL_STATE_LOCAL;

bool g_rprogAlarm = false;

void (*g_diagCallback)();

////////////////////////////////////////////////////////////////////////////////

static bool testMaster();

////////////////////////////////////////////////////////////////////////////////

void loadConf() {
    // loads global configuration parameters
    persist_conf::loadDevice();

    // loads global configuration parameters block 2
    persist_conf::loadDevice2();

    // load channels calibration parameters
    for (int i = 0; i < CH_NUM; ++i) {
        if (Channel::get(i).isInstalled()) {
            persist_conf::loadChannelCalibration(Channel::get(i));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void init() {
    for (int i = 0; i < CH_MAX; i++) {
    	Channel::get(i).setChannelIndex(i);
    }

    sound::init();

    mcu::eeprom::init();
    mcu::eeprom::test();

    bp3c::eeprom::init();
    bp3c::eeprom::test();

#if OPTION_SD_CARD
    sd_card::init();
#endif

    bp3c::relays::init();

    // inst:memo 1,0,2,406
    // 1: slot no. (1, 2 or 3)
    // 0: address
    // 2: size of value (1 byte or 2 bytes)
    // 406: value

    ontime::g_mcuCounter.init();

    int channelIndex = 0;
    for (uint8_t slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        auto& slot = g_slots[slotIndex];

        static const uint16_t ADDRESS = 0;
        uint16_t value[2];
        if (!bp3c::eeprom::read(slotIndex, (uint8_t *)&value, sizeof(value), ADDRESS)) {
            value[0] = MODULE_TYPE_NONE;
            value[1] = 0;
        }

        uint16_t moduleType = value[0];
        uint16_t moduleRevision = value[1];

        slot.moduleInfo = getModuleInfo(moduleType);
        slot.moduleRevision = moduleRevision;
        slot.channelIndex = channelIndex;

        for (uint8_t subchannelIndex = 0; subchannelIndex < slot.moduleInfo->numChannels; subchannelIndex++) {
            Channel::get(channelIndex++).set(slotIndex, subchannelIndex);
        }

        if (slot.moduleInfo->moduleType != MODULE_TYPE_NONE) {
            persist_conf::loadModuleConf(slotIndex);
            ontime::g_moduleCounters[slotIndex].init();
        }
    }
    CH_NUM = channelIndex;

    loadConf();

    serial::init();

    rtc::init();
    datetime::init();

    event_queue::init();
    profile::init();

    io_pins::init();

    list::init();

#if OPTION_ETHERNET
    ethernet::init();
    ntp::init();
#else
    DebugTrace("Ethernet initialization skipped!");
#endif

#if OPTION_FAN
    aux_ps::fan::init();
#endif

    temperature::init();

    mcu::battery::init();

    trigger::init();

    // TODO move this to some other place
#if OPTION_DISPLAY && OPTION_ENCODER
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

static bool testMaster() {
    bool result = true;

#if OPTION_FAN
    result &= aux_ps::fan::test();
#endif

    result &= rtc::test();
    result &= datetime::test();
    result &= mcu::eeprom::test();

#if OPTION_SD_CARD
    result &= sd_card::test();
#endif

#if OPTION_ETHERNET
    result &= ethernet::test();
#endif

    result &= temperature::test();

    result &= mcu::battery::test();

    // test BP3c
    result &= bp3c::relays::test();

    g_masterTestResult = result ? TEST_OK : TEST_FAILED;

    return result;
}

bool test() {
    bool testResult = true;

    testResult &= testMaster();

    testResult &= testChannels();

    if (!testResult) {
        sound::playBeep();
    }

    return testResult;
}

////////////////////////////////////////////////////////////////////////////////

static bool psuReset() {
    // *ESE 0
    scpi_reg_set(SCPI_REG_ESE, 0);

    // *SRE 0
    scpi_reg_set(SCPI_REG_SRE, 0);

    // *STB 0
    scpi_reg_set(SCPI_REG_STB, 0);

    // *ESR 0
    scpi_reg_set(SCPI_REG_ESR, 0);

    // STAT:OPER[:EVEN] 0
    scpi_reg_set(SCPI_REG_OPER, 0);

    // STAT:OPER:COND 0
    reg_set(SCPI_PSU_REG_OPER_COND, 0);

    // STAT:OPER:ENAB 0
    scpi_reg_set(SCPI_REG_OPERE, 0);

    // STAT:OPER:INST[:EVEN] 0
    reg_set(SCPI_PSU_REG_OPER_INST_EVENT, 0);

    // STAT:OPER:INST:COND 0
    reg_set(SCPI_PSU_REG_OPER_INST_COND, 0);

    // STAT:OPER:INST:ENAB 0
    reg_set(SCPI_PSU_REG_OPER_INST_ENABLE, 0);

    // STAT:OPER:INST:ISUM[:EVEN] 0
    reg_set(SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1, 0);
    reg_set(SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT2, 0);

    // STAT:OPER:INST:ISUM:COND 0
    reg_set(SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1, 0);
    reg_set(SCPI_PSU_CH_REG_OPER_INST_ISUM_COND2, 0);

    // STAT:OPER:INST:ISUM:ENAB 0
    reg_set(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1, 0);
    reg_set(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE2, 0);

    // STAT:QUES[:EVEN] 0
    scpi_reg_set(SCPI_REG_QUES, 0);

    // STAT:QUES:COND 0
    reg_set(SCPI_PSU_REG_QUES_COND, 0);

    // STAT:QUES:ENAB 0
    scpi_reg_set(SCPI_REG_QUESE, 0);

    // STAT:QUES:INST[:EVEN] 0
    reg_set(SCPI_PSU_REG_QUES_INST_EVENT, 0);

    // STAT:QUES:INST:COND 0
    reg_set(SCPI_PSU_REG_QUES_INST_COND, 0);

    // STAT:QUES:INST:ENAB 0
    reg_set(SCPI_PSU_REG_QUES_INST_ENABLE, 0);

    // STAT:QUES:INST:ISUM[:EVEN] 0
    reg_set(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1, 0);
    reg_set(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT2, 0);

    // STAT:QUES:INST:ISUM:COND 0
    reg_set(SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1, 0);
    reg_set(SCPI_PSU_CH_REG_QUES_INST_ISUM_COND2, 0);

    // STAT:OPER:INST:ISUM:ENAB 0
    reg_set(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1, 0);
    reg_set(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE2, 0);

    eez::scpi::resetContext();

#if OPTION_ETHERNET
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
    int err;
    if (!channel_dispatcher::setCouplingType(channel_dispatcher::COUPLING_TYPE_NONE, &err)) {
        event_queue::pushEvent(err);
    }
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

static profile::Parameters *loadAutoRecallProfile(int *location) {
    if (persist_conf::isProfileAutoRecallEnabled()) {
        *location = persist_conf::getProfileAutoRecallLocation();
        profile::Parameters *profile = profile::load(*location);
        if (profile) {
            bool outputEnabled = false;

            for (int i = 0; i < CH_NUM; ++i) {
                if (profile->channels[i].flags.output_enabled) {
                    outputEnabled = true;
                    break;
                }
            }

            if (outputEnabled) {
                bool disableOutputs = false;

                if (persist_conf::isForceDisablingAllOutputsOnPowerUpEnabled() || !g_bootTestSuccess) {
                    disableOutputs = true;
                } else {
                    if (*location != 0) {
                        profile::Parameters *defaultProfile = profile::load(0);
                        if (defaultProfile) {
                            if (profile->flags.couplingType != defaultProfile->flags.couplingType) {
                                disableOutputs = true;
                                event_queue::pushEvent(event_queue::EVENT_WARNING_AUTO_RECALL_VALUES_MISMATCH);
                            } else {
                                for (int i = 0; i < CH_NUM; ++i) {
                                    if (profile->channels[i].u_set != defaultProfile->channels[i].u_set ||
                                        profile->channels[i].i_set != defaultProfile->channels[i].i_set) {
                                        disableOutputs = true;
                                        event_queue::pushEvent( event_queue::EVENT_WARNING_AUTO_RECALL_VALUES_MISMATCH);
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

            return profile;
        }
    }

    return nullptr;
}

static bool autoRecall() {
    int location;
    profile::Parameters *profile = loadAutoRecallProfile(&location);
    if (profile) {
        if (!checkProfileModuleMatch(profile)) {
            event_queue::pushEvent(event_queue::EVENT_WARNING_AUTO_RECALL_MODULE_MISMATCH);
            return false;
        }

        if (profile::recallFromProfile(profile, location)) {
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

    g_bootTestSuccess &= testMaster();

    if (!autoRecall()) {
        psuReset();
    }

    // play beep if there is an error during boot procedure
    if (!g_bootTestSuccess) {
        sound::playBeep();
    }

    g_isBootedTime = micros();
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

    sound::playPowerUp(sound::PLAY_POWER_UP_CONDITION_NONE);

    g_rlState = persist_conf::devConf.flags.isFrontPanelLocked ? RL_STATE_REMOTE : RL_STATE_LOCAL;

#if OPTION_DISPLAY
    eez::gui::showWelcomePage();
#endif

    // reset channels
    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).reset();
    }

    // turn power on
    board::powerUp();
    g_powerIsUp = true;

    ontime::g_mcuCounter.start();
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex].moduleInfo->moduleType != MODULE_TYPE_NONE) {
            ontime::g_moduleCounters[slotIndex].start();
        }
    }

    // init channels
    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).init();
    }

    bool testSuccess = true;

    if (g_isBooted) {
    	testSuccess &= testMaster();
    }

    // test channels
    for (int i = 0; i < CH_NUM; ++i) {
        testSuccess &= Channel::get(i).test();
    }

    // turn on Power On (PON) bit of ESE register
    reg_set_esr_bits(ESR_PON);

    event_queue::pushEvent(event_queue::EVENT_INFO_POWER_UP);

    // play power up tune on success
    if (testSuccess) {
        sound::playPowerUp(sound::PLAY_POWER_UP_CONDITION_TEST_SUCCESSFUL);
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

    int err;
    if (!channel_dispatcher::setCouplingType(channel_dispatcher::COUPLING_TYPE_NONE, &err)) {
        event_queue::pushEvent(err);
    }

    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).onPowerDown();
    }

    board::powerDown();

    g_powerIsUp = false;

    ontime::g_mcuCounter.stop();
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex].moduleInfo->moduleType != MODULE_TYPE_NONE) {
            ontime::g_moduleCounters[slotIndex].stop();
        }
    }

    event_queue::pushEvent(event_queue::EVENT_INFO_POWER_DOWN);

#if OPTION_FAN
    aux_ps::fan::g_testResult = TEST_OK;
#endif

    io_pins::tick(micros());

    sound::playPowerDown();
}

bool isPowerUp() {
    return g_powerIsUp;
}

void changePowerState(bool up) {
    if (up == g_powerIsUp)
        return;

    // at least MIN_POWER_UP_DELAY seconds shall pass after last power down
    if (g_testPowerUpDelay) {
        if (millis() - g_powerDownTime < MIN_POWER_UP_DELAY * 1000)
            return;
        g_testPowerUpDelay = false;
    }

    if (osThreadGetId() != g_psuTaskHandle) {
        osMessagePut(g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_TYPE_CHANGE_POWER_STATE, up ? 1 : 0), osWaitForever);
        return;
    }

    if (up) {
        g_bootTestSuccess = true;

        // auto recall channels parameters from profile
        int location;
        profile::Parameters *profile = loadAutoRecallProfile(&location);

        if (!powerUp()) {
            return;
        }

        // auto recall channels parameters from profile
        if (profile) {
            if (!checkProfileModuleMatch(profile)) {
                event_queue::pushEvent(event_queue::EVENT_WARNING_AUTO_RECALL_MODULE_MISMATCH);
            } else {
                for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
                    memcpy(&temperature::sensors[i].prot_conf, profile->temp_prot + i, sizeof(temperature::ProtectionConfiguration));
                }

                profile::recallChannelsFromProfile(profile, location);
            }
        }

        profile::save();
    } else {
#if OPTION_DISPLAY
        eez::gui::showEnteringStandbyPage();
#endif
        g_powerIsUp = false;
        profile::save(true);
        g_powerIsUp = true;

        profile::enableSave(false);
        powerDown();
        profile::enableSave(true);

        g_testPowerUpDelay = true;
        g_powerDownTime = millis();
    }
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
        profile::save(true);
        g_powerIsUp = true;

        profile::enableSave(false);
        powerDown();
        profile::enableSave(true);
    }
}

////////////////////////////////////////////////////////////////////////////////

bool reset() {
    if (osThreadGetId() != g_psuTaskHandle) {
        osMessagePut(g_psuMessageQueueId, PSU_QUEUE_MESSAGE(PSU_QUEUE_MESSAGE_TYPE_RESET, 0), osWaitForever);
        return true;
    }

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

void tick_onTimeCounters(uint32_t tickCount) {
    ontime::g_mcuCounter.tick(tickCount);
    for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
        if (g_slots[slotIndex].moduleInfo->moduleType != MODULE_TYPE_NONE) {
            ontime::g_moduleCounters[slotIndex].tick(tickCount);
        }
    }
}

typedef void (*TickFunc)(uint32_t tickCount);
static TickFunc g_tickFuncs[] = {
    event_queue::tick,
    tick_onTimeCounters,
    temperature::tick,
#if OPTION_FAN
    aux_ps::fan::tick,
#endif
    datetime::tick,
    sound::tick,
    mcu::battery::tick,
    idle::tick
};
static const int NUM_TICK_FUNCS = sizeof(g_tickFuncs) / sizeof(TickFunc);
static int g_tickFuncIndex = 0;

void tick() {
    uint32_t tickCount = micros();

//#if OPTION_SD_CARD
    dlog::tick(tickCount);
//#endif

    for (int i = 0; i < CH_NUM; ++i) {
        Channel::get(i).tick(tickCount);
    }

    io_pins::tick(tickCount);

    trigger::tick(tickCount);

    list::tick(tickCount);

    g_tickFuncs[g_tickFuncIndex](tickCount);
    g_tickFuncIndex = (g_tickFuncIndex + 1) % NUM_TICK_FUNCS;

    if (g_diagCallback) {
        g_diagCallback();
        g_diagCallback = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

void setQuesBits(int bit_mask, bool on) {
    reg_set_ques_bit(bit_mask, on);
}

void setOperBits(int bit_mask, bool on) {
    reg_set_oper_bit(bit_mask, on);
}

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

#if defined(EEZ_PLATFORM_SIMULATOR)

namespace simulator {

static float g_temperature[temp_sensor::NUM_TEMP_SENSORS];
static bool g_pwrgood[CH_MAX];
static bool g_rpol[CH_MAX];
static bool g_cv[CH_MAX];
static bool g_cc[CH_MAX];
float g_uSet[CH_MAX];
float g_iSet[CH_MAX];

void init() {
    for (int i = 0; i < temp_sensor::NUM_TEMP_SENSORS; ++i) {
        g_temperature[i] = 25.0f;
    }

    for (int i = 0; i < CH_MAX; ++i) {
        g_pwrgood[i] = true;
        g_rpol[i] = false;
    }
}

void tick() {
    psu::tick();
}

void setTemperature(int sensor, float value) {
    g_temperature[sensor] = value;
}

float getTemperature(int sensor) {
    return g_temperature[sensor];
}

bool getPwrgood(int pin) {
    return g_pwrgood[pin];
}

void setPwrgood(int pin, bool on) {
    g_pwrgood[pin] = on;
}

bool getRPol(int pin) {
    return g_rpol[pin];
}

void setRPol(int pin, bool on) {
    g_rpol[pin] = on;
}

bool getCV(int pin) {
    return g_cv[pin];
}

void setCV(int pin, bool on) {
    g_cv[pin] = on;
}

bool getCC(int pin) {
    return g_cc[pin];
}

void setCC(int pin, bool on) {
    g_cc[pin] = on;
}

////////////////////////////////////////////////////////////////////////////////

void exit() {
}

} // namespace simulator

#endif

} // namespace psu
} // namespace eez
