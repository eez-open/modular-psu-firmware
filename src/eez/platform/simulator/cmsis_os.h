#pragma once

#include <stdint.h>
#include <mutex>

typedef enum {
    osOK = 0,
    osEventMessage = 0x10
} osStatus;

typedef enum {
    osPriorityBelowNormal   = -1,
    osPriorityNormal = 0,
    osPriorityAboveNormal = 1
} osPriority;

typedef void (*os_pthread)(void const *argument);

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
typedef uint32_t(__stdcall *THREAD_START_ROUTINE)(void *);
#else
typedef void *(*THREAD_START_ROUTINE)(void *);
#endif

typedef struct os_thread_def {
    const char *name;             ///< Thread name
    THREAD_START_ROUTINE pthread; ///< start address of thread function
    osPriority tpriority;         ///< initial thread priority
    uint32_t instances;           ///< maximum number of instances of that thread function
    uint32_t stacksize;           ///< stack size requirements in bytes; 0 is default stack size
} osThreadDef_t;

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
#define osThreadDef(name, thread, priority, instances, stacksz)                                    \
    uint32_t __stdcall os_thread_func_##name(void *lpParam) {                                      \
        thread((const void *)lpParam);                                                             \
        return 0;                                                                                  \
    }                                                                                              \
    const osThreadDef_t os_thread_def_##name = { #name, (os_thread_func_##name), (priority),       \
                                                 (instances), (stacksz) }

typedef uint32_t osThreadId;
#else

#include <pthread.h>
#include <signal.h>

#define osThreadDef(name, thread, priority, instances, stacksz)                                    \
    void *os_thread_func_##name(void *lpParam) {                                                   \
        thread((const void *)lpParam);                                                             \
        return nullptr;                                                                            \
    }                                                                                              \
    const osThreadDef_t os_thread_def_##name = { #name, (os_thread_func_##name), (priority),       \
                                                 (instances), (stacksz) }

#ifdef __EMSCRIPTEN__
typedef void *osThreadId;
#else
typedef pthread_t osThreadId;
#endif

#endif

#define osThread(name) &os_thread_def_##name

osThreadId osThreadCreate(const osThreadDef_t *thread_def, void *argument);
osStatus osThreadTerminate(osThreadId thread_id);

osThreadId osThreadGetId();

osStatus osKernelStart(void);

osStatus osDelay(uint32_t millisec);

uint32_t osKernelSysTick(void);

extern uint32_t osKernelSysTickFrequency;

//

#define osWaitForever     0xFFFFFFFF

struct osEvent {
    osStatus status;
    union  {
        uint32_t v;
    } value;
};

// Message Queue

struct MessageQueue {
    void *data;
    uint8_t numElements;
    volatile uint16_t tail;
    volatile uint16_t head;
    volatile uint8_t overflow;
};

typedef MessageQueue *osMessageQId;

#define osMessageQDef(name, numElements, ElementType) \
    static ElementType name##Data[numElements];       \
    static MessageQueue name = {                      \
        (void *)&name##Data[0],                       \
        numElements                                   \
    }
#define osMessageQ(name) (&name)

osMessageQId osMessageCreate(osMessageQId queue_id, osThreadId thread_id);
osEvent osMessageGet(osMessageQId queue_id, uint32_t millisec);
osStatus osMessagePut(osMessageQId queue_id, uint32_t info, uint32_t millisec);
uint32_t osMessageWaiting(osMessageQId queue_id);

// Mutex

typedef std::mutex Mutex;

#define osMutexDef(mutex) Mutex mutex
#define osMutexId(mutexId) Mutex *mutexId
#define osMutex(mutex) mutex

Mutex *osMutexCreate(Mutex &mutex);
osStatus osMutexWait(Mutex *mutex, unsigned int timeout);
void osMutexRelease(Mutex *mutex);