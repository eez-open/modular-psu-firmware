cmake_minimum_required(VERSION 3.10)

project(modular-psu-firmware)

set (CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wunused-const-variable -fPIC -s DEMANGLE_SUPPORT=1 -s FORCE_FILESYSTEM=1 -s ALLOW_MEMORY_GROWTH=1 -s \"BINARYEN_TRAP_MODE='clamp'\"")
    #set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} --preload-file ../../images/eez.png")
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='[png]'")
else()
    set(SDL2_BUILDING_LIBRARY 1)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
    include_directories(${SDL2IMAGE_INCLUDE_DIR})
    add_definitions(-DOPTION_ETHERNET=1)
endif()

add_definitions(-DHAVE_STRTOLL)
add_definitions(-DHAVE_STDBOOL)
add_definitions(-DSCPI_USER_CONFIG)

add_definitions(-DOPTION_DISPLAY=1)
add_definitions(-DOPTION_FAN=1)
add_definitions(-DOPTION_AUX_TEMP_SENSOR=1)
add_definitions(-DOPTION_EXT_RTC=1)
add_definitions(-DOPTION_ENCODER=1)
add_definitions(-DOPTION_EXT_EEPROM=1)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    add_definitions(-DOPTION_ETHERNET=0)
else()
    add_definitions(-DOPTION_ETHERNET=1)
endif()

add_definitions(-DOPTION_SD_CARD=1)

add_definitions(-DEEZ_PLATFORM_SIMULATOR)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DEEZ_PLATFORM_SIMULATOR_WIN32)
endif()

if (UNIX)
    add_definitions(-DEEZ_PLATFORM_SIMULATOR_UNIX)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    add_definitions(-DEEZ_PLATFORM_SIMULATOR_EMSCRIPTEN)
endif()

include_directories(
    src
    src/eez/platform/simulator
    src/eez/scpi

    src/third_party/libscpi/inc
)

set(src_files)
set(header_files)

set(src_eez
    src/eez/debug.cpp
    src/eez/index.cpp
    src/eez/sound.cpp
    src/eez/system.cpp
    src/eez/unit.cpp
    src/eez/util.cpp
)
list (APPEND src_files ${src_eez})
set(header_eez
    src/eez/debug.h
    src/eez/index.h
    src/eez/sound.h
    src/eez/system.h
    src/eez/unit.h
    src/eez/util.h
)
list (APPEND header_files ${header_eez})
source_group("eez" FILES ${src_eez} ${header_eez})

set(src_eez_apps_home
    src/eez/apps/home/data.cpp
    src/eez/apps/home/home.cpp
    src/eez/apps/home/touch_calibration.cpp
)
list (APPEND src_files ${src_eez_apps_home})
set(header_eez_apps_home
    src/eez/apps/home/data.h
    src/eez/apps/home/home.h
    src/eez/apps/home/touch_calibration.h
)
list (APPEND header_files ${header_eez_apps_home})
source_group("eez\\apps\\home" FILES ${src_eez_apps_home} ${header_eez_apps_home})

