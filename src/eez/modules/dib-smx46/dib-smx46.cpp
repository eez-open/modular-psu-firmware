/*
 * EEZ DIB SMX46
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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(EEZ_PLATFORM_STM32)
#include <spi.h>
#include <eez/platform/stm32/spi.h>
#endif

#include "eez/debug.h"
#include "eez/firmware.h"
#include <eez/system.h>
#include "eez/hmi.h"
#include "eez/util.h"
#include "eez/gui/document.h"
#include <eez/modules/psu/timer.h>
#include "eez/modules/psu/psu.h"
#include "eez/modules/psu/profile.h"
#include "eez/modules/psu/event_queue.h"
#include "eez/modules/psu/calibration.h"
#include "eez/modules/psu/trigger.h"
#include "eez/modules/psu/gui/psu.h"
#include "eez/modules/psu/gui/animations.h"
#include "eez/modules/psu/gui/keypad.h"
#include "eez/modules/psu/gui/labels_and_colors.h"
#include "eez/modules/psu/gui/edit_mode.h"
#include "eez/modules/bp3c/comm.h"
#include "eez/modules/psu/gui/edit_mode.h"

#include "eez/function_generator.h"

#include "scpi/scpi.h"

#include "./dib-smx46.h"

using namespace eez::psu;
using namespace eez::psu::gui;
using namespace eez::gui;

namespace eez {
namespace dib_smx46 {

enum Smx46LowPriorityThreadMessage {
    THREAD_MESSAGE_SAVE_RELAY_CYCLES = THREAD_MESSAGE_MODULE_SPECIFIC
};

static const uint16_t MODULE_REVISION_R1B2  = 0x0102;

static float U_CAL_POINTS[2] = { 1.0f, 9.0f };
static float DAC_MIN = 0.0f;
static float DAC_MAX = 10.0f;
static float DAC_RESOLUTION = 0.01f;
static float DAC_ENCODER_STEP_VALUES[] = { 0.01f, 0.1f, 0.2f, 0.5f  };

static const int NUM_COLUMNS = 6;
static const int NUM_ROWS = 4;

static const uint32_t MIN_TO_MS = 60L * 1000L;

////////////////////////////////////////////////////////////////////////////////

static const int NUM_REQUEST_RETRIES = 10;
static const int MAX_DMA_TRANSFER_ERRORS = 10;
static const uint32_t REFRESH_TIME_MS = 250;
static const uint32_t TIMEOUT_TIME_MS = 350;
static const uint32_t TIMEOUT_UNTIL_OUT_OF_SYNC_MS = 1000;

enum Command {
	COMMAND_NONE       = 0x113B3759,
    COMMAND_GET_INFO   = 0x21EC18D4,
    COMMAND_GET_STATE  = 0x3C1D2EF4,
    COMMAND_SET_PARAMS = 0x4B723BFF
};

using function_generator::Waveform;

struct WaveformParameters {
	Waveform waveform;
	float frequency;
	float phaseShift;
	float amplitude;
	float offset;
	float dutyCycle;
};

struct SetParams {
    uint32_t routes;
    float aoutValue[2];
    WaveformParameters aoutWaveformParameters[2];
    uint8_t relayOn;
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

struct Smx46Module : public Module {
public:
    TestResult testResult = TEST_NONE;

    bool powerDown = false;
    bool synchronized = false;

    ////////////////////////////////////////
    uint32_t input[(sizeof(Request) + 3) / 4 + 1];
    uint32_t output[(sizeof(Request) + 3) / 4];

    bool spiReady = false;
    bool spiDmaTransferCompleted = false;
    int spiDmaTransferStatus;

    uint32_t lastTransferTime = 0;
	SetParams lastTransferredParams;
    bool forceTransferSetParams;

	struct CommandDef {
		uint32_t command;
		void (Smx46Module::*fillRequest)(Request &request);
		void (Smx46Module::*done)(Response &response, bool isSuccess);
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
    ////////////////////////////////////////


    uint32_t routes;

    char columnLabels[NUM_COLUMNS][MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    char rowLabels[NUM_ROWS][MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];

    float aoutValue[2];
    bool calibrationEnabled[2];
    CalibrationConfiguration calConf[2];

    static const size_t CHANNEL_LABEL_MAX_LENGTH = 5;
    char aoutLables[2][CHANNEL_LABEL_MAX_LENGTH + 1];

    TriggerMode aoutTriggerMode[2];

    bool relayOn;

    // relay cycles counting    
    uint32_t signalRelayCycles[NUM_ROWS][NUM_COLUMNS];
    uint32_t powerRelayCycles;
    uint32_t lastWrittenSignalRelayCycles[NUM_ROWS][NUM_COLUMNS];
    uint32_t lastWrittenPowerRelayCycles;
    Interval relayCyclesWriteInterval = WRITE_ONTIME_INTERVAL * MIN_TO_MS;

    Smx46Module() {
        moduleType = MODULE_TYPE_DIB_SMX46;
        moduleName = "SMX46";
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
        numOtherChannels = 2 + 1 + NUM_ROWS * NUM_COLUMNS;
        isResyncSupported = true;

        resetConfiguration();

        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));
    }

    void boot() override {
        Module::boot();

        loadRelayCylces();
        
        calibrationEnabled[0] = persist_conf::isChannelCalibrationEnabled(slotIndex, 0);
        calibrationEnabled[1] = persist_conf::isChannelCalibrationEnabled(slotIndex, 1);
    }

    Module *createModule() override {
        return new Smx46Module();
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

        if (synchronized) {
			for (int otherChannelIndex = 0; otherChannelIndex < numOtherChannels; otherChannelIndex++) {
				int subchannelIndex = numPowerChannels + otherChannelIndex;
				g_slots[slotIndex]->loadChannelCalibration(subchannelIndex, nullptr);
			}
        }
    }

    ////////////////////////////////////////

    void Command_GetInfo_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            auto &data = response.getInfo;

            if (data.moduleType == MODULE_TYPE_DIB_SMX46) {
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

        if (!powerDown) {
            params.routes = routes;

			for (int i = 0; i < 2; i++) {
				params.aoutValue[i] = calibrationEnabled[i] && isVoltageCalibrationExists(i) ? calibration::remapValue(aoutValue[i], calConf[i].u) : aoutValue[i];

                auto waveformParameters = function_generator::getWaveformParameters(slotIndex, i, 0);
                if (!powerDown && waveformParameters && aoutTriggerMode[i] == TRIGGER_MODE_FUNCTION_GENERATOR && function_generator::isActive()) {
                    params.aoutWaveformParameters[i].waveform = waveformParameters->waveform;
                    params.aoutWaveformParameters[i].frequency = waveformParameters->frequency;
                    params.aoutWaveformParameters[i].phaseShift = waveformParameters->phaseShift;
                    params.aoutWaveformParameters[i].amplitude = calibrationEnabled[i] && isVoltageCalibrationExists(i) ? calibration::remapValue(waveformParameters->amplitude, calConf[i].u) : waveformParameters->amplitude;
                    params.aoutWaveformParameters[i].offset = calibrationEnabled[i] && isVoltageCalibrationExists(i) ? calibration::remapValue(waveformParameters->offset, calConf[i].u) : waveformParameters->offset;
                    params.aoutWaveformParameters[i].dutyCycle = waveformParameters->dutyCycle;
                } else {
                    params.aoutWaveformParameters[i].waveform = Waveform::WAVEFORM_NONE;
                }
            }
            
            params.relayOn = relayOn ? 1 : 0;
        }
	}

    void Command_SetParams_FillRequest(Request &request) {
        fillSetParams(request.setParams);
    }

    void Command_SetParams_Done(Response &response, bool isSuccess) {
        if (isSuccess) {
            auto &data = response.setParams;
            if (data.result) {
                SetParams &params = ((Request *)output)->setParams;

                updateRelayCycles(lastTransferredParams.routes, params.routes, lastTransferredParams.relayOn, params.relayOn);

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
        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)output, (uint8_t *)input, sizeof(Request));
        return status == bp3c::comm::TRANSFER_STATUS_OK;
    }

    bool getCommandResult() {
        Request &request = *(Request *)output;
        request.command = COMMAND_NONE;
        spiReady = false;
        spiDmaTransferCompleted = false;
        auto status = bp3c::comm::transferDMA(slotIndex, (uint8_t *)output, (uint8_t *)input, sizeof(Request));
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
						response->getInfo.moduleType = MODULE_TYPE_DIB_SMX46;
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

    Page *getPageFromId(int pageId) override;

    void animatePageAppearance(int previousPageId, int activePageId) override {
        if (
			(previousPageId == PAGE_ID_MAIN && activePageId == PAGE_ID_DIB_SMX46_SETTINGS) ||
			(previousPageId == PAGE_ID_MAIN && activePageId == PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES)
		) {
            psu::gui::animateSlideDown();
        } else if (
			(previousPageId == PAGE_ID_DIB_SMX46_SETTINGS && activePageId == PAGE_ID_MAIN) ||
			(previousPageId == PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES && activePageId == PAGE_ID_MAIN)
		) {
            psu::gui::animateSlideUp();
        } else if (
			(previousPageId == PAGE_ID_DIB_SMX46_SETTINGS && activePageId == PAGE_ID_DIB_SMX46_RELAY_CYCLES) ||
			(previousPageId == PAGE_ID_DIB_SMX46_SETTINGS && activePageId == PAGE_ID_DIB_SMX46_INFO) ||
			(previousPageId == PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION && activePageId == PAGE_ID_CH_SETTINGS_CALIBRATION) ||
			(previousPageId == PAGE_ID_DIB_SMX46_SETTINGS && activePageId == PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION)
		) {
			psu::gui::animateSlideLeft();
		} else if (
			(previousPageId == PAGE_ID_DIB_SMX46_RELAY_CYCLES && activePageId == PAGE_ID_DIB_SMX46_SETTINGS) ||
			(previousPageId == PAGE_ID_DIB_SMX46_INFO && activePageId == PAGE_ID_DIB_SMX46_SETTINGS) ||
			(previousPageId == PAGE_ID_CH_SETTINGS_CALIBRATION && activePageId == PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION) ||
			(previousPageId == PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION && activePageId == PAGE_ID_DIB_SMX46_SETTINGS)
		) {
			psu::gui::animateSlideRight();
		}
    }

    int getSlotView(SlotViewType slotViewType, int slotIndex, int cursor) override {
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_VERT : PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_HORZ;
        }
        if (slotViewType == SLOT_VIEW_TYPE_DEFAULT_2COL) {
            return isDefaultViewVertical() ? PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_2COL_VERT : PAGE_ID_DIB_SMX46_SLOT_VIEW_DEF_2COL_HORZ;
        }
        if (slotViewType == SLOT_VIEW_TYPE_MAX) {
            return PAGE_ID_DIB_SMX46_SLOT_VIEW_MAX;
        }
        assert(slotViewType == SLOT_VIEW_TYPE_MIN);
        return PAGE_ID_DIB_SMX46_SLOT_VIEW_MIN;
    }

    struct ProfileParameters : public Module::ProfileParameters {
        unsigned int routes;
        char columnLabels[NUM_COLUMNS][MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
        char rowLabels[NUM_ROWS][MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
        float aoutValue[2];
        char aoutLables[2][CHANNEL_LABEL_MAX_LENGTH + 1];
        uint8_t aoutTriggerMode[2];
        bool relayOn;
    };

    int getSlotSettingsPageId() override {
        return PAGE_ID_DIB_SMX46_SETTINGS;
    }

    int getLabelsAndColorsPageId() override {
        return getTestResult() == TEST_OK ? PAGE_ID_DIB_SMX46_LABELS_AND_COLORS : PAGE_ID_NONE;
    }

    void onLowPriorityThreadMessage(uint8_t type, uint32_t param) override {
        if (type == THREAD_MESSAGE_SAVE_RELAY_CYCLES) {
            saveRelayCycles();
        }
    }

    void resetProfileToDefaults(uint8_t *buffer) override {
        Module::resetProfileToDefaults(buffer);

        auto parameters = (ProfileParameters *)buffer;

        parameters->routes = 0;

        for (int i = 0; i < NUM_COLUMNS; i++) {
            parameters->columnLabels[i][0] = 'X';
            parameters->columnLabels[i][1] = '1' + i;
            parameters->columnLabels[i][2] = 0;
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            parameters->rowLabels[i][0] = 'Y';
            parameters->rowLabels[i][1] = '1' + i;
            parameters->rowLabels[i][2] = 0;
        }

        parameters->aoutValue[0] = 0.0f;
        parameters->aoutValue[1] = 0.0f;

        parameters->aoutLables[0][0] = 0;
        parameters->aoutLables[1][0] = 0;

        parameters->aoutTriggerMode[0] = TRIGGER_MODE_FIXED;
        parameters->aoutTriggerMode[1] = TRIGGER_MODE_FIXED;

        parameters->relayOn = false;
    }

    void getProfileParameters(uint8_t *buffer) override {
        Module::getProfileParameters(buffer);

        assert(sizeof(ProfileParameters) < MAX_SLOT_PARAMETERS_SIZE);

        auto parameters = (ProfileParameters *)buffer;

        parameters->routes = routes;
        memcpy(&parameters->columnLabels[0][0], &columnLabels[0][0], sizeof(columnLabels));
        memcpy(&parameters->rowLabels[0][0], &rowLabels[0][0], sizeof(rowLabels));
        parameters->aoutValue[0] = aoutValue[0];
        parameters->aoutValue[1] = aoutValue[1];
        memcpy(&parameters->aoutLables[0][0], &aoutLables[0][0], sizeof(aoutLables));
        parameters->aoutTriggerMode[0] = aoutTriggerMode[0];
        parameters->aoutTriggerMode[1] = aoutTriggerMode[1];
        parameters->relayOn = relayOn;
    }
    
    void setProfileParameters(uint8_t *buffer, bool mismatch, int recallOptions) override {
        Module::setProfileParameters(buffer, mismatch, recallOptions);

        auto parameters = (ProfileParameters *)buffer;

        routes = parameters->routes;
        memcpy(&columnLabels[0][0], &parameters->columnLabels[0][0], sizeof(columnLabels));
        memcpy(&rowLabels[0][0], &parameters->rowLabels[0][0], sizeof(rowLabels));
		aoutValue[0] = parameters->aoutValue[0];
		aoutValue[1] = parameters->aoutValue[1];
        memcpy(&aoutLables[0][0], &parameters->aoutLables[0][0], sizeof(aoutLables));
        aoutTriggerMode[0] = (TriggerMode)parameters->aoutTriggerMode[0];
        aoutTriggerMode[1] = (TriggerMode)parameters->aoutTriggerMode[1];
        relayOn = parameters->relayOn;
    }
    
    bool writeProfileProperties(psu::profile::WriteContext &ctx, const uint8_t *buffer) override {
        if (!Module::writeProfileProperties(ctx, buffer)) {
            return false;
        }

        auto parameters = (const ProfileParameters *)buffer;
        
        WRITE_PROPERTY("routes", parameters->routes);

        for (int i = 0; i < NUM_COLUMNS; i++) {
            char propName[16];
            snprintf(propName, sizeof(propName), "xLabel%d", i+1);
            WRITE_PROPERTY(propName, parameters->columnLabels[i]);
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            char propName[16];
            snprintf(propName, sizeof(propName), "yLabel%d", i+1);
            WRITE_PROPERTY(propName, parameters->rowLabels[i]);
        }

        WRITE_PROPERTY("aout1Value", parameters->aoutValue[0]);
        WRITE_PROPERTY("aout2Value", parameters->aoutValue[1]);

        WRITE_PROPERTY("aout1Label", parameters->aoutLables[0]);
        WRITE_PROPERTY("aout2Label", parameters->aoutLables[1]);

        WRITE_PROPERTY("aout1TriggerMode", parameters->aoutTriggerMode[0]);
        WRITE_PROPERTY("aout2TriggerMode", parameters->aoutTriggerMode[1]);

        WRITE_PROPERTY("relayOn", parameters->relayOn);

        return true;
    }
    
    bool readProfileProperties(psu::profile::ReadContext &ctx, uint8_t *buffer) override {
        if (Module::readProfileProperties(ctx, buffer)) {
            return true;
        }

        auto parameters = (ProfileParameters *)buffer;

        READ_PROPERTY("routes", parameters->routes);
        
        for (int i = 0; i < NUM_COLUMNS; i++) {
            char propName[16];
            snprintf(propName, sizeof(propName), "xLabel%d", i+1);
            READ_STRING_PROPERTY(propName, parameters->columnLabels[i], MAX_SWITCH_MATRIX_LABEL_LENGTH);
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            char propName[16];
            snprintf(propName, sizeof(propName), "yLabel%d", i+1);
            READ_STRING_PROPERTY(propName, parameters->rowLabels[i], MAX_SWITCH_MATRIX_LABEL_LENGTH);
        }

        READ_PROPERTY("aout1Value", parameters->aoutValue[0]);
        READ_PROPERTY("aout2Value", parameters->aoutValue[1]);

        READ_STRING_PROPERTY("aout1Label", parameters->aoutLables[0], CHANNEL_LABEL_MAX_LENGTH);
        READ_STRING_PROPERTY("aout2Label", parameters->aoutLables[1], CHANNEL_LABEL_MAX_LENGTH);

        READ_PROPERTY("aout1TriggerMode", parameters->aoutTriggerMode[0]);
        READ_PROPERTY("aout2TriggerMode", parameters->aoutTriggerMode[1]);

        READ_PROPERTY("relayOn", parameters->relayOn);

        return false;
    }

    void resetConfiguration() override {
        Module::resetConfiguration();

        routes = 0;

        for (int i = 0; i < NUM_COLUMNS; i++) {
            columnLabels[i][0] = 'X';
            columnLabels[i][1] = '1' + i;
            columnLabels[i][2] = 0;
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            rowLabels[i][0] = 'Y';
            rowLabels[i][1] = '1' + i;
            rowLabels[i][2] = 0;
        }

		aoutValue[0] = 0.0f;
		aoutValue[1] = 0.0f;

        aoutLables[0][0] = 0;
        aoutLables[1][0] = 0;

        aoutTriggerMode[0] = TRIGGER_MODE_FIXED;
        aoutTriggerMode[1] = TRIGGER_MODE_FIXED;

        relayOn = false;
    }

    bool isValidSubchannelIndex(int subchannelIndex) override {
        subchannelIndex++;
        return subchannelIndex == 1 || subchannelIndex == 2 || subchannelIndex == 3 ||
            (subchannelIndex >= 11 && subchannelIndex <= 16) ||
            (subchannelIndex >= 21 && subchannelIndex <= 26) ||
            (subchannelIndex >= 31 && subchannelIndex <= 36) ||
            (subchannelIndex >= 41 && subchannelIndex <= 46);
    }

    int getSubchannelIndexFromRelativeChannelIndex(int relativeChannelIndex) override {
        int subchannelIndex;
        if (relativeChannelIndex >= 21) {
            subchannelIndex = 40 + (relativeChannelIndex - 21);
        } else if (relativeChannelIndex >= 15) {
            subchannelIndex = 30 + (relativeChannelIndex - 15);
        } else  if (relativeChannelIndex >= 9) {
            subchannelIndex = 20 + (relativeChannelIndex - 9);
        } else if (relativeChannelIndex >= 3) {
            subchannelIndex = 10 + (relativeChannelIndex - 3);
        } else {
            subchannelIndex = relativeChannelIndex;
        }
        return subchannelIndex;
    }

    size_t getChannelLabelMaxLength(int subchannelIndex) override {
        return CHANNEL_LABEL_MAX_LENGTH;
    }

    eez_err_t getChannelLabel(int subchannelIndex, const char *&label) override {
        if (subchannelIndex == 0 || subchannelIndex == 1) {
            label = aoutLables[subchannelIndex];
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
        if (subchannelIndex == 0) {
            return "AOUT1";
        } 
        if (subchannelIndex == 1) {
            return "AOUT2";
        }
        return "";
    }

    eez_err_t setChannelLabel(int subchannelIndex, const char *label, int length) override {
        if (length == -1) {
            length = strlen(label);
        }
        if (length > (int)CHANNEL_LABEL_MAX_LENGTH) {
            length = CHANNEL_LABEL_MAX_LENGTH;
        }

        if (subchannelIndex == 0) {
            stringCopy(aoutLables[0], length + 1, label);
            return SCPI_RES_OK;
        }
        
        if (subchannelIndex == 1) {
            stringCopy(aoutLables[1], length + 1, label);
            return SCPI_RES_OK;
        }

        return SCPI_ERROR_HARDWARE_MISSING;
    }

    bool isRouteOpen(int subchannelIndex, bool &isRouteOpen_, int *err) override {
        subchannelIndex++;

        if (subchannelIndex == 3) {
            isRouteOpen_ = !relayOn;
        } else {
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }

            isRouteOpen_ = isRouteOpen(x, y);
        }

        return true;
    }

    bool routeOpen(ChannelList channelList, int *err) override {
        uint32_t tempRoutes = routes;
        bool tempRelayOn = relayOn;
        for (int i = 0; i < channelList.numChannels; i++) {
            int subchannelIndex = channelList.channels[i].subchannelIndex + 1;
            if (subchannelIndex == 3) {
                tempRelayOn = false;
                continue;
            }

            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }

            tempRoutes &= ~(1 << (y * NUM_COLUMNS + x));
        }
        routes = tempRoutes;
        relayOn = tempRelayOn;
        return true;
    }
    
    bool routeClose(ChannelList channelList, int *err) override {
        uint32_t tempRoutes = routes;
        bool tempRelayOn = relayOn;
        for (int i = 0; i < channelList.numChannels; i++) {
            int subchannelIndex = channelList.channels[i].subchannelIndex + 1;
            if (subchannelIndex == 3) {
                tempRelayOn = true;
                continue;
            }
            
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }

            tempRoutes |= (1 << (y * NUM_COLUMNS + x));
        }
        routes = tempRoutes;
        relayOn = tempRelayOn;
        return true;
    }

    bool getSwitchMatrixNumRows(int &numRows, int *err) override {
        numRows = NUM_ROWS;
        return true;
    }

    bool getSwitchMatrixNumColumns(int &numRows, int *err) override {
        numRows = NUM_COLUMNS;
        return true;
    }

    bool setSwitchMatrixRowLabel(int rowIndex, const char *label, int *err) override {
        stringCopy(rowLabels[rowIndex], sizeof(rowLabels[rowIndex]), label);
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_REFRESH_SCREEN);
        return true;
    }
    
    bool getSwitchMatrixRowLabel(int rowIndex, char *label, size_t labelLength, int *err) override {
        stringCopy(label, labelLength, rowLabels[rowIndex]);
        return true;
    }

    bool setSwitchMatrixColumnLabel(int columnIndex, const char *label, int *err) override {
        stringCopy(columnLabels[columnIndex], sizeof(columnLabels[columnIndex]), label);
        sendMessageToGuiThread(GUI_QUEUE_MESSAGE_REFRESH_SCREEN);
        return true;
    }
    
    bool getSwitchMatrixColumnLabel(int columnIndex, char *label, size_t labelLength, int *err) override {
        stringCopy(label, labelLength, columnLabels[columnIndex]);
        return true;
    }

    bool getRelayCycles(int subchannelIndex, uint32_t &relayCycles, int *err) override {
        subchannelIndex++;

        if (subchannelIndex == 3) {
            relayCycles = powerRelayCycles;
        } else {
            int x = subchannelIndex % 10 - 1;
            int y = subchannelIndex / 10 - 1;

            if (x < 0 || x >= NUM_COLUMNS || y < 0 || y >= NUM_ROWS) {
                if (err) {
                    *err = SCPI_ERROR_ILLEGAL_PARAMETER_VALUE;
                }
                return false;
            }

            relayCycles = signalRelayCycles[y][x];
        }

        return true;
    }

    bool getVoltage(int subchannelIndex, float &value, int *err) override {
        ++subchannelIndex;

        if (subchannelIndex != 1 && subchannelIndex != 2) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        value = aoutValue[subchannelIndex - 1];

        return true;
    }

    bool setVoltage(int subchannelIndex, float value, int *err) override {
        ++subchannelIndex;

        if (subchannelIndex != 1 && subchannelIndex != 2) {
            if (err) {
                *err = SCPI_ERROR_HARDWARE_MISSING;
            }
            return false;
        }

        if (value < DAC_MIN || value > DAC_MAX) {
            if (err) {
                *err = SCPI_ERROR_VOLTAGE_LIMIT_EXCEEDED;
            }
            return false;
        }

		aoutValue[subchannelIndex - 1] = value;

        return true;
    }

    void getVoltageStepValues(int subchannelIndex, StepValues *stepValues, bool calibrationMode) override {
        stepValues->values = DAC_ENCODER_STEP_VALUES;
        stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
        stepValues->unit = UNIT_VOLT;

		stepValues->encoderSettings.accelerationEnabled = true;
		stepValues->encoderSettings.range = DAC_MAX - DAC_MIN;
		stepValues->encoderSettings.step = stepValues->values[0];
        if (calibrationMode) {
            stepValues->encoderSettings.step /= 10.0f;
            stepValues->encoderSettings.range = stepValues->encoderSettings.step * 10.0f;
        }
        stepValues->encoderSettings.mode = edit_mode_step::g_smx46DacEncoderMode;
    }
    
    void setVoltageEncoderMode(int subchannelIndex, EncoderMode encoderMode) override {
        edit_mode_step::g_smx46DacEncoderMode = encoderMode;
    }
    
    float getVoltageResolution(int subchannelIndex) override {
        return DAC_RESOLUTION;
    }

    float getVoltageMinValue(int subchannelIndex) override {
        return DAC_MIN;
    }

    float getVoltageMaxValue(int subchannelIndex) override {
        return DAC_MAX;
    }

    bool isConstantVoltageMode(int subchannelIndex) override {
        return true;
    }

    bool isVoltageCalibrationExists(int subchannelIndex) override {
        return subchannelIndex < 2 && calConf[subchannelIndex].u.numPoints > 1;
    }

    bool isVoltageCalibrationEnabled(int subchannelIndex) override {
        return subchannelIndex < 2 && calibrationEnabled[subchannelIndex];
    }
    
    void enableVoltageCalibration(int subchannelIndex, bool enabled) override {
        if (subchannelIndex < 2) {
            calibrationEnabled[subchannelIndex] = enabled;
            persist_conf::saveCalibrationEnabledFlag(slotIndex, subchannelIndex, enabled);
        }
    }

    bool loadChannelCalibration(int subchannelIndex, int *err) override {
        if (subchannelIndex == 0 || subchannelIndex == 1) {
            persist_conf::loadChannelCalibrationConfiguration(slotIndex, subchannelIndex, calConf[subchannelIndex]);
            return true;
        }
        
        return true;
    }

    bool saveChannelCalibration(int subchannelIndex, int *err) override {
        if (subchannelIndex == 0 || subchannelIndex == 1) {
            return persist_conf::saveChannelCalibrationConfiguration(slotIndex, subchannelIndex, calConf[subchannelIndex]);
        }
      
        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    void getDefaultCalibrationPoints(int subchannelIndex, CalibrationValueType type, unsigned int &numPoints, float *&points) override {
        points = U_CAL_POINTS;
        numPoints = sizeof(U_CAL_POINTS) / sizeof(float);
    }

    bool getCalibrationConfiguration(int subchannelIndex, CalibrationConfiguration &calConf, int *err) override {
        if (subchannelIndex == 0) {
            memcpy(&calConf, &this->calConf[0], sizeof(CalibrationConfiguration));
            return true;
        } else if (subchannelIndex == 1) {
            memcpy(&calConf, &this->calConf[1], sizeof(CalibrationConfiguration));
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool setCalibrationConfiguration(int subchannelIndex, const CalibrationConfiguration &calConf, int *err) override {
        if (subchannelIndex == 0) {
            memcpy(&this->calConf[0], &calConf, sizeof(CalibrationConfiguration));
            return true;
        } else if (subchannelIndex == 1) {
            memcpy(&this->calConf[1], &calConf, sizeof(CalibrationConfiguration));
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool getCalibrationRemark(int subchannelIndex, const char *&calibrationRemark, int *err) override {
        if (subchannelIndex == 0) {
            calibrationRemark = calConf[0].calibrationRemark;
            return true;
        } else if (subchannelIndex == 1) {
            calibrationRemark = calConf[1].calibrationRemark;
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool getCalibrationDate(int subchannelIndex, uint32_t &calibrationDate, int *err) override {
        if (subchannelIndex == 0) {
            calibrationDate = calConf[0].calibrationDate;
            return true;
        } else if (subchannelIndex == 1) {
            calibrationDate = calConf[1].calibrationDate;
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }
        return false;
    }

    bool isRouteOpen(int x, int y) {
        return !(routes & (1 << (y * NUM_COLUMNS + x)));
    }

    void toggleRoute(int x, int y) {
        routes ^= 1 << (y * NUM_COLUMNS + x);
    }

    void updateRelayCycles(uint32_t oldRoutes, uint32_t newRoutes, uint8_t oldRelayOn, uint8_t newRelayOn) {
        for (int y = 0; y < NUM_ROWS; y++) {
            for (int x = 0; x < NUM_COLUMNS; x++) {
                if (!(oldRoutes & (1 << (y * NUM_COLUMNS + x))) && (newRoutes & (1 << (y * NUM_COLUMNS + x)))) {
                    signalRelayCycles[y][x]++;
                }
            }
        }

        if (!oldRelayOn && newRelayOn) {
            powerRelayCycles++;
        }
    }

    void loadRelayCylces() {
        powerRelayCycles = lastWrittenPowerRelayCycles = persist_conf::readCounter(slotIndex, 0);

        for (int y = 0; y < NUM_ROWS; y++) {
            for (int x = 0; x < NUM_COLUMNS; x++) {
                signalRelayCycles[y][x] = lastWrittenSignalRelayCycles[y][x] = persist_conf::readCounter(slotIndex, 1 + y * NUM_COLUMNS + x);
            }
        }
    }

    void saveRelayCycles() {
        if (powerRelayCycles != lastWrittenPowerRelayCycles) {
            persist_conf::writeCounter(slotIndex, 0, powerRelayCycles);
            lastWrittenPowerRelayCycles = powerRelayCycles;
        }

        for (int y = 0; y < NUM_ROWS; y++) {
            for (int x = 0; x < NUM_COLUMNS; x++) {
                if (signalRelayCycles[y][x] != lastWrittenSignalRelayCycles[y][x]) {
                    persist_conf::writeCounter(slotIndex, 1 + y * NUM_COLUMNS + x, signalRelayCycles[y][x]);
                    lastWrittenSignalRelayCycles[y][x] = signalRelayCycles[y][x];
                }
            }
        }
    }

    int getNumFunctionGeneratorResources(int subchannelIndex) override {
        if (subchannelIndex >= 0 && subchannelIndex <= 1) {
            return 1;
        }
        return 0;
    }

    FunctionGeneratorResourceType getFunctionGeneratorResourceType(int subchannelIndex, int resourceIndex) override {
        return FUNCTION_GENERATOR_RESOURCE_TYPE_U;
    }

    bool getFunctionGeneratorResourceTriggerMode(int subchannelIndex, int resourceIndex, TriggerMode &triggerMode, int *err) override {
        if (subchannelIndex == 0 || subchannelIndex == 1) {
            triggerMode = aoutTriggerMode[subchannelIndex];
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }

        return false;
    }

    bool setFunctionGeneratorResourceTriggerMode(int subchannelIndex, int resourceIndex, TriggerMode triggerMode, int *err) override {
        if (subchannelIndex == 0 || subchannelIndex == 1) {
            aoutTriggerMode[subchannelIndex] = triggerMode;
            return true;
        }

        if (err) {
            *err = SCPI_ERROR_HARDWARE_MISSING;
        }

        return false;    
    }

    const char *getFunctionGeneratorResourceLabel(int subchannelIndex, int resourceIndex) override {
        const char *label = getChannelLabel(subchannelIndex);
		if (*label) {
			return label;
		}
		return getDefaultChannelLabel(subchannelIndex);
	}

	void getFunctionGeneratorAmplitudeInfo(int subchannelIndex, int resourceIndex, FunctionGeneratorResourceType resourceType, float &min, float &max, StepValues *stepValues) override {
        min = DAC_MIN;
        max = DAC_MAX;
        if (stepValues) {
            stepValues->values = DAC_ENCODER_STEP_VALUES;
            stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
            stepValues->unit = UNIT_VOLT;
        }
    }
	
    void getFunctionGeneratorFrequencyInfo(int subchannelIndex, int resourceIndex, float &min, float &max, StepValues *stepValues) override {
        min = 0.1f;
        max = 10000.0f;

        if (stepValues) {
            static float values[] = { 1.0f, 10.0f, 100.0f, 500.0f };
            stepValues->values = values;
            stepValues->count = sizeof(values) / sizeof(float);
            stepValues->unit = UNIT_HERTZ;
        }
    }

	const char *getPinoutFile() override {
		return "smx46_pinout.jpg";
	}
};

////////////////////////////////////////////////////////////////////////////////

const Smx46Module::CommandDef Smx46Module::getInfo_command = {
	COMMAND_GET_INFO,
	nullptr,
	&Smx46Module::Command_GetInfo_Done
};

const Smx46Module::CommandDef Smx46Module::getState_command = {
	COMMAND_GET_STATE,
	nullptr,
	&Smx46Module::Command_GetState_Done
};

const Smx46Module::CommandDef Smx46Module::setParams_command = {
	COMMAND_SET_PARAMS,
	&Smx46Module::Command_SetParams_FillRequest,
	&Smx46Module::Command_SetParams_Done
};

////////////////////////////////////////////////////////////////////////////////

static Smx46Module g_smx46Module;
Module *g_module = &g_smx46Module;

////////////////////////////////////////////////////////////////////////////////

class ConfigureRoutesPage : public SetPage {
public:
    void pageAlloc() {
        Smx46Module *module = (Smx46Module *)g_slots[hmi::g_selectedSlotIndex];

        memcpy(columnLabels, module->columnLabels, sizeof(columnLabels));
        memcpy(columnLabelsOrig, module->columnLabels, sizeof(columnLabels));

        memcpy(rowLabels, module->rowLabels, sizeof(rowLabels));
        memcpy(rowLabelsOrig, module->rowLabels, sizeof(rowLabels));

        routes = routesOrig = module->routes;
    }

    int getDirty() { 
        return memcmp(columnLabels, columnLabelsOrig, sizeof(columnLabels)) != 0 || 
            memcmp(rowLabels, rowLabelsOrig, sizeof(rowLabels)) != 0 ||
            routes != routesOrig;
    }

    void set() {
        if (getDirty()) {
            Smx46Module *module = (Smx46Module *)g_slots[hmi::g_selectedSlotIndex];

            memcpy(module->columnLabels, columnLabels, sizeof(columnLabels));
            memcpy(module->rowLabels, rowLabels, sizeof(rowLabels));
            
            module->routes = routes;
        }

        popPage();
    }

    bool isRouteOpen(int x, int y) {
        return !(routes & (1 << (y * NUM_COLUMNS + x)));
    }

    void toggleRoute(int x, int y) {
        routes ^= 1 << (y * NUM_COLUMNS + x);
    }

    char columnLabels[NUM_COLUMNS][Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    char rowLabels[NUM_COLUMNS][Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    uint32_t routes = 0;

private:
    char columnLabelsOrig[NUM_COLUMNS][Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    char rowLabelsOrig[NUM_COLUMNS][Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];
    uint32_t routesOrig = 0;
};

static ConfigureRoutesPage g_ConfigureRoutesPage;

////////////////////////////////////////////////////////////////////////////////

class AoutConfigurationPage : public SetPage {
public:
	void pageAlloc() {
	}

	void pageWillAppear() {
		hmi::g_selectedSlotIndex = g_slotIndex;
		hmi::g_selectedSubchannelIndex = g_subchannelIndex;
	}

	int getDirty() {
		return 0;
	}

	void set() {
	}

	static int g_slotIndex;
	static int g_subchannelIndex;
};

int AoutConfigurationPage::g_slotIndex;
int AoutConfigurationPage::g_subchannelIndex;
static AoutConfigurationPage g_aoutConfigurationPage;

////////////////////////////////////////////////////////////////////////////////

Page *Smx46Module::getPageFromId(int pageId) {
    if (pageId == PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES) {
        return &g_ConfigureRoutesPage;
	} else if (pageId == PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION) {
		return &g_aoutConfigurationPage;
	}
    return nullptr;
}

} // namespace dib_smx46

namespace gui {

using namespace eez::dib_smx46;

void data_dib_smx46_routes(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_COLUMNS * NUM_ROWS;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * (NUM_COLUMNS * NUM_ROWS) + value.getInt();
    }
}

void data_dib_smx46_route_open(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / (NUM_COLUMNS * NUM_ROWS);
        int i = cursor % (NUM_COLUMNS * NUM_ROWS);
        int x = i % NUM_COLUMNS;
        int y = i / NUM_COLUMNS;
        ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
        if (page) {
            value = page->isRouteOpen(x, y);
        } else {
            value = ((Smx46Module *)g_slots[slotIndex])->isRouteOpen(x, y);
        }
    }
}

void data_dib_smx46_x_labels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_COLUMNS;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * NUM_COLUMNS + value.getInt();
    }
}

void data_dib_smx46_x_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / NUM_COLUMNS;
        int i = cursor % NUM_COLUMNS;
        ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
        if (page) {
            value = (const char *)&page->columnLabels[i][0];
        } else {
            value = (const char *)&((Smx46Module *)g_slots[slotIndex])->columnLabels[i][0];
        }
    }
}

void data_dib_smx46_y_labels(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_COUNT) {
        value = NUM_ROWS;
    } else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
        value = hmi::g_selectedSlotIndex * NUM_ROWS + value.getInt();
    }
}

void data_dib_smx46_y_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int slotIndex = cursor / NUM_ROWS;
        int i = cursor % NUM_ROWS;
        ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
        if (page) {
            value = (const char *)&page->rowLabels[i][0];
        } else {
            value = (const char *)&((Smx46Module *)g_slots[slotIndex])->rowLabels[i][0];
        }
    }
}

void data_dib_smx46_aout_channels(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_COUNT) {
		value = 2;
	} else if (operation == DATA_OPERATION_GET_CURSOR_VALUE) {
		value = hmi::g_selectedSlotIndex * 2 + value.getInt();
	}
}

void data_dib_smx46_aout_value(DataOperationEnum operation, Cursor cursor, Value &value) {
	int slotIndex = cursor / 2;
	int subchannelIndex = cursor % 2;
	
	if (operation == DATA_OPERATION_GET) {
        bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_DIB_SMX46_AOUT_VALUE;
        if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
            value = g_focusEditValue;
        } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
            data_keypad_text(operation, cursor, value);
        } else {
            value = MakeValue(((Smx46Module *)g_slots[slotIndex])->aoutValue[subchannelIndex], UNIT_VOLT);
        }
    } else if (operation == DATA_OPERATION_GET_MIN) {
        value = MakeValue(DAC_MIN, UNIT_VOLT);
    } else if (operation == DATA_OPERATION_GET_MAX) {
        value = MakeValue(DAC_MAX, UNIT_VOLT);
    }else if (operation == DATA_OPERATION_GET_NAME) {
        value = ((Smx46Module *)g_slots[slotIndex])->aoutLables[subchannelIndex];
    }  else if (operation == DATA_OPERATION_GET_UNIT) {
        value = UNIT_VOLT;
    } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
        StepValues *stepValues = value.getStepValues();

        stepValues->values = DAC_ENCODER_STEP_VALUES;
        stepValues->count = sizeof(DAC_ENCODER_STEP_VALUES) / sizeof(float);
        stepValues->unit = UNIT_VOLT;

        stepValues->encoderSettings.accelerationEnabled = true;
        stepValues->encoderSettings.range = DAC_MAX - DAC_MIN;
        stepValues->encoderSettings.step = stepValues->values[0];

        stepValues->encoderSettings.mode = edit_mode_step::g_smx46DacEncoderMode;

        value = 1;
    } else if (operation == DATA_OPERATION_SET_ENCODER_MODE) {
        edit_mode_step::g_smx46DacEncoderMode = (EncoderMode)value.getInt();
    } else if (operation == DATA_OPERATION_SET) {
        ((Smx46Module *)g_slots[slotIndex])->aoutValue[subchannelIndex] = roundPrec(value.getFloat(), DAC_RESOLUTION);
    }
}

void data_dib_smx46_aout_label(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		int subchannelIndex;

		if (cursor != -1) {
			slotIndex = cursor / 2;
			subchannelIndex = cursor % 2;
		} else {
			if (isPageOnStack(PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION)) {
				slotIndex = AoutConfigurationPage::g_slotIndex;
				subchannelIndex = AoutConfigurationPage::g_subchannelIndex;
			} else {
				slotIndex = hmi::g_selectedSlotIndex;
				subchannelIndex = hmi::g_selectedSubchannelIndex;
			}
		}

		if (isPageOnStack(PAGE_ID_SYS_SETTINGS_LABELS_AND_COLORS)) {
			const char *label = LabelsAndColorsPage::getChannelLabel(slotIndex, subchannelIndex);
			if (*label) {
				value = label;
			} else {
				value = g_slots[slotIndex]->getDefaultChannelLabel(subchannelIndex);
			}
		} else {
			const char *label = ((Smx46Module *)g_slots[slotIndex])->getChannelLabel(subchannelIndex);
			if (*label) {
				value = label;
			} else {
				value = ((Smx46Module *)g_slots[slotIndex])->getDefaultChannelLabel(subchannelIndex);
			}
		}
    }
}

void data_dib_smx46_aout_trigger_mode(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		int subchannelIndex;

		if (cursor != -1) {
			slotIndex = cursor / 2;
			subchannelIndex = cursor % 2;
		} else {
			if (isPageOnStack(PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION)) {
				slotIndex = AoutConfigurationPage::g_slotIndex;
				subchannelIndex = AoutConfigurationPage::g_subchannelIndex;
			} else {
				slotIndex = hmi::g_selectedSlotIndex;
				subchannelIndex = hmi::g_selectedSubchannelIndex;
			}
		}
		
		value = MakeEnumDefinitionValue(((Smx46Module *)g_slots[slotIndex])->aoutTriggerMode[subchannelIndex], ENUM_DEFINITION_CHANNEL_TRIGGER_MODE);
	}
}

void data_dib_smx46_aout_trigger_is_initiated(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex = cursor / 2;
		int subchannelIndex = cursor % 2;
		TriggerMode triggerMode = ((Smx46Module *)g_slots[slotIndex])->aoutTriggerMode[subchannelIndex];

		value = (trigger::isInitiated() || trigger::isTriggered()) && triggerMode == TRIGGER_MODE_FUNCTION_GENERATOR;
	} else if (operation == DATA_OPERATION_IS_BLINKING) {
		value = trigger::isInitiated();
	}
}

void data_dib_smx46_aout_function_label(DataOperationEnum operation, Cursor cursor, Value &value) {
	if (operation == DATA_OPERATION_GET) {
		int slotIndex;
		int subchannelIndex;

		if (cursor != -1) {
			slotIndex = cursor / 2;
			subchannelIndex = cursor % 2;
		} else {
			if (isPageOnStack(PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION)) {
				slotIndex = AoutConfigurationPage::g_slotIndex;
				subchannelIndex = AoutConfigurationPage::g_subchannelIndex;
			} else {
				slotIndex = hmi::g_selectedSlotIndex;
				subchannelIndex = hmi::g_selectedSubchannelIndex;
			}
		}

        auto waveformParameters = function_generator::getWaveformParameters(slotIndex, subchannelIndex, 0);
		
		value = function_generator::g_waveformShortLabel[waveformParameters ? waveformParameters->waveform : function_generator::WAVEFORM_NONE];
	}
}

void action_dib_smx46_aout_show_configuration() {
	g_aoutConfigurationPage.g_slotIndex = hmi::g_selectedSlotIndex;
	g_aoutConfigurationPage.g_subchannelIndex = getFoundWidgetAtDown().cursor % 2;
	pushPage(PAGE_ID_DIB_SMX46_AOUT_CONFIGURATION);
}

void action_dib_smx46_aout_show_calibration() {
	hmi::g_selectedSlotIndex = g_aoutConfigurationPage.g_slotIndex;
	hmi::g_selectedSubchannelIndex = g_aoutConfigurationPage.g_subchannelIndex;
	calibration::g_viewer.start(hmi::g_selectedSlotIndex, hmi::g_selectedSubchannelIndex);
	pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION);
}

static void onSetTriggerMode(uint16_t value) {
	popPage();

	((Smx46Module *)g_slots[g_aoutConfigurationPage.g_slotIndex])->aoutTriggerMode[g_aoutConfigurationPage.g_subchannelIndex] = (TriggerMode)value;

    if (value == TRIGGER_MODE_FUNCTION_GENERATOR) {
        function_generator::addChannelWaveformParameters(g_aoutConfigurationPage.g_slotIndex, g_aoutConfigurationPage.g_subchannelIndex, 0);
    }
}

void action_dib_smx46_aout_select_trigger_mode() {
	static EnumItem g_enumDefinitionMio168AoutTriggerMode[] = {
		{ TRIGGER_MODE_FIXED, "Fixed" },
		{ TRIGGER_MODE_FUNCTION_GENERATOR, "Function generator" },
		{ 0, 0 }
	};

	TriggerMode triggerMode = ((Smx46Module *)g_slots[g_aoutConfigurationPage.g_slotIndex])->aoutTriggerMode[g_aoutConfigurationPage.g_subchannelIndex];
	pushSelectFromEnumPage(g_enumDefinitionMio168AoutTriggerMode, triggerMode, nullptr, onSetTriggerMode);
}

void action_dib_smx46_aout_show_function() {
	function_generator::addChannelWaveformParameters(g_aoutConfigurationPage.g_slotIndex, g_aoutConfigurationPage.g_subchannelIndex, 0);

	pushPage(PAGE_ID_SYS_SETTINGS_FUNCTION_GENERATOR);

	function_generator::selectWaveformParametersForChannel(g_aoutConfigurationPage.g_slotIndex, g_aoutConfigurationPage.g_subchannelIndex, 0);
}

void data_dib_smx46_relay_on(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = ((Smx46Module *)g_slots[cursor])->relayOn ? 1 : 0;
    }
}

void data_dib_smx46_signal_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int i = cursor % (NUM_COLUMNS * NUM_ROWS);
        int x = i % NUM_COLUMNS;
        int y = i / NUM_COLUMNS;
        value = Value(((Smx46Module *)g_slots[hmi::g_selectedSlotIndex])->signalRelayCycles[y][x], VALUE_TYPE_UINT32);
    }
}

void data_dib_smx46_power_relay_cycles(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = Value(((Smx46Module *)g_slots[hmi::g_selectedSlotIndex])->powerRelayCycles, VALUE_TYPE_UINT32);
    }
}

void action_dib_smx46_show_configure_routes() {
    hmi::selectSlot(getFoundWidgetAtDown().cursor);
    pushPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
}

void action_dib_smx46_toggle_route() {
    int cursor = getFoundWidgetAtDown().cursor;
    int i = cursor % (NUM_COLUMNS * NUM_ROWS);
    int x = i % NUM_COLUMNS;
    int y = i / NUM_COLUMNS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    if (page) {
        page->toggleRoute(x, y);
    } else {
		int slotIndex = cursor / (NUM_COLUMNS * NUM_ROWS);
		auto module = (Smx46Module *)g_slots[slotIndex];
		module->toggleRoute(x, y);
    }
}

void action_dib_smx46_toggle_relay() {
    int cursor = getFoundWidgetAtDown().cursor;
    Smx46Module *module = (Smx46Module *)g_slots[cursor];
    module->relayOn = !module->relayOn;
}

static char *g_labelPointer;
static char g_labelDefault[Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1];

void onSetLabel(char *value) {
    stringCopy(g_labelPointer, Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1, value);
    popPage();
}

void onSetLabelDefault() {
	stringCopy(g_labelPointer, Module::MAX_SWITCH_MATRIX_LABEL_LENGTH + 1, g_labelDefault);
	popPage();
}

void action_dib_smx46_edit_x_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int i = cursor % NUM_COLUMNS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    if (page) {
        g_labelPointer = &page->columnLabels[i][0];
    } else {
		int slotIndex = cursor / NUM_COLUMNS;
        auto module = (Smx46Module *)g_slots[slotIndex];
        g_labelPointer = &module->columnLabels[i][0];
    }

	g_labelDefault[0] = 'X';
	g_labelDefault[1] = '1' + i;
	g_labelDefault[2] = 0;

    Keypad::startPush("Label: ", g_labelPointer, 1, Module::MAX_SWITCH_MATRIX_LABEL_LENGTH, false, onSetLabel, popPage, onSetLabelDefault);
}

void action_dib_smx46_edit_y_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int i = cursor % NUM_ROWS;
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    if (page) {
        g_labelPointer = &page->rowLabels[i][0];
    } else {
		int slotIndex = cursor / NUM_ROWS;
		auto module = (Smx46Module *)g_slots[slotIndex];
		g_labelPointer = &module->rowLabels[i][0];
    }
	
	g_labelDefault[0] = 'Y';
	g_labelDefault[1] = '1' + i;
	g_labelDefault[2] = 0;
	
	Keypad::startPush("Label: ", g_labelPointer, 1, Module::MAX_SWITCH_MATRIX_LABEL_LENGTH, false, onSetLabel, popPage, onSetLabelDefault);
}

void action_dib_smx46_clear_all_routes() {
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);
    if (page) {
        page->routes = 0;
    } else {
		int cursor = getFoundWidgetAtDown().cursor;
		int slotIndex = cursor;
		auto module = (Smx46Module *)g_slots[slotIndex];
		module->routes = 0;
    }
}

void action_dib_smx46_clear_all_labels() {
    ConfigureRoutesPage *page = (ConfigureRoutesPage *)getPage(PAGE_ID_DIB_SMX46_CONFIGURE_ROUTES);

    if (page) {
        for (int i = 0; i < NUM_COLUMNS; i++) {
            page->columnLabels[i][0] = 'X';
            page->columnLabels[i][1] = '1' + i;
            page->columnLabels[i][2] = 0;
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            page->rowLabels[i][0] = 'Y';
            page->rowLabels[i][1] = '1' + i;
            page->rowLabels[i][2] = 0;
        }
    } else {
		int cursor = getFoundWidgetAtDown().cursor;
		int slotIndex = cursor;
		auto module = (Smx46Module *)g_slots[slotIndex];

        for (int i = 0; i < NUM_COLUMNS; i++) {
            module->columnLabels[i][0] = 'X';
            module->columnLabels[i][1] = '1' + i;
            module->columnLabels[i][2] = 0;
        }

        for (int i = 0; i < NUM_ROWS; i++) {
            module->rowLabels[i][0] = 'Y';
            module->rowLabels[i][1] = '1' + i;
            module->rowLabels[i][2] = 0;
        }
    }

    refreshScreen();
}

void action_dib_smx46_change_subchannel_label() {
    int cursor = getFoundWidgetAtDown().cursor;
    int slotIndex = cursor / 2;
    int subchannelIndex = cursor % 2;
    LabelsAndColorsPage::editChannelLabel(slotIndex, subchannelIndex);
}

void action_dib_smx46_show_info() {
    pushPage(PAGE_ID_DIB_SMX46_INFO);
}

void action_dib_smx46_show_relay_cycles() {
    pushPage(PAGE_ID_DIB_SMX46_RELAY_CYCLES);
}

} // gui

} // namespace eez
