#include "cmsis_os2.h"

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

////////////////////////////////////////////////////////////////////////////////

#ifdef __EMSCRIPTEN__
#define MAX_THREADS 100
struct Thread {
	osThreadFunc_t func;
};

static Thread g_threads[MAX_THREADS];
Thread *g_currentThread;
#endif

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32

DWORD __stdcall win32_thread_func(void *lpParam) {
	auto thread_func = (osThreadFunc_t)lpParam;
	thread_func(nullptr);
    return 0;
}

#else

void *posix_thread_func(void *lpParam) {
	auto thread_func = (osThreadFunc_t)lpParam;
	thread_func(nullptr);
	return 0;
}

#endif


osThreadId_t osThreadNew(osThreadFunc_t func, void *, const osThreadAttr_t *attr) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    DWORD threadId;
    HANDLE handle = CreateThread(NULL, attr->stack_size, win32_thread_func, func, 0, &threadId);
	g_handles.insert(std::make_pair(threadId, handle));
    return threadId;
#elif defined(__EMSCRIPTEN__)
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (!g_threads[i].func) {
            g_threads[i].func = func;
            return &g_threads[i];
        }
    }
    assert(false);
    return nullptr;
#else
    pthread_t thread;
    pthread_create(&thread, 0, posix_thread_func, func);
    return thread;
#endif    
}

osStatus osThreadTerminate(osThreadId_t thread_id) {
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

osThreadId_t osThreadGetId() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return GetCurrentThreadId();
#elif defined(__EMSCRIPTEN__)
    return g_currentThread;
#else
    return pthread_self();
#endif    
}

////////////////////////////////////////////////////////////////////////////////

#ifdef __EMSCRIPTEN__
void eez_system_tick() {
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (g_threads[i].func) {
            g_currentThread = &g_threads[i];
            g_threads[i].func(nullptr);
        }
    }
}
#endif

osStatus osKernelInitialize(void) {
	return osOK;
}

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

uint32_t osKernelGetTickCount() {
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

////////////////////////////////////////////////////////////////////////////////

osMutexId_t osMutexNew(const osMutexAttr_t *) {
    return new std::mutex;
}

osStatus osMutexAcquire(osMutexId_t mutex, unsigned int timeout) {
#ifndef __EMSCRIPTEN__
	mutex->lock();
#endif
    return osOK;
}

osStatus osMutexRelease(osMutexId_t mutex) {
#ifndef __EMSCRIPTEN__
	mutex->unlock();
#endif
	return osOK;
}

////////////////////////////////////////////////////////////////////////////////

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size, const void *) {
	auto queue = new MessageQueue();
	queue->data = new uint8_t[msg_count * msg_size];
	queue->numElements = msg_count;
	queue->elementSize = msg_size;
    queue->tail = 0;
    queue->head = 0;
    queue->overflow = 0;
    return queue;
}

osStatus osMessageQueueGet(osMessageQueueId_t queue, void *msg_ptr, uint8_t *, uint32_t timeout) {
    if (timeout == 0) timeout = 1;

    queue->mutex.lock();

    while (queue->tail == queue->head) {
        if (queue->overflow) {
            break;
        }

        queue->mutex.unlock();

#ifdef __EMSCRIPTEN__
        return {
            osOK,
            0
        };
#else
        
        osDelay(1);
		timeout -= 1;

        if (timeout == 0) {
            return osError;
        }
#endif

        queue->mutex.lock();
    }

    uint16_t tail = queue->tail + 1;
    if (tail >= queue->numElements) {
        tail = 0;
    }
    queue->tail = tail;
    
	memcpy(msg_ptr, queue->data + tail * queue->elementSize, queue->elementSize);

	if (queue->overflow) {
        queue->overflow = 0;
    }

    queue->mutex.unlock();

    return osOK;
}

osStatus osMessageQueuePut(osMessageQueueId_t queue, const void *msg_ptr, uint8_t, uint32_t timeout) {
    queue->mutex.lock();
    for (uint32_t i = 0; queue->overflow && i < timeout; i++) {
        queue->mutex.unlock();
        osDelay(1);
        queue->mutex.lock();
    }
    
    if (queue->overflow) {
        queue->mutex.unlock();
		printf("overflow\n");
        return osError;
    }

    uint16_t head = queue->head + 1;
    if (head >= queue->numElements) {
        head = 0;
    }

	memcpy(queue->data + head * queue->elementSize, msg_ptr, queue->elementSize);
    
	queue->head = head;
    if (queue->head == queue->tail) {
        queue->overflow = 1;
    }

    queue->mutex.unlock();

    return osOK;
}