set(src_eez_apps_psu
    src/eez/apps/psu/calibration.cpp
    src/eez/apps/psu/channel.cpp
    src/eez/apps/psu/channel_dispatcher.cpp
    src/eez/apps/psu/datetime.cpp
    src/eez/apps/psu/debug.cpp
    src/eez/apps/psu/devices.cpp
    src/eez/apps/psu/dlog.cpp
    src/eez/apps/psu/ethernet.cpp
    src/eez/apps/psu/event_queue.cpp
    src/eez/apps/psu/idle.cpp
    src/eez/apps/psu/init.cpp
    src/eez/apps/psu/io_pins.cpp
    src/eez/apps/psu/lcd.cpp
    src/eez/apps/psu/list_program.cpp
    src/eez/apps/psu/ntp.cpp
    src/eez/apps/psu/ontime.cpp
    src/eez/apps/psu/persist_conf.cpp
    src/eez/apps/psu/pid.cpp
    src/eez/apps/psu/profile.cpp
    src/eez/apps/psu/psu.cpp
    src/eez/apps/psu/sd_card.cpp
    src/eez/apps/psu/serial_psu.cpp
    src/eez/apps/psu/temp_sensor.cpp
    src/eez/apps/psu/temperature.cpp
    src/eez/apps/psu/timer.cpp
    src/eez/apps/psu/trigger.cpp
    src/eez/apps/psu/unit.cpp
    src/eez/apps/psu/util.cpp
)
list (APPEND src_files ${src_eez_apps_psu})
set(header_eez_apps_psu
    src/eez/apps/psu/board.h
    src/eez/apps/psu/calibration.h
    src/eez/apps/psu/channel.h
    src/eez/apps/psu/channel_dispatcher.h
    src/eez/apps/psu/conf.h
    src/eez/apps/psu/conf_advanced.h
    src/eez/apps/psu/conf_all.h
    src/eez/apps/psu/conf_channel.h
    src/eez/apps/psu/conf_user.h
    src/eez/apps/psu/conf_user_revision.h
    src/eez/apps/psu/dac.h
    src/eez/apps/psu/datetime.h
    src/eez/apps/psu/debug.h
    src/eez/apps/psu/devices.h
    src/eez/apps/psu/dlog.h
    src/eez/apps/psu/eeprom.h
    src/eez/apps/psu/ethernet.h
    src/eez/apps/psu/event_queue.h
    src/eez/apps/psu/fan.h
    src/eez/apps/psu/idle.h
    src/eez/apps/psu/init.h
    src/eez/apps/psu/io_pins.h
    src/eez/apps/psu/ioexp.h
    src/eez/apps/psu/list_program.h
    src/eez/apps/psu/ntp.h
    src/eez/apps/psu/ontime.h
    src/eez/apps/psu/persist_conf.h
    src/eez/apps/psu/pid.h
    src/eez/apps/psu/profile.h
    src/eez/apps/psu/psu.h
    src/eez/apps/psu/rtc.h
    src/eez/apps/psu/sd_card.h
    src/eez/apps/psu/serial_psu.h
    src/eez/apps/psu/temp_sensor.h
    src/eez/apps/psu/temperature.h
    src/eez/apps/psu/timer.h
    src/eez/apps/psu/trigger.h
    src/eez/apps/psu/unit.h
    src/eez/apps/psu/util.h
    src/eez/apps/psu/watchdog.h
)
list (APPEND header_files ${header_eez_apps_psu})
source_group("eez\\apps\\psu" FILES ${src_eez_apps_psu} ${header_eez_apps_psu})

set(src_eez_apps_psu_gui
    src/eez/apps/psu/gui/calibration.cpp
    src/eez/apps/psu/gui/data.cpp
    src/eez/apps/psu/gui/edit_mode.cpp
    src/eez/apps/psu/gui/edit_mode_keypad.cpp
    src/eez/apps/psu/gui/edit_mode_slider.cpp
    src/eez/apps/psu/gui/edit_mode_step.cpp
    src/eez/apps/psu/gui/keypad.cpp
    src/eez/apps/psu/gui/numeric_keypad.cpp
    src/eez/apps/psu/gui/page_ch_settings_adv.cpp
    src/eez/apps/psu/gui/page_ch_settings_protection.cpp
    src/eez/apps/psu/gui/page_ch_settings_trigger.cpp
    src/eez/apps/psu/gui/page_event_queue.cpp
    src/eez/apps/psu/gui/page_self_test_result.cpp
    src/eez/apps/psu/gui/page_sys_settings.cpp
    src/eez/apps/psu/gui/page_user_profiles.cpp
    src/eez/apps/psu/gui/password.cpp
    src/eez/apps/psu/gui/psu.cpp
)
list (APPEND src_files ${src_eez_apps_psu_gui})
set(header_eez_apps_psu_gui
    src/eez/apps/psu/gui/calibration.h
    src/eez/apps/psu/gui/data.h
    src/eez/apps/psu/gui/edit_mode.h
    src/eez/apps/psu/gui/edit_mode_keypad.h
    src/eez/apps/psu/gui/edit_mode_slider.h
    src/eez/apps/psu/gui/edit_mode_step.h
    src/eez/apps/psu/gui/keypad.h
    src/eez/apps/psu/gui/numeric_keypad.h
    src/eez/apps/psu/gui/page_ch_settings_adv.h
    src/eez/apps/psu/gui/page_ch_settings_protection.h
    src/eez/apps/psu/gui/page_ch_settings_trigger.h
    src/eez/apps/psu/gui/page_event_queue.h
    src/eez/apps/psu/gui/page_self_test_result.h
    src/eez/apps/psu/gui/page_sys_settings.h
    src/eez/apps/psu/gui/page_user_profiles.h
    src/eez/apps/psu/gui/password.h
    src/eez/apps/psu/gui/psu.h
)
list (APPEND header_files ${header_eez_apps_psu_gui})
source_group("eez\\apps\\psu\\gui" FILES ${src_eez_apps_psu_gui} ${header_eez_apps_psu_gui})

