/*
 * EEZ Modular Firmware
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

#pragma once

#include <stdint.h>

#include "cmsis_os2.h"

#if defined(EEZ_PLATFORM_STM32)
#include "FreeRTOS.h"
#include "task.h"
#endif

#define EEZ_THREAD_DECLARE(NAME, PRIORITY, STACK_SIZE) \
    osThreadId_t g_##NAME##TaskHandle; \
    const osThreadAttr_t g_##NAME##TaskAttributes = { \
        #NAME, \
		0, \
		0, \
		0, \
		0, \
        STACK_SIZE, \
        osPriority##PRIORITY, \
		0, \
		0, \
    }
#define EEZ_THREAD_CREATE(NAME, THREAD_FUNC) g_##NAME##TaskHandle = osThreadNew(THREAD_FUNC, nullptr, &g_##NAME##TaskAttributes);
#define EEZ_THREAD_TERMINATE(NAME) osThreadTerminate(g_##NAME##TaskHandle)

#define EEZ_MESSAGE_QUEUE_DECLARE(NAME, OBJECT_DEF) \
    struct NAME##MessageQueueObject OBJECT_DEF; \
    osMessageQueueId_t g_##NAME##MessageQueueId
#define EEZ_MESSAGE_QUEUE_CREATE(NAME, QUEUE_SIZE) g_##NAME##MessageQueueId = osMessageQueueNew(QUEUE_SIZE, sizeof(NAME##MessageQueueObject), nullptr)
#define EEZ_MESSAGE_QUEUE_GET(NAME, OBJ, TIMEOUT) (osMessageQueueGet(g_##NAME##MessageQueueId, &OBJ, nullptr, TIMEOUT) == osOK)
#define EEZ_MESSAGE_QUEUE_PUT(NAME, OBJ, TIMEOUT) osMessageQueuePut(g_##NAME##MessageQueueId, &OBJ, 0, TIMEOUT)

#define EEZ_MUTEX_DECLARE(NAME) \
    osMutexId_t g_##NAME##mutexId;\
    const osMutexAttr_t g_##NAME##mutexAttr = { \
        #NAME, \
        osMutexRecursive | osMutexPrioInherit, \
        NULL, \
        0U \
    }
#define EEZ_MUTEX_CREATE(NAME) g_##NAME##mutexId = osMutexNew(&g_##NAME##mutexAttr)
#define EEZ_MUTEX_WAIT(NAME, TIMEOUT) osMutexAcquire(g_##NAME##mutexId, TIMEOUT) == osOK
#define EEZ_MUTEX_RELEASE(NAME) osMutexRelease(g_##NAME##mutexId)

namespace eez {

enum TestResult {
	TEST_NONE,
	TEST_FAILED,
	TEST_OK,
	TEST_CONNECTING,
	TEST_SKIPPED,
	TEST_WARNING
};

uint32_t millis();

extern bool g_shutdown;
void shutdown();

} // namespace eez