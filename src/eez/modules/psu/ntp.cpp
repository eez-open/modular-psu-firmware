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

#if OPTION_ETHERNET

#ifdef EEZ_PLATFORM_STM32
#include <api.h>
#include <sntp.h>
#include <ip_addr.h>
#endif

#include <eez/modules/psu/psu.h>

#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/ntp.h>
#include <eez/modules/psu/persist_conf.h>
#include <eez/system.h>

// Some time servers:
// - time.google.com
// - time.nist.gov

#define CONF_NTP_LOCAL_PORT 8888

#define CONF_TIMEOUT_AFTER_SUCCESS_MS CONF_NTP_PERIOD_SEC * 1000L
#define CONF_TIMEOUT_AFTER_ERROR_MS CONF_NTP_PERIOD_AFTER_ERROR_SEC * 1000L

#define CONF_TEST_NTP_SERVER_TIMEOUT 5 * 1000 // 5 second

namespace eez {
namespace psu {
namespace ntp {

#ifdef EEZ_PLATFORM_STM32

static bool g_initialized;
static bool g_succeeded;
static bool g_errorReported;
static uint32_t g_initTime;
static const char *g_ntpServerToTest;

const char *getNtpServer() {
    if (g_ntpServerToTest) {
        if (g_ntpServerToTest[0]) {
            return g_ntpServerToTest;
        }
    } else {
        if (persist_conf::devConf.ntpServer[0]) {
            return persist_conf::devConf.ntpServer;
        }
    }
    return NULL;
}

extern "C" void sntpSetSystemTimeUs(uint32_t utc, uint32_t us) {
	 uint32_t local = datetime::utcToLocal(utc, persist_conf::devConf.time_zone, (datetime::DstRule)persist_conf::devConf.dstRule);
	 int year, month, day, hour, minute, second;
	 datetime::breakTime(local, year, month, day, hour, minute, second);
	 datetime::setDateTime(year - 2000, month, day, hour, minute, second, false, 2);

	 g_succeeded = true;
}
#endif

void init() {
}

void tick() {
#ifdef EEZ_PLATFORM_STM32
	if (ethernet::g_testResult == TEST_OK && persist_conf::isNtpEnabled()) {
		if (!g_initialized) {
			g_initialized = true;

			sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_setservername(0, (char *)getNtpServer());
			sntp_init();
		} else {
			if (!g_ntpServerToTest) {
				int diff = millis() - g_initTime;
				if (g_succeeded) {
					if (diff > CONF_TIMEOUT_AFTER_SUCCESS_MS) {
						reset();
					}
				} else {
					if (diff > CONF_TIMEOUT_AFTER_ERROR_MS) {
						reset();
					} else if (!g_errorReported && diff > CONF_TEST_NTP_SERVER_TIMEOUT) {
						event_queue::pushEvent(event_queue::EVENT_WARNING_NTP_REFRESH_FAILED);
						g_errorReported = true;
					}
				}
			}
		}

    } else {
    	reset();
    }
#endif
}

void reset() {
#ifdef EEZ_PLATFORM_STM32
	if (g_initialized) {
		sntp_stop();
		g_initTime = millis();
		g_initialized = false;
		g_succeeded = false;
		g_errorReported = false;
	}
#endif
}

void testNtpServer(const char *ntpServer) {
#ifdef EEZ_PLATFORM_STM32
    // TODO
    g_ntpServerToTest = ntpServer;
    reset();
#else
#endif
}

bool isTestNtpServerDone(bool &result) {
#ifdef EEZ_PLATFORM_STM32
	if (g_succeeded) {
		g_ntpServerToTest = NULL;
        result = true;
        return true;
    } else {
    	int diff = millis() - g_initTime;
    	if (diff > CONF_TEST_NTP_SERVER_TIMEOUT) {
    		g_ntpServerToTest = NULL;
    		reset();
    		result = false;
            return true;
    	}
    }
    return false;
#else
    result = true;
    return true;
#endif
}

} // namespace ntp
} // namespace psu
} // namespace eez

#endif