set(src_eez_apps_psu_scpi
    src/eez/apps/psu/scpi/appl.cpp
    src/eez/apps/psu/scpi/cal.cpp
    src/eez/apps/psu/scpi/core.cpp
    src/eez/apps/psu/scpi/debug.cpp
    src/eez/apps/psu/scpi/diag.cpp
    src/eez/apps/psu/scpi/display.cpp
    src/eez/apps/psu/scpi/dlog.cpp
    src/eez/apps/psu/scpi/inst.cpp
    src/eez/apps/psu/scpi/meas.cpp
    src/eez/apps/psu/scpi/mem.cpp
    src/eez/apps/psu/scpi/mmem.cpp
    src/eez/apps/psu/scpi/outp.cpp
    src/eez/apps/psu/scpi/params.cpp
    src/eez/apps/psu/scpi/psu.cpp
    src/eez/apps/psu/scpi/regs.cpp
    src/eez/apps/psu/scpi/sense.cpp
    src/eez/apps/psu/scpi/simu.cpp
    src/eez/apps/psu/scpi/sour.cpp
    src/eez/apps/psu/scpi/stat.cpp
    src/eez/apps/psu/scpi/syst.cpp
    src/eez/apps/psu/scpi/trigger.cpp
)
list (APPEND src_files ${src_eez_apps_psu_scpi})
set(header_eez_apps_psu_scpi
    src/eez/apps/psu/scpi/params.h
    src/eez/apps/psu/scpi/psu.h
    src/eez/apps/psu/scpi/regs.h
)
list (APPEND header_files ${header_eez_apps_psu_scpi})
source_group("eez\\apps\\psu\\scpi" FILES ${src_eez_apps_psu_scpi} ${header_eez_apps_psu_scpi})

set(src_eez_apps_psu_simulator
    src/eez/apps/psu/simulator/board.cpp
    src/eez/apps/psu/simulator/dac.cpp
    src/eez/apps/psu/simulator/eeprom.cpp
    src/eez/apps/psu/simulator/fan.cpp
    src/eez/apps/psu/simulator/ioexp.cpp
    src/eez/apps/psu/simulator/psu.cpp
    src/eez/apps/psu/simulator/rtc.cpp
    src/eez/apps/psu/simulator/serial.cpp
    src/eez/apps/psu/simulator/watchdog.cpp
)
list (APPEND src_files ${src_eez_apps_psu_simulator})
set(header_eez_apps_psu_simulator
    src/eez/apps/psu/simulator/conf.h
    src/eez/apps/psu/simulator/psu.h
    src/eez/apps/psu/simulator/serial.h
)
list (APPEND header_files ${header_eez_apps_psu_simulator})
source_group("eez\\apps\\psu\\simulator" FILES ${src_eez_apps_psu_simulator} ${header_eez_apps_psu_simulator})

