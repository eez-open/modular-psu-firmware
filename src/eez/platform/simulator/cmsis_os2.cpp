#if defined(EEZ_PLATFORM_SIMULATOR)

#include "cmsis_os2.h"

#include <assert.h>
#include <stdio.h>
#include <memory.h>

#ifndef __EMSCRIPTEN__
#include <chrono>
#include <thread>
#include <vector>
#endif

////////////////////////////////////////////////////////////////////////////////

#ifdef __EMSCRIPTEN__
#define MAX_THREADS 100
struct Thread {
	osThreadFunc_t func;
};

static std::vector<Thread *> g_threads;
static Thread *g_currentThread;
#endif

osThreadId_t osThreadNew(osThreadFunc_t func, void *, const osThreadAttr_t *attr) {
#if defined(__EMSCRIPTEN__)
    Thread *thread = new Thread;
    thread->func = func;
    g_threads.push_back(thread);
    return thread;
#else
    auto t = new std::thread(func, nullptr);
    return t->get_id();
#endif
}

osStatus osThreadTerminate(osThreadId_t thread_id) {
#if defined(__EMSCRIPTEN__)
    auto it = find(g_threads.begin(), g_threads.end(), (Thread *)thread_id);
    if (it == g_threads.end()) {
        return osError;
    }
    g_threads.erase(it);
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

#ifdef __EMSCRIPTEN__
static void oneIter() {
    for(Thread *thread: g_threads) {
        g_currentThread = thread;
        thread->func(nullptr);
    }
}

void eez_system_tick() {
    for (int i = 0; i < 2; i++) {
        oneIter();
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

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
	queue->elementSize = msg_size;
    return queue;
}

osStatus osMessageQueueGet(osMessageQueueId_t queue, void *msg_ptr, uint8_t *, uint32_t timeout) {
#ifndef __EMSCRIPTEN__
    if (timeout == 0) timeout = 1;
    queue->mutex.lock();
#endif

    while (queue->elements.empty()) {
#ifdef __EMSCRIPTEN__
        return osError;
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

    auto data = queue->elements.front();
    queue->elements.pop();
    memcpy(msg_ptr, data, queue->elementSize);
    delete [] data;

#ifndef __EMSCRIPTEN__
    queue->mutex.unlock();
#endif

    return osOK;
}

osStatus osMessageQueuePut(osMessageQueueId_t queue, const void *msg_ptr, uint8_t, uint32_t timeout) {
#ifndef __EMSCRIPTEN__
    queue->mutex.lock();
#endif

    uint8_t *data = new uint8_t[queue->elementSize];
	memcpy(data, msg_ptr, queue->elementSize);
	queue->elements.push(data);

#ifndef __EMSCRIPTEN__
    queue->mutex.unlock();
#endif

    return osOK;
}

#endif // defined(EEZ_PLATFORM_SIMULATOR)
