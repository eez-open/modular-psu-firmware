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

#include <eez/system.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/ethernet.h>
#include <eez/modules/psu/event_queue.h>
#include <eez/modules/psu/ntp.h>
#include <eez/modules/psu/persist_conf.h>

#include <eez/modules/mcu/ethernet.h>

// Some time servers:
// - time.google.com
// - time.nist.gov

using namespace eez::psu;

namespace eez {
namespace ntp {

#ifdef EEZ_PLATFORM_STM32

#define CONF_NTP_LOCAL_PORT 8888
#define CONF_TIMEOUT_AFTER_SUCCESS_MS CONF_NTP_PERIOD_SEC * 1000L
#define CONF_TIMEOUT_AFTER_ERROR_MS CONF_NTP_PERIOD_AFTER_ERROR_SEC * 1000L
#define CONF_TEST_NTP_SERVER_TIMEOUT 5 * 1000 // 5 second
#define NUM_RETRIES 3

enum State {
	STATE_NOT_READY,
	STATE_WAIT,
	STATE_SUCCESS,
	STATE_ERROR,
	STATE_WAIT_TEST
};

enum Event {
	EVENT_READY,
	EVENT_NOT_READY,
	EVENT_SUCCEEDED,
	EVENT_TEST_NTP_SERVER,
	EVENT_TIMEOUT1,
	EVENT_TIMEOUT2,
	EVENT_RESET
};

static State g_state;

static uint32_t g_utc;

static uint32_t g_timeout1;
static uint32_t g_timeout2;

static const char *g_ntpServerToTest;
static int g_ntpServerTestResult;

extern "C" void sntpSetSystemTimeUs(uint32_t utc, uint32_t us) {
	g_utc = utc;
	mcu::ethernet::ntpStateTransition(EVENT_SUCCEEDED);
}

static bool isReady() {
	return psu::ethernet::g_testResult == TEST_OK && psu::persist_conf::isNtpEnabled();
}

static void setState(State state) {
	if (state != g_state) {
		g_state = state;
		//DebugTrace("NTP state: %d\n", g_state);
	}
}

static void setTimeout(uint32_t &timeout, uint32_t timeoutDuration) {
	timeout = millis() + timeoutDuration;
	if (timeout == 0) {
		timeout = 1;
	}
}

static const char *getNtpServer() {
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

static void start() {
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, (char *)getNtpServer());
	sntp_init();
}

static void startAndWait() {
	start();
	setTimeout(g_timeout1, CONF_TEST_NTP_SERVER_TIMEOUT);
	setTimeout(g_timeout2, NUM_RETRIES * CONF_TEST_NTP_SERVER_TIMEOUT);
	setState(STATE_WAIT);
}

static void startAndWaitTest() {
	start();
	setTimeout(g_timeout1, CONF_TEST_NTP_SERVER_TIMEOUT);
	setState(STATE_WAIT_TEST);			
}

static void stop() {
	sntp_stop();
}

#endif

void stateTransition(int event) {
#ifdef EEZ_PLATFORM_STM32
	if (g_state == STATE_NOT_READY) {
		if (event == EVENT_READY) {
			startAndWait();
		}
	} else if (g_state == STATE_WAIT) {
		if (event == EVENT_NOT_READY) {
			stop();
			setState(STATE_NOT_READY);
		} else if (event == EVENT_SUCCEEDED) {
			stop();

			uint32_t local = datetime::utcToLocal(g_utc, persist_conf::devConf.timeZone, (datetime::DstRule)persist_conf::devConf.dstRule);
			int year, month, day, hour, minute, second;
			datetime::breakTime(local, year, month, day, hour, minute, second);
			datetime::setDateTime(year - 2000, month, day, hour, minute, second, false, 2);

			setTimeout(g_timeout1, CONF_TIMEOUT_AFTER_SUCCESS_MS);
			setState(STATE_SUCCESS);
		} else if (event == EVENT_TEST_NTP_SERVER) {
			stop();
			startAndWaitTest();
		} else if (event == EVENT_TIMEOUT1) {
			stop();
			start();
			setTimeout(g_timeout1, CONF_TEST_NTP_SERVER_TIMEOUT);
		} else if (event == EVENT_TIMEOUT2) {
			stop();
			event_queue::pushEvent(event_queue::EVENT_WARNING_NTP_REFRESH_FAILED);
			setTimeout(g_timeout1, CONF_TIMEOUT_AFTER_ERROR_MS);
			setState(STATE_ERROR);
		} else if (event == EVENT_RESET) {
			stop();
			startAndWait();
		}
	} else if (g_state == STATE_SUCCESS) {
		if (event == EVENT_TEST_NTP_SERVER) {
			startAndWaitTest();
		} else if (event == EVENT_TIMEOUT1) {
			startAndWait();
		} else if (event == EVENT_RESET) {
			startAndWait();
		}
	} else if (g_state == STATE_ERROR) {
		if (event == EVENT_TEST_NTP_SERVER) {
			startAndWaitTest();
		} else if (event == EVENT_TIMEOUT1) {
			startAndWait();
		} else if (event == EVENT_RESET) {
			startAndWait();
		}
	} else if (g_state == STATE_WAIT_TEST) {
		if (event == EVENT_NOT_READY) {
			stop();
			g_ntpServerToTest = nullptr;
			g_ntpServerTestResult = 0;
			setState(STATE_NOT_READY);
		} else if (event == EVENT_SUCCEEDED) {
			stop();
			g_ntpServerToTest = nullptr;
			g_ntpServerTestResult = 1;
			startAndWait();
		} else if (event == EVENT_TEST_NTP_SERVER) {
			stop();
			startAndWaitTest();
		} else if (event == EVENT_TIMEOUT1) {
			g_ntpServerToTest = nullptr;
			g_ntpServerTestResult = 0;
			startAndWait();
		}
	}
#endif
}

void tick() {
#ifdef EEZ_PLATFORM_STM32
	if (g_state == STATE_NOT_READY) {
		if (isReady()) {
			stateTransition(EVENT_READY);
		}
	} else {
		if (!isReady()) {
			stateTransition(EVENT_NOT_READY);
		}
	}

	if (g_timeout1 && millis() >= g_timeout1) {
		g_timeout1 = 0;
		stateTransition(EVENT_TIMEOUT1);
	}

	if (g_timeout2 && millis() >= g_timeout2) {
		g_timeout2 = 0;
		stateTransition(EVENT_TIMEOUT2);
	}
#endif
}

void reset() {
#ifdef EEZ_PLATFORM_STM32
	mcu::ethernet::ntpStateTransition(EVENT_RESET);
#endif
}

void testNtpServer(const char *ntpServer) {
#ifdef EEZ_PLATFORM_STM32
    g_ntpServerToTest = ntpServer;
	g_ntpServerTestResult = 2;
	mcu::ethernet::ntpStateTransition(EVENT_TEST_NTP_SERVER);
#endif
}

bool isTestNtpServerDone(bool &result) {
#ifdef EEZ_PLATFORM_STM32
	if (g_ntpServerTestResult == 2) {
		return false;
	}
	result = g_ntpServerTestResult == 1;
	return true;
#else
    result = true;
    return true;
#endif
}

} // namespace ntp
} // namespace eez

#endif