set(src_eez_apps_settings
    src/eez/apps/settings/settings.cpp
)
list (APPEND src_files ${src_eez_apps_settings})
set(header_eez_apps_settings
    src/eez/apps/settings/settings.h
)
list (APPEND header_files ${header_eez_apps_settings})
source_group("eez\\apps\\settings" FILES ${src_eez_apps_settings} ${header_eez_apps_settings})

set(src_eez_gui
    src/eez/gui/action_impl.cpp
    src/eez/gui/app_context.cpp
    src/eez/gui/assets.cpp
    src/eez/gui/data.cpp
    src/eez/gui/dialogs.cpp
    src/eez/gui/document.cpp
    src/eez/gui/draw.cpp
    src/eez/gui/event.cpp
    src/eez/gui/font.cpp
    src/eez/gui/gui.cpp
    src/eez/gui/page.cpp
    src/eez/gui/state.cpp
    src/eez/gui/touch.cpp
    src/eez/gui/touch_filter.cpp
    src/eez/gui/update.cpp
    src/eez/gui/widget.cpp
)
list (APPEND src_files ${src_eez_gui})
set(header_eez_gui
    src/eez/gui/action.h
    src/eez/gui/app_context.h
    src/eez/gui/assets.h
    src/eez/gui/data.h
    src/eez/gui/dialogs.h
    src/eez/gui/document.h
    src/eez/gui/document_simulator.h
    src/eez/gui/document_stm32.h
    src/eez/gui/draw.h
    src/eez/gui/event.h
    src/eez/gui/font.h
    src/eez/gui/geometry.h
    src/eez/gui/gui.h
    src/eez/gui/page.h
    src/eez/gui/state.h
    src/eez/gui/touch.h
    src/eez/gui/touch_filter.h
    src/eez/gui/update.h
    src/eez/gui/widget.h
)
list (APPEND header_files ${header_eez_gui})
source_group("eez\\gui" FILES ${src_eez_gui} ${header_eez_gui})

set(src_eez_gui_widgets
    src/eez/gui/widgets/app_view.cpp
    src/eez/gui/widgets/bar_graph.cpp
    src/eez/gui/widgets/bitmap.cpp
    src/eez/gui/widgets/button.cpp
    src/eez/gui/widgets/button_group.cpp
    src/eez/gui/widgets/container.cpp
    src/eez/gui/widgets/display_data.cpp
    src/eez/gui/widgets/grid.cpp
    src/eez/gui/widgets/layout_view.cpp
    src/eez/gui/widgets/list.cpp
    src/eez/gui/widgets/list_graph.cpp
    src/eez/gui/widgets/multiline_text.cpp
    src/eez/gui/widgets/rectangle.cpp
    src/eez/gui/widgets/scale.cpp
    src/eez/gui/widgets/select.cpp
    src/eez/gui/widgets/text.cpp
    src/eez/gui/widgets/toggle_button.cpp
    src/eez/gui/widgets/up_down.cpp
    src/eez/gui/widgets/yt_graph.cpp
)
list (APPEND src_files ${src_eez_gui_widgets})
set(header_eez_gui_widgets
    src/eez/gui/widgets/app_view.h
    src/eez/gui/widgets/bar_graph.h
    src/eez/gui/widgets/bitmap.h
    src/eez/gui/widgets/button.h
    src/eez/gui/widgets/button_group.h
    src/eez/gui/widgets/container.h
    src/eez/gui/widgets/display_data.h
    src/eez/gui/widgets/grid.h
    src/eez/gui/widgets/layout_view.h
    src/eez/gui/widgets/list.h
    src/eez/gui/widgets/list_graph.h
    src/eez/gui/widgets/multiline_text.h
    src/eez/gui/widgets/rectangle.h
    src/eez/gui/widgets/scale.h
    src/eez/gui/widgets/select.h
    src/eez/gui/widgets/text.h
    src/eez/gui/widgets/toggle_button.h
    src/eez/gui/widgets/up_down.h
    src/eez/gui/widgets/yt_graph.h
)
list (APPEND header_files ${header_eez_gui_widgets})
source_group("eez\\gui\\widgets" FILES ${src_eez_gui_widgets} ${header_eez_gui_widgets})

