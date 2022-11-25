#ifdef __cplusplus
    #ifdef __has_include
        #if __has_include("eez-framework-conf.h")
            #include "eez-framework-conf.h"
        #else
            #define OPTION_KEYBOARD 0
            #define OPTION_MOUSE 0
            #define OPTION_KEYPAD 0
            #define CUSTOM_VALUE_TYPES
        #endif
    #endif
#endif

#if ARDUINO
    #define EEZ_FOR_LVGL
    #include <Arduino.h>
#endif

#if EEZ_FOR_LVGL
    #define EEZ_OPTION_GUI 0
#endif

#ifndef EEZ_OPTION_GUI
    #define EEZ_OPTION_GUI 1
#endif

#ifdef __cplusplus
    #if EEZ_OPTION_GUI
        #ifdef __has_include
            #if __has_include("eez-framework-gui-conf.h")
                #include <eez-framework-gui-conf.h>
            #endif
        #endif
    #endif
#endif
