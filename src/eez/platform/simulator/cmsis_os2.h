#pragma once

#include <stdint.h>

#ifndef __EMSCRIPTEN__
#include <mutex>
#include <thread>
#endif

typedef enum {
    osOK = 0,
    osError = 1,
    osEventMessage = 0x10
} osStatus;

typedef enum {
    osPriorityBelowNormal   = -1,
    osPriorityNormal = 0,
    osPriorityAboveNormal = 1
} osPriority;

////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TZ_ModuleId_t;

/// Attributes structure for thread.
typedef struct {
	const char    *name;       // name of the thread
	uint32_t       attr_bits;  // attribute bits
	void          *cb_mem;     // memory for control block
	uint32_t       cb_size;    // size of provided memory for control block
	void          *stack_mem;  // memory for stack
	uint32_t       stack_size; // size of stack
	osPriority     priority;   // initial thread priority (default: osPriorityNormal)
	TZ_ModuleId_t  tz_module;  // TrustZone module identifier
	uint32_t       reserved;   // reserved (must be 0)
} osThreadAttr_t;

typedef void(*osThreadFunc_t) (void *argument);

#ifdef __EMSCRIPTEN__
typedef void *osThreadId_t;
#else
typedef std::thread::id osThreadId_t;
#endif

osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr);
osStatus osThreadTerminate(osThreadId_t thread_id);
osThreadId_t osThreadGetId();

////////////////////////////////////////////////////////////////////////////////

osStatus osKernelInitialize(void);
osStatus osKernelStart(void);

osStatus osDelay(uint32_t millisec);
uint32_t osKernelGetTickCount(void);

#define osWaitForever     0xFFFFFFFF

struct osEvent {
    osStatus status;
    union  {
        uint32_t v;
    } value;
};

////////////////////////////////////////////////////////////////////////////////
// Mutex

// Mutex attributes (attr_bits in \ref osMutexAttr_t).
#define osMutexRecursive      0x00000001U ///< Recursive mutex.
#define osMutexPrioInherit    0x00000002U ///< Priority inherit protocol.

typedef struct {
	const char *name;     ///< name of the mutex
	uint32_t   attr_bits; ///< attribute bits
	void       *cb_mem;   ///< memory for control block
	uint32_t   cb_size;   ///< size of provided memory for control block
} osMutexAttr_t;

#ifdef __EMSCRIPTEN__
typedef bool osMutexId_t;
#else
typedef std::mutex *osMutexId_t;
#endif

#define osMutexDef(mutex) Mutex mutex
#define osMutexId(mutexId) Mutex *mutexId
#define osMutex(mutex) mutex

osMutexId_t osMutexNew(const osMutexAttr_t *);
osStatus osMutexAcquire(osMutexId_t mutex, unsigned int timeout);
osStatus osMutexRelease(osMutexId_t mutex);

////////////////////////////////////////////////////////////////////////////////
// Message Queue

struct MessageQueue {
    uint8_t *data;
    uint8_t numElements;
	uint32_t elementSize;
	std::mutex mutex;
    volatile uint16_t tail;
    volatile uint16_t head;
    volatile uint8_t overflow;
};

typedef MessageQueue *osMessageQueueId_t;

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size, const void *);
osStatus osMessageQueueGet(osMessageQueueId_t queue, void *msg_ptr, uint8_t *, uint32_t timeout);
osStatus osMessageQueuePut(osMessageQueueId_t queue, const void *msg_ptr, uint8_t, uint32_t timeout);