set(src_eez_libs_lz4
    src/eez/libs/lz4/lz4.c
)
list (APPEND src_files ${src_eez_libs_lz4})
set(header_eez_libs_lz4
    src/eez/libs/lz4/lz4.h
)
list (APPEND header_files ${header_eez_libs_lz4})
source_group("eez\\libs\\lz4" FILES ${src_eez_libs_lz4} ${header_eez_libs_lz4})

set(src_eez_libs_sd_fat
    src/eez/libs/sd_fat/simulator/sd_fat.cpp
)
list (APPEND src_files ${src_eez_libs_sd_fat})
set(header_eez_libs_sd_fat
    src/eez/libs/sd_fat/sd_fat.h
)
list (APPEND header_files ${header_eez_libs_sd_fat})
source_group("eez\\libs\\sd_fat" FILES ${src_eez_libs_sd_fat} ${header_eez_libs_sd_fat})

set(src_eez_modules_bp3c
    src/eez/modules/bp3c/relays.cpp
) 
list (APPEND src_files ${src_eez_modules_bp3c})
set(header_eez_modules_bp3c
    src/eez/modules/bp3c/relays.h
) 
list (APPEND header_files ${header_eez_modules_bp3c})
source_group("eez\\modules\\bp3c" FILES ${src_eez_modules_bp3c} ${header_eez_modules_bp3c})

set(src_eez_modules_dcpX05
    src/eez/modules/dcpX05/eeprom.cpp
) 
list (APPEND src_files ${src_eez_modules_dcpX05})
set(header_eez_modules_dcpX05
    src/eez/modules/dcpX05/eeprom.h
) 
list (APPEND header_files ${header_eez_modules_dcpX05})
source_group("eez\\modules\\dcpX05" FILES ${src_eez_modules_dcpX05} ${header_eez_modules_dcpX05})

set(src_eez_modules_mcu
    src/eez/modules/mcu/display.cpp
    src/eez/modules/mcu/ethernet.cpp
    src/eez/modules/mcu/encoder.cpp) 
list (APPEND src_files ${src_eez_modules_mcu})
set(header_eez_modules_mcu
    src/eez/modules/mcu/display.h
    src/eez/modules/mcu/encoder.h
    src/eez/modules/mcu/ethernet.h
    src/eez/modules/mcu/touch.h) 
list (APPEND header_files ${header_eez_modules_mcu})
source_group("eez\\modules\\mcu" FILES ${src_eez_modules_mcu} ${header_eez_modules_mcu})

set(src_eez_modules_mcu_simulator
    src/eez/modules/mcu/simulator/display.cpp
    src/eez/modules/mcu/simulator/touch.cpp
    src/eez/modules/mcu/simulator/ethernet.cpp

) 
list (APPEND src_files ${src_eez_modules_mcu_simulator})
source_group("eez\\modules\\mcu\\simulator" FILES ${src_eez_modules_mcu_simulator})

set(src_eez_modules_psu_simulator
    src/eez/modules/psu/simulator/adc.cpp
) 
source_group("eez\\modules\\psu\\simulator" FILES ${src_eez_modules_psu_simulator})
list (APPEND src_files ${src_eez_modules_psu_simulator})

set(src_eez_platform_simulator
    src/eez/platform/simulator/cmsis_os.cpp
    src/eez/platform/simulator/events.cpp
    src/eez/platform/simulator/front_panel.cpp
    src/eez/platform/simulator/main.cpp
    src/eez/platform/simulator/system.cpp
    src/eez/platform/simulator/texture.cpp
) 
list (APPEND src_files ${src_eez_platform_simulator})
set(header_eez_platform_simulator
    src/eez/platform/simulator/cmsis_os.h
    src/eez/platform/simulator/events.h
    src/eez/platform/simulator/front_panel.h
    src/eez/platform/simulator/texture.h
) 
list (APPEND header_files ${header_eez_platform_simulator})
source_group("eez\\platform\\simulator" FILES ${src_eez_platform_simulator} ${header_eez_platform_simulator})

