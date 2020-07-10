    /*
* EEZ Generic Firmware
* Copyright (C) 2020-present, Envox d.o.o.
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

#include <stdio.h> // sprintf

#include <eez/tasks.h>
#include <eez/mp.h>
#include <eez/sound.h>
#include <eez/hmi.h>
#include <eez/usb.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/dlog_record.h>
#include <eez/modules/psu/dlog_view.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/ontime.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/sd_card.h>
#include <eez/modules/psu/serial_psu.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/gui/page_ch_settings.h>
#include <eez/modules/psu/gui/page_user_profiles.h>

#include <eez/modules/bp3c/flash_slave.h>

#include <eez/modules/mcu/battery.h>

#include <eez/libs/sd_fat/sd_fat.h>
#include <eez/libs/image/jpeg.h>

namespace eez {

#define CONF_SCREENSHOT_TIMEOUT_MS 2000

////////////////////////////////////////////////////////////////////////////////

#define QUEUE_MESSAGE(type, param) (((param) << 8) | (type))
#define QUEUE_MESSAGE_TYPE(message) ((message) & 0xFF)
#define QUEUE_MESSAGE_PARAM(param) ((message) >> 8)

////////////////////////////////////////////////////////////////////////////////

void highPriorityThreadMainLoop(const void *);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_highPriorityThread, highPriorityThreadMainLoop, osPriorityAboveNormal, 0, 2048);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osThreadId g_highPriorityThreadHandle;

#if defined(EEZ_PLATFORM_STM32)
#define HIGH_PRIORITY_QUEUE_SIZE 50
#endif

#if defined(EEZ_PLATFORM_SIMULATOR)
#define HIGH_PRIORITY_QUEUE_SIZE 100
#endif

osMessageQDef(g_highPriorityMessageQueue, HIGH_PRIORITY_QUEUE_SIZE, uint32_t);
osMessageQId g_highPriorityMessageQueueId;

////////////////////////////////////////////////////////////////////////////////

void lowPriorityThreadMainLoop(const void *);

osThreadId g_lowPriorityTaskHandle;

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

osThreadDef(g_lowPriorityTask, lowPriorityThreadMainLoop, osPriorityNormal, 0, 8192);

#if defined(EEZ_PLATFORM_STM32)
#pragma GCC diagnostic pop
#endif

osMessageQDef(g_lowPriorityMessageQueue, LOW_PRIORITY_THREAD_QUEUE_SIZE, uint32_t);
osMessageQId g_lowPriorityMessageQueueId;

static bool g_shutingDown;
static bool g_isLowPriorityThreadAlive;

char g_listFilePath[CH_MAX][MAX_PATH_LENGTH];
bool g_screenshotGenerating;

static uint32_t g_timer1LastTickCount;

////////////////////////////////////////////////////////////////////////////////

void initHighPriorityMessageQueue() {
    g_highPriorityMessageQueueId = osMessageCreate(osMessageQ(g_highPriorityMessageQueue), NULL);
}

void startHighPriorityThread() {
	g_highPriorityThreadHandle = osThreadCreate(osThread(g_highPriorityThread), nullptr);
}

void highPriorityThreadOneIter();

void highPriorityThreadMainLoop(const void *) {
#ifdef __EMSCRIPTEN__
    highPriorityThreadOneIter();
#else
    g_highPriorityThreadHandle = osThreadGetId();

    while (1) {
        highPriorityThreadOneIter();
    }
#endif
}

void highPriorityThreadOneIter() {
    osEvent event = osMessageGet(g_highPriorityMessageQueueId, 1);
    if (event.status == osEventMessage) {
    	uint32_t message = event.value.v;
    	uint8_t type = QUEUE_MESSAGE_TYPE(message);
        uint32_t param = QUEUE_MESSAGE_PARAM(message);
        psu::onThreadMessage(type, param);
    } else {
        WATCHDOG_RESET();
        for (int i = 0; i < NUM_SLOTS; i++) {
            g_slots[i]->tick();
        }

        psu::tick();
    }
}

bool isPsuThread() {
    return !g_isBooted || osThreadGetId() == g_highPriorityThreadHandle;
}

void sendMessageToPsu(HighPriorityThreadMessage messageType, uint32_t messageParam, uint32_t timeoutMillisec) {
    osMessagePut(g_highPriorityMessageQueueId, QUEUE_MESSAGE(messageType, messageParam), timeoutMillisec);
}

////////////////////////////////////////////////////////////////////////////////

void initLowPriorityMessageQueue() {
    g_lowPriorityMessageQueueId = osMessageCreate(osMessageQ(g_lowPriorityMessageQueue), NULL);
}

void startLowPriorityThread() {
    g_isLowPriorityThreadAlive = true;
    g_timer1LastTickCount = micros();
    g_lowPriorityTaskHandle = osThreadCreate(osThread(g_lowPriorityTask), nullptr);
}

void lowPriorityThreadOneIter();

void lowPriorityThreadMainLoop(const void *) {
#ifdef __EMSCRIPTEN__
    if (g_isLowPriorityThreadAlive) {
        lowPriorityThreadOneIter();
    }
#else
    g_lowPriorityTaskHandle = osThreadGetId();

    while (g_isLowPriorityThreadAlive) {
    	lowPriorityThreadOneIter();
    }

    while (true) {
    	osDelay(1);
    }
#endif
}

void lowPriorityThreadOneIter() {
    using namespace psu;

    osEvent event = osMessageGet(g_lowPriorityMessageQueueId, 25);
    if (event.status == osEventMessage) {
    	uint32_t message = event.value.v;

    	uint32_t type = QUEUE_MESSAGE_TYPE(message);
    	uint32_t param = QUEUE_MESSAGE_PARAM(message);
        
        if (type < SERIAL_LAST_MESSAGE_TYPE) {
            serial::onQueueMessage(type, param);
        }
#if OPTION_ETHERNET
        else if (type < ETHERNET_LAST_MESSAGE_TYPE) {
            ethernet::onQueueMessage(type, param);
        }
#endif  
        else if (type < MP_LAST_MESSAGE_TYPE) {
            mp::onQueueMessage(type, param);
        } else {
            if (type == THREAD_MESSAGE_SAVE_LIST) {
                int err;
                if (!list::saveList(param, &g_listFilePath[param][0], &err)) {
                    generateError(err);
                }
            } else if (type == THREAD_MESSAGE_SHUTDOWN) {
                g_shutingDown = true;
            }
#if defined(EEZ_PLATFORM_STM32)
			else if (type == THREAD_MESSAGE_SD_DETECT_IRQ) {
				sd_card::onSdDetectInterruptHandler();
			}
#endif
            else if (type == THREAD_MESSAGE_DLOG_STATE_TRANSITION) {
                dlog_record::stateTransition(param);
            } else if (type == THREAD_MESSAGE_DLOG_SHOW_FILE) {
                dlog_view::openFile(nullptr);
            } else if (type == THREAD_MESSAGE_DLOG_LOAD_BLOCK) {
                dlog_view::loadBlock();
            } else if (type == THREAD_MESSAGE_ABORT_DOWNLOADING) {
                psu::scpi::abortDownloading();
            } else if (type == THREAD_MESSAGE_SCREENSHOT) {
                if (!sd_card::isMounted(nullptr)) {
                    g_screenshotGenerating = false;
                    generateError(SCPI_ERROR_MISSING_MASS_MEDIA);
                    return;
                }

                sound::playShutter();

                const uint8_t *screenshotPixels = mcu::display::takeScreenshot();

                unsigned char* imageData;
                size_t imageDataSize;

                if (jpegEncode(screenshotPixels, &imageData, &imageDataSize)) {
                    event_queue::pushEvent(SCPI_ERROR_OUT_OF_MEMORY_FOR_REQ_OP);
                    g_screenshotGenerating = false;
                    return;
                }

                char filePath[MAX_PATH_LENGTH + 1];
                uint8_t year, month, day, hour, minute, second;
                datetime::getDateTime(year, month, day, hour, minute, second);
                if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24) {
                    sprintf(filePath, "%s/%02d_%02d_%02d-%02d_%02d_%02d.jpg",
                        SCREENSHOTS_DIR,
                        (int)day, (int)month, (int)year,
                        (int)hour, (int)minute, (int)second);
                } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_24) {
                    sprintf(filePath, "%s/%02d_%02d_%02d-%02d_%02d_%02d.jpg",
                        SCREENSHOTS_DIR,
                        (int)month, (int)day, (int)year,
                        (int)hour, (int)minute, (int)second);
                } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12) {
                    bool am;
                    datetime::convertTime24to12(hour, am);
                    sprintf(filePath, "%s/%02d_%02d_%02d-%02d_%02d_%02d_%s.jpg",
                        SCREENSHOTS_DIR,
                        (int)day, (int)month, (int)year,
                        (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
                } else if (persist_conf::devConf.dateTimeFormat == datetime::FORMAT_MDY_12) {
                    bool am;
                    datetime::convertTime24to12(hour, am);
                    sprintf(filePath, "%s/%02d_%02d_%02d-%02d_%02d_%02d_%s.jpg",
                        SCREENSHOTS_DIR,
                        (int)month, (int)day, (int)year,
                        (int)hour, (int)minute, (int)second, am ? "AM" : "PM");
                }

                uint32_t timeout = millis() + CONF_SCREENSHOT_TIMEOUT_MS;
                while (millis() < timeout) {
                    File file;
                    if (file.open(filePath, FILE_CREATE_ALWAYS | FILE_WRITE)) {
                        size_t written = file.write(imageData, imageDataSize);
                        if (written == imageDataSize) {
                            if (file.close()) {
                                // success!
                                event_queue::pushEvent(event_queue::EVENT_INFO_SCREENSHOT_SAVED);
                                onSdCardFileChangeHook(filePath);
                                g_screenshotGenerating = false;
                                return;
                            }
                        }
                    }

                    sd_card::reinitialize();
                }

                // timeout
                event_queue::pushEvent(SCPI_ERROR_MASS_STORAGE_ERROR);
                g_screenshotGenerating = false;
            } else if (type == THREAD_MESSAGE_FILE_MANAGER_LOAD_DIRECTORY) {
                file_manager::doLoadDirectory();
            } else if (type == THREAD_MESSAGE_FILE_MANAGER_UPLOAD_FILE) {
                file_manager::uploadFile();
            } else if (type == THREAD_MESSAGE_FILE_MANAGER_OPEN_IMAGE_FILE) {
                file_manager::openImageFile();
            } else if (type == THREAD_MESSAGE_FILE_MANAGER_DELETE_FILE) {
                file_manager::deleteFile();
            } else if (type == THREAD_MESSAGE_FILE_MANAGER_RENAME_FILE) {
                file_manager::doRenameFile();
            } else if (type == THREAD_MESSAGE_DLOG_UPLOAD_FILE) {
                dlog_view::uploadFile();
            } else if (type == THREAD_MESSAGE_FLASH_SLAVE_UPLOAD_HEX_FILE) {
                bp3c::flash_slave::uploadHexFile();
            } else if (type == THREAD_MESSAGE_RECALL_PROFILE) {
                int err;
                if (!profile::recallFromLocation(param, 0, false, &err)) {
                    generateError(err);
                }
            } else if (type == THREAD_MESSAGE_LISTS_PAGE_IMPORT_LIST) {
                psu::gui::ChSettingsListsPage::doImportList();
            } else if (type == THREAD_MESSAGE_LISTS_PAGE_EXPORT_LIST) {
                psu::gui::ChSettingsListsPage::doExportList();
            } else if (type == THREAD_MESSAGE_LOAD_PROFILE) {
                profile::loadProfileParametersToCache(param);
            } else if (type == THREAD_MESSAGE_USER_PROFILES_PAGE_SAVE) {
                psu::gui::UserProfilesPage::doSaveProfile();
            } else if (type == THREAD_MESSAGE_USER_PROFILES_PAGE_RECALL) {
                psu::gui::UserProfilesPage::doRecallProfile();
            } else if (type == THREAD_MESSAGE_USER_PROFILES_PAGE_IMPORT) {
                psu::gui::UserProfilesPage::doImportProfile();
            } else if (type == THREAD_MESSAGE_USER_PROFILES_PAGE_EXPORT) {
                psu::gui::UserProfilesPage::doExportProfile();
            } else if (type == THREAD_MESSAGE_USER_PROFILES_PAGE_DELETE) {
                psu::gui::UserProfilesPage::doDeleteProfile();
            } else if (type == THREAD_MESSAGE_USER_PROFILES_PAGE_EDIT_REMARK) {
                psu::gui::UserProfilesPage::doEditRemark();
            } else if (type == THREAD_MESSAGE_SOUND_TICK) {
                sound::tick();
            } else if (type == THREAD_MESSAGE_SELECT_USB_MODE) {
                usb::selectUsbMode(param, usb::g_otgMode);
            } else if (type == THREAD_MESSAGE_SELECT_USB_DEVICE_CLASS) {
                usb::selectUsbDeviceClass(param);
            }
        }
    } else {
        if (g_shutingDown) {
            g_isLowPriorityThreadAlive = false;
            return;
        }
    	uint32_t tickCount = micros();
    	int32_t diff = tickCount - g_timer1LastTickCount;

        event_queue::tick();

        sound::tick();

    	if (diff >= 1000000L) { // 1 sec
            g_timer1LastTickCount = tickCount;

    		profile::tick();

            ontime::g_mcuCounter.tick(tickCount);
            for (int slotIndex = 0; slotIndex < NUM_SLOTS; slotIndex++) {
                if (g_slots[slotIndex]->moduleInfo->moduleType != MODULE_TYPE_NONE) {
                    ontime::g_moduleCounters[slotIndex].tick(tickCount);
                }
            }

            mcu::battery::tick();
    	}

        persist_conf::tick();

        sd_card::tick();

        eez::psu::dlog_record::fileWrite();

        eez::hmi::tick(tickCount);

        usb::tick(tickCount);

#ifdef DEBUG
        psu::debug::tick(tickCount);
#endif
    }

    return;
}

bool isLowPriorityThreadAlive() {
    return g_isLowPriorityThreadAlive;
}

bool isLowPriorityThread() {
    return osThreadGetId() == g_lowPriorityTaskHandle;
}

void sendMessageToLowPriorityThread(LowPriorityThreadMessage messageType, uint32_t messageParam, uint32_t timeoutMillisec) {
    osMessagePut(g_lowPriorityMessageQueueId, QUEUE_MESSAGE(messageType, messageParam), timeoutMillisec);
}

} // namespace eez
