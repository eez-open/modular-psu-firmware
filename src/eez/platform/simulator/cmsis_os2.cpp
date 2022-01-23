#include "cmsis_os2.h"

#include <assert.h>
#include <stdio.h>

#include <map>

#ifndef __EMSCRIPTEN__
#include <chrono>
#include <thread>
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

osThreadId_t osThreadNew(osThreadFunc_t func, void *, const osThreadAttr_t *attr) {
#if defined(__EMSCRIPTEN__)
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (!g_threads[i].func) {
            g_threads[i].func = func;
            return &g_threads[i];
        }
    }
    assert(false);
    return nullptr;
#else
    auto t = new std::thread(func, nullptr);
    return t->get_id();
#endif    
}

osStatus osThreadTerminate(osThreadId_t thread_id) {
#if defined(__EMSCRIPTEN__)
	return osOK;
#else
    // TODO
	return osOK;
#endif    
}

osThreadId_t osThreadGetId() {
#if defined(__EMSCRIPTEN__)
    return g_currentThread;
#else
    return std::this_thread::get_id();
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
#ifndef __EMSCRIPTEN__
    std::this_thread::sleep_for(std::chrono::milliseconds(millisec));
#endif
    return osOK;
}

uint32_t osKernelGetTickCount() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

////////////////////////////////////////////////////////////////////////////////

osMutexId_t osMutexNew(const osMutexAttr_t *) {
#ifdef __EMSCRIPTEN__
    return true;
#else
    return new std::mutex;
#endif
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

#ifndef __EMSCRIPTEN__
    queue->mutex.lock();
#endif

    while (queue->tail == queue->head) {
        if (queue->overflow) {
            break;
        }

#ifdef __EMSCRIPTEN__
        return osOK;
#else
        queue->mutex.unlock();
        
        osDelay(1);
		timeout -= 1;

        if (timeout == 0) {
            return osError;
        }

        queue->mutex.lock();
#endif
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

#ifndef __EMSCRIPTEN__
    queue->mutex.unlock();
#endif

    return osOK;
}

osStatus osMessageQueuePut(osMessageQueueId_t queue, const void *msg_ptr, uint8_t, uint32_t timeout) {
#ifndef __EMSCRIPTEN__    
    queue->mutex.lock();
    for (uint32_t i = 0; queue->overflow && i < timeout; i++) {
        queue->mutex.unlock();
        osDelay(1);
        queue->mutex.lock();
    }
#endif
    
    if (queue->overflow) {
#ifndef __EMSCRIPTEN__    
        queue->mutex.unlock();
#endif
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

#ifndef __EMSCRIPTEN__    
    queue->mutex.unlock();
#endif

    return osOK;
}