set(src_eez_scpi
    src/eez/scpi/scpi.cpp
) 
list (APPEND src_files ${src_eez_scpi})
set(header_eez_scpi
    src/eez/scpi/commands.h
    src/eez/scpi/commands_simulator.h
    src/eez/scpi/commands_stm32.h
    src/eez/scpi/scpi.h
    src/eez/scpi/scpi_user_config.h
) 
list (APPEND header_files ${header_eez_scpi})
source_group("eez\\scpi" FILES ${src_eez_scpi} ${header_eez_scpi})

set(src_third_party_scpi
    src/third_party/libscpi/src/error.c
    src/third_party/libscpi/src/expression.c
    src/third_party/libscpi/src/fifo.c
    src/third_party/libscpi/src/ieee488.c
    src/third_party/libscpi/src/lexer.c
    src/third_party/libscpi/src/minimal.c
    src/third_party/libscpi/src/parser.c
    src/third_party/libscpi/src/units.c
    src/third_party/libscpi/src/utils.c
)
list (APPEND src_files ${src_third_party_scpi})
set(header_third_party_scpi
    src/third_party/libscpi/inc/scpi/cc.h
    src/third_party/libscpi/inc/scpi/config.h
    src/third_party/libscpi/inc/scpi/constants.h
    src/third_party/libscpi/inc/scpi/error.h
    src/third_party/libscpi/inc/scpi/expression.h
    src/third_party/libscpi/inc/scpi/ieee488.h
    src/third_party/libscpi/inc/scpi/minimal.h
    src/third_party/libscpi/inc/scpi/parser.h
    src/third_party/libscpi/inc/scpi/scpi.h
    src/third_party/libscpi/inc/scpi/types.h
    src/third_party/libscpi/inc/scpi/units.h
    src/third_party/libscpi/inc/scpi/utils.h
)
list (APPEND header_files ${header_third_party_scpi})
source_group("third_party\\scpi" FILES ${src_third_party_scpi} ${header_third_party_scpi})

add_executable(modular-psu-firmware ${src_files} ${header_files})

if(MSVC)
    target_compile_options(modular-psu-firmware PRIVATE "/MP")
endif()

target_link_libraries(modular-psu-firmware ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})

if(WIN32)
    target_link_libraries(modular-psu-firmware wsock32 ws2_32)

    add_custom_command(TARGET modular-psu-firmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/../SDL2-2.0.9/lib/x86/SDL2.dll"
            $<TARGET_FILE_DIR:modular-psu-firmware>)

    add_custom_command(TARGET modular-psu-firmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x86/libjpeg-9.dll"
            $<TARGET_FILE_DIR:modular-psu-firmware>)
    
    add_custom_command(TARGET modular-psu-firmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x86/libpng16-16.dll"
            $<TARGET_FILE_DIR:modular-psu-firmware>)

    add_custom_command(TARGET modular-psu-firmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x86/libtiff-5.dll"
            $<TARGET_FILE_DIR:modular-psu-firmware>)

    add_custom_command(TARGET modular-psu-firmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x86/libwebp-7.dll"
            $<TARGET_FILE_DIR:modular-psu-firmware>)
                        
    add_custom_command(TARGET modular-psu-firmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x86/SDL2_image.dll"
            $<TARGET_FILE_DIR:modular-psu-firmware>)
            
    add_custom_command(TARGET modular-psu-firmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x86/zlib1.dll"
            $<TARGET_FILE_DIR:modular-psu-firmware>)
                
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    add_custom_command(TARGET modular-psu-firmware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/src/eez/platform/simulator/emscripten"
        $<TARGET_FILE_DIR:modular-psu-firmware>)
endif()