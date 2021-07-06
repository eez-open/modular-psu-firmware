#include "cmsis_os.h"

#include <assert.h>
#include <stdio.h>

#include <map>

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
#include <windows.h>
std::map<DWORD, HANDLE> g_handles;
#else
#include <sys/time.h>
#include <time.h>
#endif

#ifdef __EMSCRIPTEN__
#define MAX_THREADS 100
struct Thread {
    const osThreadDef_t *thread_def;
    void *argument;
};

static Thread g_threads[MAX_THREADS];
Thread *g_currentThread;
#endif

osThreadId osThreadCreate(const osThreadDef_t *thread_def, void *argument) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    DWORD threadId;
    HANDLE handle = CreateThread(NULL, thread_def->stacksize, (LPTHREAD_START_ROUTINE)thread_def->pthread, argument, 0, &threadId);
	g_handles.insert(std::make_pair(threadId, handle));
    return threadId;
#elif defined(__EMSCRIPTEN__)
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (!g_threads[i].thread_def) {
            g_threads[i].thread_def = thread_def;
            g_threads[i].argument = argument;
            return &g_threads[i];
        }
    }
    assert(false);
    return nullptr;
#else
    pthread_t thread;
    pthread_create(&thread, 0, thread_def->pthread, 0);
    return thread;
#endif    
}

osStatus osThreadTerminate(osThreadId thread_id) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
	HANDLE handle = g_handles.at(thread_id);
	TerminateThread(handle, 1);
	WaitForSingleObject(handle, INFINITE);
	CloseHandle(handle);
	return osOK;
#elif defined(__EMSCRIPTEN__)
	return osOK;
#else
	pthread_kill(thread_id, SIGKILL);
	pthread_join(thread_id, 0);
	return osOK;
#endif    
}

osThreadId osThreadGetId() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return GetCurrentThreadId();
#elif defined(__EMSCRIPTEN__)
    return g_currentThread;
#else
    return pthread_self();
#endif    
}

#ifdef __EMSCRIPTEN__
void eez_system_tick() {
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (g_threads[i].thread_def) {
            g_currentThread = &g_threads[i];
            g_threads[i].thread_def->pthread(g_threads[i].argument);
        }
    }
}
#endif

osStatus osKernelStart(void) {
    return osOK;
}

osStatus osDelay(uint32_t millisec) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    Sleep(millisec);
    return osOK;
#else
    timespec ts;
    ts.tv_sec = millisec / 1000;
    ts.tv_nsec = (millisec % 1000) * 1000000;
    nanosleep(&ts, 0);
    return osOK;
#endif    
}

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
uint32_t osKernelSysTickFrequency;
#else
uint32_t osKernelSysTickFrequency = 1000000;
#endif

uint32_t osKernelSysTick() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    static bool isFirstTime = true;
    static LARGE_INTEGER frequency;
    static LARGE_INTEGER startTime;

    if (isFirstTime) {
        isFirstTime = false;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&startTime);
        return 0;
    } else {
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);

        auto diff = (currentTime.QuadPart - startTime.QuadPart) * 1000 / frequency.QuadPart;

        return uint32_t(diff % 4294967296);
    }
#else
    timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t micros = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
    return uint32_t((micros / 1000) % 4294967296);
#endif    
}

osMessageQId osMessageCreate(osMessageQId queue_id, osThreadId thread_id) {
    queue_id->tail = 0;
    queue_id->head = 0;
    queue_id->overflow = 0;
    return queue_id;
}

osEvent osMessageGet(osMessageQId queue_id, uint32_t millisec) {
    if (millisec == 0) millisec = 1;

    while (queue_id->tail == queue_id->head) {
        if (queue_id->overflow) {
            queue_id->overflow = 0;
        }

#ifdef __EMSCRIPTEN__
        return {
            osOK,
            0
        };
#else
        
        osDelay(1);
        millisec -= 1;

        if (millisec == 0) {
            return {
                osOK,
                0
            };
        }
#endif
    }
    uint16_t tail = queue_id->tail + 1;
    if (tail >= queue_id->numElements) {
        tail = 0;
    }
    queue_id->tail = tail;
    uint32_t info = ((uint32_t *)queue_id->data)[tail];
    if (queue_id->overflow) {
        queue_id->overflow = 0;
    }
    return {
        osEventMessage,
        info
    };
}

osStatus osMessagePut(osMessageQId queue_id, uint32_t info, uint32_t millisec) {
    while (queue_id->overflow) {
        osDelay(0);
    }
    uint16_t head = queue_id->head + 1;
    if (head >= queue_id->numElements) {
        head = 0;
    }
    ((uint32_t *)queue_id->data)[head] = info;
    queue_id->head = head;
    if (queue_id->head == queue_id->tail) {
        queue_id->overflow = 1;
    }
    return osOK;
}

uint32_t osMessageWaiting(osMessageQId queue_id) {
    return queue_id->head - queue_id->tail;
}

Mutex *osMutexCreate(Mutex &mutex) {
    return &mutex;
}

osStatus osMutexWait(Mutex *mutex, unsigned int timeout) {
    while (mutex->locked) {
    	osDelay(1);
    }
    mutex->locked = true;
    return osOK;
}

void osMutexRelease(Mutex *mutex) {
    mutex->locked = false;
}
