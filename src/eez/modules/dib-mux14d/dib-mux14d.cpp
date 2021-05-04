/*
 * EEZ DIB MUX14D
 * Copyright (C) 2021-present, Envox d.o.o.
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

#include "./dib-mux14d.h"

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

namespace eez {
namespace dib_mux14d {

enum Mux14DLowPriorityThreadMessage {
    THREAD_MESSAGE_SAVE_RELAY_CYCLES = THREAD_MESSAGE_MODULE_SPECIFIC
};

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

static const int NUM_RELAYS = 7;

static const uint32_t MIN_TO_MS = 60L * 1000L;

static const int NUM_REQUEST_RETRIES = 10;
static const int MAX_DMA_TRANSFER_ERRORS = 100;
static const uint32_t REFRESH_TIME_MS = 1000;
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
	uint8_t p1RelayStates;
	uint8_t p2RelayStates;
    uint8_t adib1RelayState;
    uint8_t adib2RelayState;
	uint8_t extRelayState;
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
            uint8_t firmwareMajorVersion;
            uint8_t firmwareMinorVersion;
            uint32_t idw0;
            uint32_t idw1;
            uint32_t idw2;
        } getInfo;

        struct {
            float cjTemp;
        } getState;

        struct {
            uint8_t result; // 1 - success, 0 - failure
        } setParams;
    };
};

////////////////////////////////////////////////////////////////////////////////

#define BUFFER_SIZE 20

////////////////////////////////////////////////////////////////////////////////

struct Mux14DModule : public Module {
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
		void (Mux14DModule::*fillRequest)(Request &request);
		void (Mux14DModule::*done)(Response &response, bool isSuccess);
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

	uint8_t adib1RelayState = 0;
	uint8_t adib2RelayState = 0;
	uint8_t extRelayState = 0;
	uint8_t p1RelayStates = 0;
	uint8_t p2RelayStates = 0;

    static const size_t RELAY_LABEL_MAX_LENGTH = 10;
    char p1RelayLabels[NUM_RELAYS][RELAY_LABEL_MAX_LENGTH + 1];
	char p2RelayLabels[NUM_RELAYS][RELAY_LABEL_MAX_LENGTH + 1];

    // relay cycles counting    
	uint32_t adib1RelayCycles;
	uint32_t lastWrittenAdib1RelayCycles;

	uint32_t adib2RelayCycles;
	uint32_t lastWrittenAdib2RelayCycles;

	uint32_t extRelayCycles;
	uint32_t lastWrittenExtRelayCycles;
	
	uint32_t p1RelayCycles[NUM_RELAYS];
    uint32_t lastWrittenP1RelayCycles[NUM_RELAYS];

	uint32_t p2RelayCycles[NUM_RELAYS];
	uint32_t lastWrittenP2RelayCycles[NUM_RELAYS];

	Interval relayCyclesWriteInterval = WRITE_ONTIME_INTERVAL * MIN_TO_MS;

    float cjTemp = 0.0f;

	Mux14DModule() {
        assert(sizeof(Request) <= BUFFER_SIZE);
        assert(sizeof(Response) <= BUFFER_SIZE);

        moduleType = MODULE_TYPE_DIB_MUX14D;
        moduleName = "MUX14D";
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
        numOtherChannels = 3 + 2 *NUM_RELAYS;
        isResyncSupported = true;

        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));
    }

    void boot() override {
        Module::boot();

        loadRelayCylces();
    }

    Module *createModule() override {
        return new Mux14DModule();
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

            cjTemp = response.getState.cjTemp;
        }
    }

    ////////////////////////////////////////

	void fillSetParams(SetParams &params) {
		memset(&params, 0, sizeof(SetParams));
        params.p1RelayStates = powerDown ? 0 : p1RelayStates;
		params.p2RelayStates = powerDown ? 0 : p2RelayStates;
        params.adib1RelayState = powerDown ? 0 : adib1RelayState;
        params.adib2RelayState = powerDown ? 0 : adib2RelayState;
		params.extRelayState = powerDown ? 0 : extRelayState;
	}

    void Command_SetParams_FillRequest(Request &request) {
        fillSetParams(request.setParams);
    }

    void Command_SetParams_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            auto &data = response.setParams;
            if (data.result) {
                SetParams &params = ((Request *)output)->setParams;

                updateRelayCycles(lastTransferredParams, params);

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
                } else if (tickCountMs - lastRefreshTime >= getRefreshTimeMs()) {
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

    void onSpiIrq() {
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
        if (previousPageId == PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS && activePageId == PAGE_ID_DIB_MUX14D_CHANNEL_LABELS) {
            psu::gui::animateSettingsSlideLeft(true);
        } else if (previousPageId == PAGE_ID_DIB_MIO168_CHANNEL_LABELS && activePageId == PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS) {
            psu::gui::animateSettingsSlideRight(true);
        }
    }

    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return psu::gui::isDefaultViewVertical() ? gui::PAGE_ID_DIB_MUX14D_SLOT_VIEW_DEF : gui::PAGE_ID_SLOT_DEF_HORZ_EMPTY;
        }
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            return psu::gui::isDefaultViewVertical() ? gui::PAGE_ID_DIB_MUX14D_SLOT_VIEW_DEF_2COL : gui::PAGE_ID_SLOT_DEF_HORZ_EMPTY;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return gui::PAGE_ID_DIB_MUX14D_SLOT_VIEW_MAX;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MIN) {
            return gui::PAGE_ID_DIB_MUX14D_SLOT_VIEW_MIN;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MICRO);
        return gui::PAGE_ID_DIB_MUX14D_SLOT_VIEW_MICRO;
    }

    int getSlotSettingsPageId() override {
        return PAGE_ID_DIB_MUX14D_SETTINGS;
    }

    int getLabelsAndColorsPageId() override {
        return PAGE_ID_DIB_MUX14D_LABELS_AND_COLORS;
    }

    struct ProfileParameters : public Module::ProfileParameters {
        uint8_t p1RelayStates;
        char p1RelayLabels[NUM_RELAYS][RELAY_LABEL_MAX_LENGTH + 1];
	
		uint8_t p2RelayStates;
		char p2RelayLabels[NUM_RELAYS][RELAY_LABEL_MAX_LENGTH + 1];

        uint8_t adib1RelayState;
        uint8_t adib2RelayState;

		uint8_t extRelayState;
	};

    void resetProfileToDefaults(uint8_t *buffer) override {
        Module::resetProfileToDefaults(buffer);

		auto parameters = (ProfileParameters *)buffer;

		parameters->p1RelayStates = p1RelayStates;
        memset(parameters->p1RelayLabels, 0, sizeof(p1RelayLabels));

		parameters->p2RelayStates = p2RelayStates;
		memset(parameters->p2RelayLabels, 0, sizeof(p2RelayLabels));

        parameters->adib1RelayState = adib1RelayState;
        parameters->adib2RelayState = adib2RelayState;

		parameters->extRelayState = extRelayState;
	}

    void getProfileParameters(uint8_t *buffer) override {
        Module::getProfileParameters(buffer);

		assert(sizeof(ProfileParameters) < MAX_CHANNEL_PARAMETERS_SIZE);
        
		auto parameters = (ProfileParameters *)buffer;
        
		parameters->p1RelayStates = p1RelayStates;
        memcpy(parameters->p1RelayLabels, p1RelayLabels, sizeof(p1RelayLabels));

		parameters->p2RelayStates = p2RelayStates;
		memcpy(parameters->p2RelayLabels, p2RelayLabels, sizeof(p2RelayLabels));

        parameters->adib1RelayState = adib1RelayState;
        parameters->adib2RelayState = adib2RelayState;

		parameters->extRelayState = extRelayState;
	}
    
    void setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions) override {
        Module::setProfileParameters(buffer, mismatch, recallOptions);
        
		auto parameters = (ProfileParameters *)buffer;
        
		p1RelayStates = parameters->p1RelayStates;
        memcpy(p1RelayLabels, parameters->p1RelayLabels, sizeof(p1RelayLabels));

		p2RelayStates = parameters->p2RelayStates;
		memcpy(p2RelayLabels, parameters->p2RelayLabels, sizeof(p2RelayLabels));

        adib1RelayState = parameters->adib1RelayState;
        adib2RelayState = parameters->adib2RelayState;

		extRelayState = parameters->extRelayState;
	}
    
    bool writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) override {
        if (!Module::writeProfileProperties(ctx, buffer)) {
            return false;
        }
        auto parameters = (const ProfileParameters *)buffer;

        WRITE_PROPERTY("p1RelayStates", parameters->p1RelayStates);

        for (int i = 0; i < NUM_RELAYS; i++) {
            char propName[16];
            snprintf(propName, sizeof(propName), "p1ChannelLabel%d", i+1);
            WRITE_PROPERTY(propName, parameters->p1RelayLabels[i]);
        }

		WRITE_PROPERTY("p2RelayStates", parameters->p2RelayStates);

		for (int i = 0; i < NUM_RELAYS; i++) {
			char propName[16];
			snprintf(propName, sizeof(propName), "p2ChannelLabel%d", i + 1);
			WRITE_PROPERTY(propName, parameters->p2RelayLabels[i]);
		}

        WRITE_PROPERTY("adib1RelayState", parameters->adib1RelayState);
        WRITE_PROPERTY("adib2RelayState", parameters->adib2RelayState);
		
		WRITE_PROPERTY("extRelayState", parameters->extRelayState);

		return true;
    }
    
    bool readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) override {
        if (Module::readProfileProperties(ctx, buffer)) {
            return true;
        }
        auto parameters = (ProfileParameters *)buffer;
        
        READ_PROPERTY("p1RelayStates", parameters->p1RelayStates);
		
        for (int i = 0; i < NUM_RELAYS; i++) {
            char propName[16];
            snprintf(propName, sizeof(propName), "p1ChannelLabel%d", i+1);
            READ_STRING_PROPERTY(propName, parameters->p1RelayLabels[i], RELAY_LABEL_MAX_LENGTH);
        }

		READ_PROPERTY("p2RelayStates", parameters->p2RelayStates);

		for (int i = 0; i < NUM_RELAYS; i++) {
			char propName[16];
			snprintf(propName, sizeof(propName), "p2ChannelLabel%d", i + 1);
			READ_STRING_PROPERTY(propName, parameters->p2RelayLabels[i], RELAY_LABEL_MAX_LENGTH);
		}

        READ_PROPERTY("adib1RelayState", parameters->adib1RelayState);
        READ_PROPERTY("adib2RelayState", parameters->adib2RelayState);

		READ_PROPERTY("extRelayState", parameters->extRelayState);
		
		return false;
    }

    void resetConfiguration() {
        Module::resetConfiguration();

		p1RelayStates = 0;
        memset(p1RelayLabels, 0, sizeof(p1RelayLabels));

		p2RelayStates = 0;
		memset(p2RelayLabels, 0, sizeof(p2RelayLabels));

        adib1RelayState = 0;
        adib2RelayState = 0;

		extRelayState = 0;
	}

    size_t getChannelLabelMaxLength(int subchannelIndex) override {
        return RELAY_LABEL_MAX_LENGTH;
    }

    eez_err_t getChannelLabel(int subchannelIndex, const char *&label) override {
		subchannelIndex++;

		if (subchannelIndex >= 11 && subchannelIndex <= 17) {
			label = p1RelayLabels[subchannelIndex - 11];
			return SCPI_RES_OK;
		}

		if (subchannelIndex >= 21 && subchannelIndex <= 27) {
			label = p2RelayLabels[subchannelIndex - 21];
			return SCPI_RES_OK;
		}
        
		return SCPI_ERROR_HARDWARE_MISSING;
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
        static const char *g_p1RelayLabels[NUM_RELAYS] = {
            "P1.1",
            "P1.2",
            "P1.3",
            "P1.4",
            "P1.5",
            "P1.6",
			"P1.7"
        };

        static const char *g_p2RelayLabels[NUM_RELAYS] = {
            "P2.1",
            "P2.2",
            "P2.3",
            "P2.4",
            "P2.5",
            "P2.6",
			"P2.7"
        };

        subchannelIndex++;

        if (subchannelIndex >= 11 && subchannelIndex <= 17) {
		    return g_p1RelayLabels[subchannelIndex - 11];
        }

        if (subchannelIndex >= 21 && subchannelIndex <= 27) {
		    return g_p2RelayLabels[subchannelIndex - 21];
        }

		return nullptr;
    }

    const char *getRelayLabelOrDefault(char subchannelIndex) {
        const char *label = getChannelLabel(subchannelIndex);
        if (*label) {
            return label;
        }
        return getDefaultChannelLabel(subchannelIndex);
    }

    eez_err_t setChannelLabel(int subchannelIndex, const char *label, int length) override {
        subchannelIndex++;

        if (length == -1) {
            length = strlen(label);
        }
        if (length > (int)RELAY_LABEL_MAX_LENGTH) {
            length = RELAY_LABEL_MAX_LENGTH;
        }

        if (subchannelIndex >= 11 && subchannelIndex <= 17) {
            stringCopy(p1RelayLabels[subchannelIndex - 11], length + 1, label);
            return SCPI_RES_OK;
        }

        if (subchannelIndex >= 21 && subchannelIndex <= 27) {
            stringCopy(p2RelayLabels[subchannelIndex - 21], length + 1, label);
            return SCPI_RES_OK;
        } 

        return SCPI_ERROR_HARDWARE_MISSING;
    }

    bool isValidSubchannelIndex(int subchannelIndex) override {
        subchannelIndex++;
        return subchannelIndex == 1 || subchannelIndex == 2 || subchannelIndex == 3 ||
            (subchannelIndex >= 11 && subchannelIndex <= 17) ||
            (subchannelIndex >= 21 && subchannelIndex <= 27);
    }

    int getSubchannelIndexFromRelativeChannelIndex(int relativeChannelIndex) override {
        int subchannelIndex;
        if (relativeChannelIndex == 0) {
            subchannelIndex = 1; // P1 "activation" ralay, i.e. P1_C
        } else if (relativeChannelIndex == 1) {
            subchannelIndex = 2; // P2 "activation" ralay, i.e. P2_C
        } else if (relativeChannelIndex == 2) {
            subchannelIndex = 3; // EXT relay
        } else if (relativeChannelIndex >= 3 && relativeChannelIndex < 10) {
            subchannelIndex = 11 + (relativeChannelIndex - 3); // P1_1 : P1_7
        } else {
            subchannelIndex = 21 + (relativeChannelIndex - 10); // P2_1 : P2_7
        }
        return subchannelIndex - 1;
    }

    bool isRouteOpen(int subchannelIndex, bool &isRouteOpen, int *err) override {
        subchannelIndex++;

        if (subchannelIndex == 1) {
            isRouteOpen = adib1RelayState  ? false : true;
            return true;
        }

        if (subchannelIndex == 2) {
            isRouteOpen = adib2RelayState ? false : true;
            return true;
        }

        if (subchannelIndex == 3) {
            isRouteOpen = extRelayState ? false : true;
            return true;
        }

        if (subchannelIndex >= 11 && subchannelIndex <= 17) {
            isRouteOpen = p1RelayStates & (1 << (subchannelIndex - 11)) ? false : true;
            return true;
        }

        if (subchannelIndex >= 21 && subchannelIndex <= 27) {
            isRouteOpen = p2RelayStates & (1 << (subchannelIndex - 21)) ? false : true;
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
        }
        return false;
    }

    bool routeOpen(ChannelList channelList, int *err) override {
        for (int i = 0; i < channelList.numChannels; i++) {
			int subchannelIndex = channelList.channels[i].subchannelIndex;
            if (!isValidSubchannelIndex(subchannelIndex)) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }
        }

        for (int i = 0; i < channelList.numChannels; i++) {
			int subchannelIndex = channelList.channels[i].subchannelIndex;

            subchannelIndex++;

            if (subchannelIndex == 1) {
                adib1RelayState = 0;
            } else if (subchannelIndex == 2) {
                adib2RelayState = 0;
            } else if (subchannelIndex == 3) {
                extRelayState = 0;
            } else if (subchannelIndex >= 11 && subchannelIndex <= 17) {
			    p1RelayStates &= ~(1 << (subchannelIndex - 11));
            } else if (subchannelIndex >= 21 && subchannelIndex <= 27) {
			    p2RelayStates &= ~(1 << (subchannelIndex - 21));
            }            
        }

        return true;
    }
    
    bool routeClose(ChannelList channelList, int *err) override {
        for (int i = 0; i < channelList.numChannels; i++) {
			int subchannelIndex = channelList.channels[i].subchannelIndex;
            if (!isValidSubchannelIndex(subchannelIndex)) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }
        }

        for (int i = 0; i < channelList.numChannels; i++) {
			int subchannelIndex = channelList.channels[i].subchannelIndex;

            subchannelIndex++;

            if (subchannelIndex == 1) {
                adib1RelayState = 1;
            } else if (subchannelIndex == 2) {
                adib2RelayState = 1;
            } else if (subchannelIndex == 3) {
                extRelayState = 1;
            } else if (subchannelIndex >= 11 && subchannelIndex <= 17) {
			    p1RelayStates |= 1 << (subchannelIndex - 11);
            } else if (subchannelIndex >= 21 && subchannelIndex <= 27) {
			    p2RelayStates |= 1 << (subchannelIndex - 21);
            }            
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
        if (subchannelIndex >= 11 && subchannelIndex <= 17) {
            p1RelayStates = 1 << (subchannelIndex - 11);
        } else if (subchannelIndex >= 21 && subchannelIndex <= 27) {
            p2RelayStates = 1 << (subchannelIndex - 21);
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
        if (!isValidSubchannelIndex(subchannelIndex)) {
            if (err) {
                *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
            }
            return false;
        }

        subchannelIndex++;

        if (subchannelIndex == 1) {
		    relayCycles = adib1RelayCycles;
        } else if (subchannelIndex == 2) {
		    relayCycles = adib2RelayCycles;
        } else if (subchannelIndex == 3) {
		    relayCycles = extRelayCycles;
        } else if (subchannelIndex >= 11 && subchannelIndex <= 17) {
		    relayCycles = p1RelayCycles[subchannelIndex - 11];
        } else if (subchannelIndex >= 21 && subchannelIndex <= 27) {
		    relayCycles = p1RelayCycles[subchannelIndex - 21];
        }

        return true;
    }

    void updateRelayCycles(SetParams oldParams, SetParams &newParams) {
        for (int i = 0; i < NUM_RELAYS; i++) {
            if (!(oldParams.p1RelayStates & (1 << i)) && (newParams.p1RelayStates & (1 << i))) {
                p1RelayCycles[i]++;
            }
		
			if (!(oldParams.p2RelayStates & (1 << i)) && (newParams.p2RelayStates & (1 << i))) {
				p2RelayCycles[i]++;
			}
		}

		if (!oldParams.adib1RelayState && newParams.adib1RelayState) {
			adib1RelayCycles++;
		}

		if (!oldParams.adib2RelayState && newParams.adib2RelayState) {
			adib2RelayCycles++;
		}

		if (!oldParams.extRelayState && newParams.extRelayState) {
			extRelayCycles++;
		}
	}

    void loadRelayCylces() {
        for (int relativeChannelIndex = 0; relativeChannelIndex < 3 + 2 * NUM_RELAYS; relativeChannelIndex++) {
            auto relayCycles = persist_conf::readCounter(slotIndex, relativeChannelIndex);

			int subchannelIndex = getSubchannelIndexFromRelativeChannelIndex(relativeChannelIndex);
            subchannelIndex++;

            if (subchannelIndex == 1) {
                adib1RelayCycles = relayCycles;
            } else if (subchannelIndex == 2) {
                adib2RelayCycles = relayCycles;
            } else if (subchannelIndex == 3) {
                extRelayCycles = relayCycles;
            } else if (subchannelIndex >= 11 && subchannelIndex <= 17) {
                p1RelayCycles[subchannelIndex - 11] = relayCycles;
            } else if (subchannelIndex >= 21 && subchannelIndex <= 27) {
                p2RelayCycles[subchannelIndex - 21] = relayCycles;
            }
        }
    }

    void saveRelayCycles() {
        for (int relativeChannelIndex = 0; relativeChannelIndex < 3 + 2 * NUM_RELAYS; relativeChannelIndex++) {
			int subchannelIndex = getSubchannelIndexFromRelativeChannelIndex(relativeChannelIndex);
            subchannelIndex++;

            if (subchannelIndex == 1) {
                if (adib1RelayCycles != lastWrittenAdib1RelayCycles) {
                    persist_conf::writeCounter(slotIndex, relativeChannelIndex, adib1RelayCycles);
                }
            } else if (subchannelIndex == 2) {
                if (adib2RelayCycles != lastWrittenAdib2RelayCycles) {
                    persist_conf::writeCounter(slotIndex, relativeChannelIndex, adib2RelayCycles);
                }
            } else if (subchannelIndex == 3) {
                if (extRelayCycles != lastWrittenExtRelayCycles) {
                    persist_conf::writeCounter(slotIndex, relativeChannelIndex, extRelayCycles);
                }
            } else if (subchannelIndex >= 11 && subchannelIndex <= 17) {
                if (p1RelayCycles[subchannelIndex - 11] != lastWrittenP1RelayCycles[subchannelIndex - 11]) {
                    persist_conf::writeCounter(slotIndex, relativeChannelIndex, p1RelayCycles[subchannelIndex - 11]);
                }
            } else if (subchannelIndex >= 21 && subchannelIndex <= 27) {
                if (p2RelayCycles[subchannelIndex - 21] != lastWrittenP2RelayCycles[subchannelIndex - 21]) {
                    persist_conf::writeCounter(slotIndex, relativeChannelIndex, p2RelayCycles[subchannelIndex - 21]);
                }
            }
        }
    }

	const char *getPinoutFile() override {
		return "mux14d_pinout.jpg";
	}
};

////////////////////////////////////////////////////////////////////////////////

const Mux14DModule::CommandDef Mux14DModule::getInfo_command = {
	COMMAND_GET_INFO,
	nullptr,
	&Mux14DModule::Command_GetInfo_Done
};

const Mux14DModule::CommandDef Mux14DModule::getState_command = {
	COMMAND_GET_STATE,
	nullptr,
	&Mux14DModule::Command_GetState_Done
};

const Mux14DModule::CommandDef Mux14DModule::setParams_command = {
	COMMAND_SET_PARAMS,
	&Mux14DModule::Command_SetParams_FillRequest,
	&Mux14DModule::Command_SetParams_Done
};

////////////////////////////////////////////////////////////////////////////////

static Mux14DModule g_mux14DModule;
Module *g_module = &g_mux14DModule;

} // namespace dib_mux14d

namespace gui {

using namespace dib_mux14d;

void data_dib_mux14d_p1_relays(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_RELAYS;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * NUM_RELAYS + value.getInt();
    } 
}

void data_dib_mux14d_p1_relay_is_first(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int subchannelIndex = cursor % NUM_RELAYS;
		value = subchannelIndex == 0;
	}
}

void data_dib_mux14d_p1_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / NUM_RELAYS;
        int subchannelIndex = cursor % NUM_RELAYS;
        value = ((Mux14DModule *)g_slots[slotIndex])->p1RelayStates & (1 << subchannelIndex) ? 1 : 0;
    }
}

void data_dib_mux14d_p1_relay_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / NUM_RELAYS;
        int subchannelIndex = 11 + cursor % NUM_RELAYS - 1;
        if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
            const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, subchannelIndex);
            if (*label) {
                value = label;
            } else {
                value = g_slots[slotIndex]->getDefaultChannelLabel(subchannelIndex);
            }   
        } else {
            value = ((Mux14DModule *)g_slots[slotIndex])->getRelayLabelOrDefault(subchannelIndex);
        }
    }
}

void data_dib_mux14d_p1_relay_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_relayLabelLabels[NUM_RELAYS] = {
            "Relay #1 label:",
            "Relay #2 label:",
            "Relay #3 label:",
            "Relay #4 label:",
            "Relay #5 label:",
            "Relay #6 label:",
			"Relay #7 label:"
        };
        value = g_relayLabelLabels[cursor % NUM_RELAYS];
    }
}

void action_dib_mux14d_change_p1_relay_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / NUM_RELAYS;
    int subchannelIndex = 11 + cursor % NUM_RELAYS - 1;
    LabelsAndColorsPage::editChannelLabel(slotIndex, subchannelIndex);
}

void data_dib_mux14d_p1_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = cursor / NUM_RELAYS;
		int subchannelIndex = cursor % NUM_RELAYS;
		value = Value(((Mux14DModule *)g_slots[slotIndex])->p1RelayCycles[subchannelIndex], VALUE_TYPE_UINT32);
	}
}

void action_dib_mux14d_toggle_p1_relay() {
	int cursor = getFoundWidgetAtDown().cursor;
	int slotIndex = cursor / NUM_RELAYS;
	int subchannelIndex = cursor % NUM_RELAYS;
	((Mux14DModule *)g_slots[slotIndex])->p1RelayStates ^= 1 << subchannelIndex;
}

void data_dib_mux14d_p2_relays(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_COUNT) {
		value = NUM_RELAYS;
	} else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
		value = hmi::g_selectedSlotIndex * NUM_RELAYS + value.getInt();
	}
}

void data_dib_mux14d_p2_relay_is_first(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int subchannelIndex = cursor % NUM_RELAYS;
		value = subchannelIndex == 0;
	}
}

void data_dib_mux14d_p2_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = cursor / NUM_RELAYS;
		int subchannelIndex = cursor % NUM_RELAYS;
		value = ((Mux14DModule *)g_slots[slotIndex])->p2RelayStates & (1 << subchannelIndex) ? 1 : 0;
	}
}

void data_dib_mux14d_p2_relay_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = cursor / NUM_RELAYS;
		int subchannelIndex = 21 + cursor % NUM_RELAYS - 1;
		if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
			const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, subchannelIndex);
			if (*label) {
				value = label;
			} else {
				value = g_slots[slotIndex]->getDefaultChannelLabel(subchannelIndex);
			}
		} else {
			value = ((Mux14DModule *)g_slots[slotIndex])->getRelayLabelOrDefault(subchannelIndex);
		}
	}
}

void data_dib_mux14d_p2_relay_label_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        static const char *g_relayLabelLabels[NUM_RELAYS] = {
            "Relay #1 label:",
            "Relay #2 label:",
            "Relay #3 label:",
            "Relay #4 label:",
            "Relay #5 label:",
            "Relay #6 label:",
			"Relay #7 label:"
        };
        value = g_relayLabelLabels[cursor % NUM_RELAYS];
    }
}

void action_dib_mux14d_change_p2_relay_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / NUM_RELAYS;
    int subchannelIndex = 21 + cursor % NUM_RELAYS - 1;
    LabelsAndColorsPage::editChannelLabel(slotIndex, subchannelIndex);
}

void data_dib_mux14d_p2_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = cursor / NUM_RELAYS;
		int subchannelIndex = cursor % NUM_RELAYS;
		value = Value(((Mux14DModule *)g_slots[slotIndex])->p2RelayCycles[subchannelIndex], VALUE_TYPE_UINT32);
	}
}

void action_dib_mux14d_toggle_p2_relay() {
	int cursor = getFoundWidgetAtDown().cursor;
	int slotIndex = cursor / NUM_RELAYS;
	int subchannelIndex = cursor % NUM_RELAYS;
	((Mux14DModule *)g_slots[slotIndex])->p2RelayStates ^= 1 << subchannelIndex;
}

void data_dib_mux14d_adib1_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = hmi::g_selectedSlotIndex;
		value = ((Mux14DModule *)g_slots[slotIndex])->adib1RelayState ? 1 : 0;
	}
}

void data_dib_mux14d_adib1_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = hmi::g_selectedSlotIndex;
		value = Value(((Mux14DModule *)g_slots[slotIndex])->adib1RelayCycles, VALUE_TYPE_UINT32);
	}
}

void action_dib_mux14d_toggle_adib1_relay() {
	int slotIndex = getFoundWidgetAtDown().cursor;
	((Mux14DModule *)g_slots[slotIndex])->adib1RelayState = !((Mux14DModule *)g_slots[slotIndex])->adib1RelayState;
}

void data_dib_mux14d_adib2_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = hmi::g_selectedSlotIndex;
		value = ((Mux14DModule *)g_slots[slotIndex])->adib2RelayState ? 1 : 0;
	}
}

void data_dib_mux14d_adib2_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = hmi::g_selectedSlotIndex;
		value = Value(((Mux14DModule *)g_slots[slotIndex])->adib2RelayCycles, VALUE_TYPE_UINT32);
	}
}

void action_dib_mux14d_toggle_adib2_relay() {
	int slotIndex = getFoundWidgetAtDown().cursor;
	((Mux14DModule *)g_slots[slotIndex])->adib2RelayState = !((Mux14DModule *)g_slots[slotIndex])->adib2RelayState;
}

void data_dib_mux14d_ext_relay_is_on(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = hmi::g_selectedSlotIndex;
		value = ((Mux14DModule *)g_slots[slotIndex])->extRelayState ? 1 : 0;
	}
}

void data_dib_mux14d_ext_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = hmi::g_selectedSlotIndex;
		value = Value(((Mux14DModule *)g_slots[slotIndex])->extRelayCycles, VALUE_TYPE_UINT32);
	}
}

void action_dib_mux14d_toggle_ext_relay() {
	int slotIndex = getFoundWidgetAtDown().cursor;
	((Mux14DModule *)g_slots[slotIndex])->extRelayState = !((Mux14DModule *)g_slots[slotIndex])->extRelayState;
}

void data_dib_mux14d_cj_temp(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = hmi::g_selectedSlotIndex;
		value = MakeValue(((Mux14DModule *)g_slots[slotIndex])->cjTemp, UNIT_CELSIUS);
	}
}

void action_dib_mux14d_show_relay_labels() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    pushPage(PAGE_ID_DIB_MUX14D_CHANNEL_LABELS);
}

void action_dib_mux14d_show_info() {
    pushPage(PAGE_ID_DIB_PREL6_INFO);
}

} // namespace dib_mux14d
} // namespace eez
