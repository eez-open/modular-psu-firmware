#pragma once

#include <stdint.h>

typedef enum {
    osOK = 0,
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

typedef void *osThreadId;
#else

#include <pthread.h>

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

osStatus osKernelStart(void);

osStatus osDelay(uint32_t millisec);

uint32_t osKernelSysTick(void);

extern uint32_t osKernelSysTickFrequency;