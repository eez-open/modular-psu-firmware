/*
 * EEZ DIB PREL6
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <memory.h>

#if defined(EEZ_PLATFORM_STM32)
#include <spi.h>
#include <eez/platform/stm32/spi.h>
#endif

#include "eez/debug.h"
#include "eez/firmware.h"
#include "eez/system.h"
#include "eez/hmi.h"
#include "eez/gui/document.h"
#include "eez/modules/psu/psu.h"
#include "eez/modules/psu/event_queue.h"
#include "eez/modules/psu/profile.h"
#include "eez/modules/psu/timer.h"
#include "eez/modules/psu/gui/psu.h"
#include "eez/modules/psu/gui/labels_and_colors.h"
#include "eez/modules/psu/gui/animations.h"
#include "eez/modules/bp3c/comm.h"

#include "scpi/scpi.h"

#include "./dib-prel6.h"

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

namespace eez {
namespace dib_prel6 {

enum Prel6LowPriorityThreadMessage {
    THREAD_MESSAGE_SAVE_RELAY_CYCLES = THREAD_MESSAGE_MODULE_SPECIFIC
};

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

static const int NUM_RELAYS = 6;

static const uint32_t MIN_TO_MS = 60L * 1000L;

static const int NUM_REQUEST_RETRIES = 100;
static const int MAX_DMA_TRANSFER_ERRORS = 100;
static const uint32_t REFRESH_TIME_MS = 250;
static const uint32_t TIMEOUT_TIME_MS = 350;
static const uint32_t TIMEOUT_UNTIL_OUT_OF_SYNC_MS = 10000;

////////////////////////////////////////////////////////////////////////////////

enum Command {
	COMMAND_NONE       = 0x113B3759,
    COMMAND_GET_INFO   = 0x21EC18D4,
    COMMAND_GET_STATE  = 0x3C1D2EF4,
    COMMAND_SET_PARAMS = 0x4B723BFF
};

struct SetParams {
	uint8_t relayStates;
};

struct Request {
	uint32_t command;

    union {
    	SetParams setParams;
    };
};

struct Response {
	uint32_t command;

    union {
        struct {
            uint16_t moduleType;
            uint8_t firmwareMajorVersion;
            uint8_t firmwareMinorVersion;
            uint32_t idw0;
            uint32_t idw1;
            uint32_t idw2;
        } getInfo;

        struct {
            uint32_t tickCount;
        } getState;

        struct {
            uint8_t result; // 1 - success, 0 - failure
        } setParams;
    };
};

////////////////////////////////////////////////////////////////////////////////

#define BUFFER_SIZE 20

////////////////////////////////////////////////////////////////////////////////

struct Prel6Module : public Module {
public:
    TestResult testResult = TEST_NONE;

    bool powerDown = false;
    bool synchronized = false;

    uint32_t input[(BUFFER_SIZE + 3) / 4 + 1];
    uint32_t output[(BUFFER_SIZE + 3) / 4];

    bool spiReady = false;
    bool spiDmaTransferCompleted = false;
    int spiDmaTransferStatus;

    uint32_t lastTransferTime = 0;
	SetParams lastTransferredParams;
    bool forceTransferSetParams;

	struct CommandDef {
		uint32_t command;
		void (Prel6Module::*fillRequest)(Request &request);
		void (Prel6Module::*done)(Response &response, bool isSuccess);
	};

    static const CommandDef getInfo_command;
    static const CommandDef getState_command;
    static const CommandDef setParams_command;

    enum State {
        STATE_IDLE,

        STATE_WAIT_SLAVE_READY_BEFORE_REQUEST,
        STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST,

        STATE_WAIT_SLAVE_READY_BEFORE_RESPONSE,
        STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_RESPONSE
    };

    enum Event {
        EVENT_SLAVE_READY,
        EVENT_DMA_TRANSFER_COMPLETED,
        EVENT_DMA_TRANSFER_FAILED,
        EVENT_TIMEOUT
    };

    const CommandDef *currentCommand = nullptr;
    uint32_t refreshStartTime;
    State state;
    uint32_t lastStateTransitionTime;
    uint32_t lastRefreshTime;
    int retry;

    uint8_t relayStates = 0;

    static const size_t RELAY_LABEL_MAX_LENGTH = 10;
    char relayLabels[NUM_RELAYS][RELAY_LABEL_MAX_LENGTH + 1];

    // relay cycles counting    
    uint32_t relayCycles[NUM_RELAYS];
    uint32_t lastWrittenRelayCycles[NUM_RELAYS];
    Interval relayCyclesWriteInterval = WRITE_ONTIME_INTERVAL * MIN_TO_MS;

    Prel6Module() {
        assert(sizeof(Request) <= BUFFER_SIZE);
        assert(sizeof(Response) <= BUFFER_SIZE);

        moduleType = MODULE_TYPE_DIB_PREL6;
        moduleName = "PREL6";
        moduleBrand = "Envox";
        latestModuleRevision = MODULE_REVISION_R1B2;
        flashMethod = FLASH_METHOD_STM32_BOOTLOADER_UART;
#if defined(EEZ_PLATFORM_STM32)        
        spiBaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
        spiCrcCalculationEnable = true;
#else
        spiBaudRatePrescaler = 0;
        spiCrcCalculationEnable = false;
#endif
        numPowerChannels = 0;
        numOtherChannels = NUM_RELAYS;
        isResyncSupported = true;

        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));
    }

    void boot() override {
        Module::boot();

        loadRelayCylces();
    }

    Module *createModule() override {
        return new Prel6Module();
    }

    TestResult getTestResult() override {
        return testResult;
    }

    void initChannels() override {
        powerDown = false;

        if (!synchronized) {
            testResult = TEST_CONNECTING;

			executeCommand(&getInfo_command);

			if (!g_isBooted) {
				while (state != STATE_IDLE) {
                    WATCHDOG_RESET(WATCHDOG_LONG_OPERATION);
#if defined(EEZ_PLATFORM_STM32)
					if (HAL_GPIO_ReadPin(spi::IRQ_GPIO_Port[slotIndex], spi::IRQ_Pin[slotIndex]) == GPIO_PIN_RESET) {
                        osDelay(1);
                        if (HAL_GPIO_ReadPin(spi::IRQ_GPIO_Port[slotIndex], spi::IRQ_Pin[slotIndex]) == GPIO_PIN_RESET) {
						    spiReady = true;
                        }
					}
#endif
					tick();
                    osDelay(1);
				}
			}

            forceTransferSetParams = true;
        }
    }

    ////////////////////////////////////////

    void Command_GetInfo_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            auto &data = response.getInfo;

            if (data.moduleType == MODULE_TYPE_DIB_PREL6) {
                firmwareMajorVersion = data.firmwareMajorVersion;
                firmwareMinorVersion = data.firmwareMinorVersion;
                idw0 = data.idw0;
                idw1 = data.idw1;
                idw2 = data.idw2;

                firmwareVersionAcquired = true;

                synchronized = true;
                testResult = TEST_OK;
            } else {
                synchronized = false;
                testResult = TEST_FAILED;
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_FIRMWARE_MISMATCH + slotIndex);
            }
        } else {
            synchronized = false;
            if (firmwareInstalled) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
            }
            testResult = TEST_FAILED;
        }
    }

    ////////////////////////////////////////

    void Command_GetState_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            lastRefreshTime = refreshStartTime;
        }
    }

    ////////////////////////////////////////

	void fillSetParams(SetParams &params) {
		memset(&params, 0, sizeof(SetParams));
        params.relayStates = powerDown ? 0 : relayStates;
	}

    void Command_SetParams_FillRequest(Request &request) {
        fillSetParams(request.setParams);
    }

    void Command_SetParams_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            auto &data = response.setParams;
            if (data.result) {
                SetParams &params = ((Request *)output)->setParams;

                updateRelayCycles(lastTransferredParams.relayStates, params.relayStates);

                memcpy(&lastTransferredParams, &params, sizeof(SetParams));
            }
        }
    }

    ////////////////////////////////////////

	uint32_t getRefreshTimeMs() {
		return REFRESH_TIME_MS;
	}

    void executeCommand(const CommandDef *command) {
        currentCommand = command;
        retry = 0;
        setState(STATE_WAIT_SLAVE_READY_BEFORE_REQUEST);
	}

    bool startCommand() {
		Request &request = *(Request *)output;

		request.command = currentCommand->command;

		if (currentCommand->fillRequest) {
			(this->*currentCommand->fillRequest)(request);
		}
        spiReady = false;
        spiDmaTransferCompleted = false;
        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)output, (uint8_t *)input, BUFFER_SIZE);
        return status == bp3c::comm::TRANSFER_STATUS_OK;
    }

    bool getCommandResult() {
        Request &request = *(Request *)output;
        request.command = COMMAND_NONE;
        spiReady = false;
        spiDmaTransferCompleted = false;
        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)output, (uint8_t *)input, BUFFER_SIZE);
        return status == bp3c::comm::TRANSFER_STATUS_OK;
    }

    bool isCommandResponse() {
        Response &response = *(Response *)input;
        return response.command == (0x8000 | currentCommand->command);
    }

    void doRetry() {
    	bp3c::comm::abortTransfer(slotIndex);
        
        if (++retry < NUM_REQUEST_RETRIES) {
            // try again
            setState(STATE_WAIT_SLAVE_READY_BEFORE_REQUEST);
        } else {
            // give up
            doCommandDone(false);
        }
    }

    void doCommandDone(bool isSuccess) {
        if (isSuccess) {
            lastTransferTime = millis();
        }

		if (currentCommand->done) {
			Response &response = *(Response *)input;
			(this->*currentCommand->done)(response, isSuccess);
		}

        if (powerDown) {
            synchronized = false;
        }

		currentCommand = nullptr;
        setState(STATE_IDLE);
    }

    void setState(State newState) {
        state = newState;
        lastStateTransitionTime = millis();
    }

    void stateTransition(Event event) {
    	if (event == EVENT_DMA_TRANSFER_COMPLETED) {
            numCrcErrors = 0;
            numTransferErrors = 0;
    	}
 
        if (state == STATE_WAIT_SLAVE_READY_BEFORE_REQUEST) {
            if (event == EVENT_TIMEOUT) {
				doRetry();
            } else if (event == EVENT_SLAVE_READY) {
                if (startCommand()) {
                    setState(STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST);
                } else {
                    doRetry();
                }
            }
        } else if (state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST) {
            if (event == EVENT_TIMEOUT) {
                doRetry();
            } else if (event == EVENT_DMA_TRANSFER_COMPLETED) {
                setState(STATE_WAIT_SLAVE_READY_BEFORE_RESPONSE);
            } else if (event == EVENT_DMA_TRANSFER_FAILED) {
                doRetry();
            }
        } else if (state == STATE_WAIT_SLAVE_READY_BEFORE_RESPONSE) {
            if (event == EVENT_TIMEOUT) {
				doRetry();
            } else if (event == EVENT_SLAVE_READY) {
                if (getCommandResult()) {
                    setState(STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_RESPONSE);
                } else {
                    doRetry();
                }
            }
        } else if (state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_RESPONSE) {
            if (event == EVENT_TIMEOUT) {
                doRetry();
            } else if (event == EVENT_DMA_TRANSFER_COMPLETED) {
                if (isCommandResponse()) {
                    doCommandDone(true);
                } else {
                    doRetry();
                }
            } else if (event == EVENT_DMA_TRANSFER_FAILED) {
                doRetry();
            }
        } 
    }

    int numCrcErrors = 0;
    int numTransferErrors = 0;

    void reportDmaTransferFailed(int status) {
        if (status == bp3c::comm::TRANSFER_STATUS_CRC_ERROR) {
            numCrcErrors++;
            if (numCrcErrors >= MAX_DMA_TRANSFER_ERRORS) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_CRC_CHECK_ERROR + slotIndex);
                synchronized = false;
                testResult = TEST_FAILED;
            }
            //else if (numCrcErrors > 5) {
            //    DebugTrace("Slot %d CRC error no. %d\n", slotIndex + 1, numCrcErrors);
            //}
        } else {
            numTransferErrors++;
            if (numTransferErrors >= MAX_DMA_TRANSFER_ERRORS) {
                event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                synchronized = false;
                testResult = TEST_FAILED;
            }
            //else if (numTransferErrors > 5) {
            //    DebugTrace("Slot %d SPI transfer error %d no. %d\n", slotIndex + 1, status, numTransferErrors);
            //}
        }
    }

    void tick() override {
        if (relayCyclesWriteInterval.test()) {
            sendMessageToLowPriorityThread((LowPriorityThreadMessage)THREAD_MESSAGE_SAVE_RELAY_CYCLES, slotIndex);
        }

        if (currentCommand) {
            if (!synchronized && currentCommand->command != COMMAND_GET_INFO) {
                doCommandDone(false);
            } else {
                if (
                    state == STATE_WAIT_SLAVE_READY_BEFORE_REQUEST ||
                    state == STATE_WAIT_SLAVE_READY_BEFORE_RESPONSE
                ) {
                    #if defined(EEZ_PLATFORM_STM32)
                	if (spiReady) {
                		stateTransition(EVENT_SLAVE_READY);
                	}
                    #endif

                    #if defined(EEZ_PLATFORM_SIMULATOR)
                    stateTransition(EVENT_SLAVE_READY);
                    #endif
                } 
                
                if (
                    state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_REQUEST ||
                    state == STATE_WAIT_DMA_TRANSFER_COMPLETED_FOR_RESPONSE
                ) {
                
                    #if defined(EEZ_PLATFORM_STM32)
                    if (spiDmaTransferCompleted) {
                        if (spiDmaTransferStatus == bp3c::comm::TRANSFER_STATUS_OK) {
                            stateTransition(EVENT_DMA_TRANSFER_COMPLETED);
                        } else {
                            reportDmaTransferFailed(spiDmaTransferStatus);
                            stateTransition(EVENT_DMA_TRANSFER_FAILED);
                        }
                    }
                    #endif                

                    #if defined(EEZ_PLATFORM_SIMULATOR)
                    auto response = (Response *)input;

                    response->command = 0x8000 | currentCommand->command;

                    if (currentCommand->command == COMMAND_GET_INFO) {
						response->getInfo.moduleType = MODULE_TYPE_DIB_PREL6;
						response->getInfo.firmwareMajorVersion = 1;
                        response->getInfo.firmwareMinorVersion = 0;
                        response->getInfo.idw0 = 0;
                        response->getInfo.idw1 = 0;
                        response->getInfo.idw2 = 0;
                    }

                    stateTransition(EVENT_DMA_TRANSFER_COMPLETED);
                    #endif
                } 
                
                uint32_t tickCountMs = millis();
                if (tickCountMs - lastStateTransitionTime >= TIMEOUT_TIME_MS) {
                    stateTransition(EVENT_TIMEOUT);
                }
            }
        } else {
            if (synchronized) {
            	uint32_t tickCountMs = millis();
                if (tickCountMs - lastTransferTime >= TIMEOUT_UNTIL_OUT_OF_SYNC_MS) {
#ifdef EEZ_PLATFORM_STM32                
                    event_queue::pushEvent(event_queue::EVENT_ERROR_SLOT1_SYNC_ERROR + slotIndex);
                    synchronized = false;
                    testResult = TEST_FAILED;
#endif
                } 
                else if (tickCountMs - lastRefreshTime >= getRefreshTimeMs()) {
                    refreshStartTime = tickCountMs;
                    executeCommand(&getState_command);
                } else if (forceTransferSetParams) {
                    forceTransferSetParams = false;
                    executeCommand(&setParams_command);
                } else {
                    SetParams params;
                    fillSetParams(params);
                    if (memcmp(&params, &lastTransferredParams, sizeof(SetParams)) != 0) {
                        executeCommand(&setParams_command);
                    }
                }
            }
        }
    }

    void onSpiIrq() override {
        spiReady = true;
		// if (g_isBooted) {
		// 	stateTransition(EVENT_SLAVE_READY);
		// }
    }

    void onSpiDmaTransferCompleted(int status) override {
        //  if (g_isBooted) {
        //      if (status == bp3c::comm::TRANSFER_STATUS_OK) {
        //          stateTransition(EVENT_DMA_TRANSFER_COMPLETED);
        //      } else {
        //          reportDmaTransferFailed(status);
        //          stateTransition(EVENT_DMA_TRANSFER_FAILED);
        //      }
        //  } else {
             spiDmaTransferCompleted = true;
             spiDmaTransferStatus = status;
        //  }
    }

    void onPowerDown() override {
        powerDown = true;
        executeCommand(&setParams_command);
    }

    void resync() override {
        if (!synchronized) {
            executeCommand(&getInfo_command);
        }
    }

    void writeUnsavedData() override {
        saveRelayCycles();
    }

    void animatePageAppearance(int previousPageId, int activePageId) override {
        if (previousPageId == PAGE_ID_MAIN && activePageId == PAGE_ID_DIB_PREL6_SETTINGS) {
            psu::gui::animateSlideDown();
        } else if (previousPageId == PAGE_ID_DIB_PREL6_SETTINGS && activePageId == PAGE_ID_MAIN) {
            psu::gui::animateSlideUp();
        } else if (previousPageId == PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS && activePageId == PAGE_ID_DIB_PREL6_CHANNEL_LABELS) {
            psu::gui::animateSlideLeft();
        } else if (previousPageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS && activePageId == PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS) {
            psu::gui::animateSlideRight();
        }
    }

    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return psu::gui::isDefaultViewVertical() ? gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_VERT : gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_HORZ;
        }
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            return psu::gui::isDefaultViewVertical() ? gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_2COL_VERT : gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_DEF_2COL_HORZ;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_MAX;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MIN);
        return gui::PAGE_ID_DIB_PREL6_SLOT_VIEW_MIN;
    }

    int getSlotSettingsPageId() override {
        return PAGE_ID_DIB_PREL6_SETTINGS;
    }

    int getLabelsAndColorsPageId() override {
        return getTestResult() == TEST_OK ? PAGE_ID_DIB_PREL6_LABELS_AND_COLORS : PAGE_ID_NONE;
    }

    struct ProfileParameters : public Module::ProfileParameters {
        uint8_t relayStates;
        char relayLabels[NUM_RELAYS][RELAY_LABEL_MAX_LENGTH + 1];
    };

    void resetProfileToDefaults(uint8_t *buffer) override {
        Module::resetProfileToDefaults(buffer);
        auto parameters = (ProfileParameters *)buffer;
        parameters->relayStates = 0;
        memset(parameters->relayLabels, 0, sizeof(relayLabels));
    }

    void getProfileParameters(uint8_t *buffer) override {
        Module::getProfileParameters(buffer);
        assert(sizeof(ProfileParameters) < MAX_SLOT_PARAMETERS_SIZE);
        auto parameters = (ProfileParameters *)buffer;
        parameters->relayStates = relayStates;
        memcpy(parameters->relayLabels, relayLabels, sizeof(relayLabels));
    }
    
    void setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions) override {
        Module::setProfileParameters(buffer, mismatch, recallOptions);
        auto parameters = (ProfileParameters *)buffer;
        relayStates = parameters->relayStates;
        memcpy(relayLabels, parameters->relayLabels, sizeof(relayLabels));
    }
    
    bool writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) override {
        if (!Module::writeProfileProperties(ctx, buffer)) {
            return false;
        }
        auto parameters = (const ProfileParameters *)buffer;

        WRITE_PROPERTY("relayStates", parameters->relayStates);

        for (int i = 0; i < NUM_RELAYS; i++) {
            char propName[16];
            snprintf(propName, sizeof(propName), "channelLabel%d", i+1);
            WRITE_PROPERTY(propName, parameters->relayLabels[i]);
        }

        return true;
    }
    
    bool readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) override {
        if (Module::readProfileProperties(ctx, buffer)) {
            return true;
        }
        auto parameters = (ProfileParameters *)buffer;
        
        READ_PROPERTY("relayStates", parameters->relayStates);
		
        for (int i = 0; i < NUM_RELAYS; i++) {
            char propName[16];
            snprintf(propName, sizeof(propName), "channelLabel%d", i+1);
            READ_STRING_PROPERTY(propName, parameters->relayLabels[i], RELAY_LABEL_MAX_LENGTH);
        }

        return false;
    }

    void resetConfiguration() override {
        Module::resetConfiguration();
        relayStates = 0;
        memset(relayLabels, 0, sizeof(relayLabels));
    }

    size_t getChannelLabelMaxLength(int subchannelIndex) override {
        return RELAY_LABEL_MAX_LENGTH;
    }

    eez_err_t getChannelLabel(int subchannelIndex, const char *&label) override {
        if (subchannelIndex < 0 || subchannelIndex >= NUM_RELAYS) {
            return SCPI_ERROR_HARDWARE_MISSING;
        } 
        
        label = relayLabels[subchannelIndex];
        return SCPI_RES_OK;
    }

    const char *getChannelLabel(int subchannelIndex) override {
        const char *label;
        auto err = getChannelLabel(subchannelIndex, label);
        if (err == SCPI_RES_OK) {
            return label;
        } 
        return "";
    }

    const char *getDefaultChannelLabel(int subchannelIndex) override {
        static const char *g_relayLabels[NUM_RELAYS] = {
            "Relay #1",
            "Relay #2",
            "Relay #3",
            "Relay #4",
            "Relay #5",
            "Relay #6"
        };

		return g_relayLabels[subchannelIndex];
    }

    const char *getRelayLabelOrDefault(char subchannelIndex) {
        const char *label = getChannelLabel(subchannelIndex);
        if (*label) {
            return label;
        }
        return getDefaultChannelLabel(subchannelIndex);
    }

    eez_err_t setChannelLabel(int subchannelIndex, const char *label, int length) override {
        if (subchannelIndex < 0 || subchannelIndex >= NUM_RELAYS) {
            return SCPI_ERROR_HARDWARE_MISSING;
        } 

        if (length == -1) {
            length = strlen(label);
        }
        if (length > (int)RELAY_LABEL_MAX_LENGTH) {
            length = RELAY_LABEL_MAX_LENGTH;
        }

        stringCopy(relayLabels[subchannelIndex], length + 1, label);

        return SCPI_RES_OK;
    }

    bool isRouteOpen(int subchannelIndex, bool &isRouteOpen, int *err) override {
        if (subchannelIndex < 0 || subchannelIndex > 5) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        isRouteOpen = relayStates & (1 << subchannelIndex) ? false : true;
        return true;
    }

    bool routeOpen(ChannelList channelList, int *err) override {
        for (int i = 0; i < channelList.numChannels; i++) {
			int subchannelIndex = channelList.channels[i].subchannelIndex;
            if (subchannelIndex < 0 || subchannelIndex > 5) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }
        }

        for (int i = 0; i < channelList.numChannels; i++) {
			int subchannelIndex = channelList.channels[i].subchannelIndex;
			relayStates &= ~(1 << subchannelIndex);
        }

        return true;
    }
    
    bool routeClose(ChannelList channelList, int *err) override {
        for (int i = 0; i < channelList.numChannels; i++) {
			int subchannelIndex = channelList.channels[i].subchannelIndex;
            if (subchannelIndex < 0 || subchannelIndex > 5) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }
        }

        for (int i = 0; i < channelList.numChannels; i++) {
			int subchannelIndex = channelList.channels[i].subchannelIndex;
			relayStates |= (1 << subchannelIndex);
        }

        return true;
    }

    bool routeCloseExclusive(ChannelList channelList, int *err) override {
        if (channelList.numChannels != 1) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;            
        }

        int subchannelIndex = channelList.channels[0].subchannelIndex + 1;

        if (subchannelIndex >= 1 && subchannelIndex <= NUM_RELAYS) {
			relayStates = (1 << (subchannelIndex - 1));
        } else {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        return true;
    }

    void onLowPriorityThreadMessage(uint8_t type, uint32_t param) override {
        if (type == THREAD_MESSAGE_SAVE_RELAY_CYCLES) {
            saveRelayCycles();
        }
    }

    bool getRelayCycles(int subchannelIndex, uint32_t &relayCycles, int *err) override {
        if (subchannelIndex < 0 || subchannelIndex >= NUM_RELAYS) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

		relayCycles = this->relayCycles[subchannelIndex];

        return true;
    }

    void updateRelayCycles(uint8_t oldRelayStates, uint8_t newRelayStates) {
        for (int i = 0; i < NUM_RELAYS; i++) {
            if (!(oldRelayStates & (1 << i)) && (newRelayStates & (1 << i))) {
                relayCycles[i]++;
            }
        }
    }

    void loadRelayCylces() {
        for (int i = 0; i < NUM_RELAYS; i++) {
            relayCycles[i] = lastWrittenRelayCycles[i] = persist_conf::readCounter(slotIndex, i);
        }
    }

    void saveRelayCycles() {
        for (int i = 0; i < NUM_RELAYS; i++) {
            if (relayCycles[i] != lastWrittenRelayCycles[i]) {
                persist_conf::writeCounter(slotIndex, i, relayCycles[i]);
                lastWrittenRelayCycles[i] = relayCycles[i];
            }
        }
    }

	const char *getPinoutFile() override {
		return "prel6_pinout.jpg";
	}
};

////////////////////////////////////////////////////////////////////////////////

const Prel6Module::CommandDef Prel6Module::getInfo_command = {
	COMMAND_GET_INFO,
	nullptr,
	&Prel6Module::Command_GetInfo_Done
};

const Prel6Module::CommandDef Prel6Module::getState_command = {
	COMMAND_GET_STATE,
	nullptr,
	&Prel6Module::Command_GetState_Done
};

const Prel6Module::CommandDef Prel6Module::setParams_command = {
	COMMAND_SET_PARAMS,
	&Prel6Module::Command_SetParams_FillRequest,
	&Prel6Module::Command_SetParams_Done
};

////////////////////////////////////////////////////////////////////////////////

static Prel6Module g_prel6Module;
Module *g_module = &g_prel6Module;

} // namespace dib_prel6

namespace gui {

using namespace dib_prel6;

void data_dib_prel6_relays(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_RELAYS;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * NUM_RELAYS + value.getInt();
    } 
}

void data_dib_prel6_is_relay_1_or_6(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int subchannelIndex = cursor % NUM_RELAYS;
        value = subchannelIndex == 0 || subchannelIndex == 5;
    }
}

void data_dib_prel6_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / NUM_RELAYS;
        int subchannelIndex = cursor % NUM_RELAYS;
        value = ((Prel6Module *)g_slots[slotIndex])->relayStates & (1 << subchannelIndex) ? 1 : 0;
    }
}

void data_dib_prel6_relay_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / NUM_RELAYS;
        int subchannelIndex = cursor % NUM_RELAYS;
        if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
            const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, subchannelIndex);
            if (*label) {
                value = label;
            } else {
                value = g_slots[slotIndex]->getDefaultChannelLabel(subchannelIndex);
            }   
        } else {
            value = ((Prel6Module *)g_slots[slotIndex])->getRelayLabelOrDefault(subchannelIndex);
        }
    }
}

void data_dib_prel6_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = cursor / NUM_RELAYS;
		int subchannelIndex = cursor % NUM_RELAYS;
		value = Value(((Prel6Module *)g_slots[slotIndex])->relayCycles[subchannelIndex], VALUE_TYPE_UINT32);
	}
}

void action_dib_prel6_toggle_relay() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / NUM_RELAYS;
    int subchannelIndex = cursor % NUM_RELAYS;
    ((Prel6Module *)g_slots[slotIndex])->relayStates ^= 1 << subchannelIndex;
}

void action_dib_prel6_show_relay_labels() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    pushPage(PAGE_ID_DIB_PREL6_CHANNEL_LABELS);
}

void data_dib_prel6_relay_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_relayLabelLabels[NUM_RELAYS] = {
            "Relay #1 label:",
            "Relay #2 label:",
            "Relay #3 label:",
            "Relay #4 label:",
            "Relay #5 label:",
            "Relay #6 label:"
        };
        value = g_relayLabelLabels[cursor % NUM_RELAYS];
    }
}

void action_dib_prel6_change_relay_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / NUM_RELAYS;
    int subchannelIndex = cursor % NUM_RELAYS;
    LabelsAndColorsPage::editChannelLabel(slotIndex, subchannelIndex);
}

void action_dib_prel6_show_info() {
    pushPage(PAGE_ID_DIB_PREL6_INFO);
}

} // namespace gui

} // namespace eez
