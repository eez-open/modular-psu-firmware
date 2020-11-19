cmake_minimum_required(VERSION 3.10)

project(modular-psu-firmware)

set (CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wunused-const-variable -O2 -s DEMANGLE_SUPPORT=1 -s FORCE_FILESYSTEM=1 -s ALLOW_MEMORY_GROWTH=1 -s TOTAL_MEMORY=83886080 -lidbfs.js")
    #set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} --preload-file ../../images/eez.png")
endif()

add_definitions(-DFIRMWARE_VERSION_MAJOR=1)
add_definitions(-DFIRMWARE_VERSION_MINOR=4)

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

if (UNIX)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fpermissive")
endif (UNIX)

add_definitions(-DDEBUG)

add_definitions(-DHAVE_STRTOLL)
add_definitions(-DHAVE_STDBOOL)
add_definitions(-DSCPI_USER_CONFIG)

add_definitions(-DOPTION_DISPLAY=1)
add_definitions(-DOPTION_GUI_THREAD=1)
add_definitions(-DOPTION_FAN=1)
add_definitions(-DOPTION_AUX_TEMP_SENSOR=1)
add_definitions(-DOPTION_EXT_RTC=1)
add_definitions(-DOPTION_ENCODER=1)
add_definitions(-DOPTION_EXT_EEPROM=1)
add_definitions(-DOPTION_SDRAM=1)
add_definitions(-DEEZ_MCU_REVISION_R1B5=1)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    add_definitions(-DOPTION_ETHERNET=0)
else()
    add_definitions(-DOPTION_ETHERNET=1)
endif()

add_definitions(-DOPTION_SD_CARD=1)

add_definitions(-DEEZ_PLATFORM_SIMULATOR)

if(WIN32)
    ENABLE_LANGUAGE(RC)
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
    src/eez/libs/mqtt
    src/eez/platform/simulator
    src/eez/scpi

    src/third_party/libscpi/inc
    src/third_party/micropython
    src/third_party/micropython/ports/bb3
)

if(WIN32)
    include_directories(src/third_party/micropython/ports/bb3/win32)
endif()

set(src_files)
set(header_files)

set(src_eez
    src/eez/action_impl.cpp
    src/eez/firmware.cpp
    src/eez/debug.cpp
    src/eez/file_type.cpp
    src/eez/hmi.cpp
    src/eez/index.cpp
    src/eez/keyboard.cpp
    src/eez/main.cpp
    src/eez/memory.cpp
    src/eez/mouse.cpp
    src/eez/mp.cpp
    src/eez/mqtt.cpp
    src/eez/sound.cpp
    src/eez/system.cpp
    src/eez/tasks.cpp
    src/eez/unit.cpp
    src/eez/usb.cpp
    src/eez/util.cpp
)
list (APPEND src_files ${src_eez})
set(header_eez
    src/eez/firmware.h
    src/eez/debug.h
    src/eez/file_type.h
    src/eez/hmi.h
    src/eez/index.h
    src/eez/keyboard.h
    src/eez/memory.h
    src/eez/mouse.h
    src/eez/mp.h
    src/eez/mqtt.h
    src/eez/sound.h
    src/eez/system.h
    src/eez/tasks.h
    src/eez/unit.h
    src/eez/usb.h
    src/eez/util.h
    src/eez/value_types.h
)
list (APPEND header_files ${header_eez})
source_group("eez" FILES ${src_eez} ${header_eez})

set(src_eez_modules_psu
    src/eez/modules/psu/board.cpp
    src/eez/modules/psu/calibration.cpp
    src/eez/modules/psu/channel.cpp
    src/eez/modules/psu/channel_dispatcher.cpp
    src/eez/modules/psu/datetime.cpp
    src/eez/modules/psu/debug.cpp
    src/eez/modules/psu/devices.cpp
    src/eez/modules/psu/dlog_record.cpp
    src/eez/modules/psu/dlog_view.cpp
    src/eez/modules/psu/ethernet.cpp
    src/eez/modules/psu/event_queue.cpp
    src/eez/modules/psu/io_pins.cpp
    src/eez/modules/psu/list_program.cpp
    src/eez/modules/psu/ntp.cpp
    src/eez/modules/psu/ontime.cpp
    src/eez/modules/psu/persist_conf.cpp
    src/eez/modules/psu/profile.cpp
    src/eez/modules/psu/psu.cpp
    src/eez/modules/psu/ramp.cpp
    src/eez/modules/psu/rtc.cpp
    src/eez/modules/psu/sd_card.cpp
    src/eez/modules/psu/serial.cpp
    src/eez/modules/psu/serial_psu.cpp
    src/eez/modules/psu/temp_sensor.cpp
    src/eez/modules/psu/temperature.cpp
    src/eez/modules/psu/timer.cpp
    src/eez/modules/psu/trigger.cpp
)
list (APPEND src_files ${src_eez_modules_psu})
set(header_eez_modules_psu
    src/eez/modules/psu/board.h
    src/eez/modules/psu/calibration.h
    src/eez/modules/psu/channel.h
    src/eez/modules/psu/channel_dispatcher.h
    src/eez/modules/psu/conf.h
    src/eez/modules/psu/conf_advanced.h
    src/eez/modules/psu/conf_user.h
    src/eez/modules/psu/datetime.h
    src/eez/modules/psu/debug.h
    src/eez/modules/psu/devices.h
    src/eez/modules/psu/dlog_record.h
    src/eez/modules/psu/dlog_view.h
    src/eez/modules/psu/ethernet.h
    src/eez/modules/psu/event_queue.h
    src/eez/modules/psu/io_pins.h
    src/eez/modules/psu/list_program.h
    src/eez/modules/psu/ntp.h
    src/eez/modules/psu/ontime.h
    src/eez/modules/psu/persist_conf.h
    src/eez/modules/psu/profile.h
    src/eez/modules/psu/psu.h
    src/eez/modules/psu/ramp.h
    src/eez/modules/psu/rtc.h
    src/eez/modules/psu/sd_card.h
    src/eez/modules/psu/serial_psu.h
    src/eez/modules/psu/temp_sensor.h
    src/eez/modules/psu/temperature.h
    src/eez/modules/psu/timer.h
    src/eez/modules/psu/trigger.h
)
list (APPEND header_files ${header_eez_modules_psu})
source_group("eez\\modules\\psu" FILES ${src_eez_modules_psu} ${header_eez_modules_psu})

set(src_eez_modules_psu_gui
    src/eez/modules/psu/gui/animations.cpp
    src/eez/modules/psu/gui/channel_calibration.cpp
    src/eez/modules/psu/gui/data.cpp
    src/eez/modules/psu/gui/edit_mode.cpp
    src/eez/modules/psu/gui/file_manager.cpp
    src/eez/modules/psu/gui/keypad.cpp
    src/eez/modules/psu/gui/page.cpp
    src/eez/modules/psu/gui/page_ch_settings.cpp
    src/eez/modules/psu/gui/page_event_queue.cpp
    src/eez/modules/psu/gui/page_sys_settings.cpp
    src/eez/modules/psu/gui/page_user_profiles.cpp
    src/eez/modules/psu/gui/password.cpp
    src/eez/modules/psu/gui/psu.cpp
    src/eez/modules/psu/gui/touch_calibration.cpp
)
list (APPEND src_files ${src_eez_modules_psu_gui})
set(header_eez_modules_psu_gui
    src/eez/modules/psu/gui/animations.h
    src/eez/modules/psu/gui/channel_calibration.h
    src/eez/modules/psu/gui/data.h
    src/eez/modules/psu/gui/edit_mode.h
    src/eez/modules/psu/gui/file_manager.h
    src/eez/modules/psu/gui/keypad.h
    src/eez/modules/psu/gui/page.h
    src/eez/modules/psu/gui/page_ch_settings.h
    src/eez/modules/psu/gui/page_event_queue.h
    src/eez/modules/psu/gui/page_sys_settings.h
    src/eez/modules/psu/gui/page_user_profiles.h
    src/eez/modules/psu/gui/password.h
    src/eez/modules/psu/gui/psu.h
    src/eez/modules/psu/gui/touch_calibration.h
)
list (APPEND header_files ${header_eez_modules_psu_gui})
source_group("eez\\modules\\psu\\gui" FILES ${src_eez_modules_psu_gui} ${header_eez_modules_psu_gui})

set(src_eez_modules_psu_scpi
    src/eez/modules/psu/scpi/appl.cpp
    src/eez/modules/psu/scpi/cal.cpp
    src/eez/modules/psu/scpi/core.cpp
    src/eez/modules/psu/scpi/debug.cpp
    src/eez/modules/psu/scpi/diag.cpp
    src/eez/modules/psu/scpi/display.cpp
    src/eez/modules/psu/scpi/dlog.cpp
    src/eez/modules/psu/scpi/inst.cpp
    src/eez/modules/psu/scpi/meas.cpp
    src/eez/modules/psu/scpi/mem.cpp
    src/eez/modules/psu/scpi/mmem.cpp
    src/eez/modules/psu/scpi/outp.cpp
    src/eez/modules/psu/scpi/params.cpp
    src/eez/modules/psu/scpi/psu.cpp
    src/eez/modules/psu/scpi/route.cpp
    src/eez/modules/psu/scpi/sense.cpp
    src/eez/modules/psu/scpi/simu.cpp
    src/eez/modules/psu/scpi/sour.cpp
    src/eez/modules/psu/scpi/stat.cpp
    src/eez/modules/psu/scpi/syst.cpp
    src/eez/modules/psu/scpi/trigger.cpp
)
list (APPEND src_files ${src_eez_modules_psu_scpi})
set(header_eez_modules_psu_scpi
    src/eez/modules/psu/scpi/params.h
    src/eez/modules/psu/scpi/psu.h
)
list (APPEND header_files ${header_eez_modules_psu_scpi})
source_group("eez\\modules\\psu\\scpi" FILES ${src_eez_modules_psu_scpi} ${header_eez_modules_psu_scpi})

set(src_eez_gui
    src/eez/gui/app_context.cpp
    src/eez/gui/assets.cpp
    src/eez/gui/data.cpp
    src/eez/gui/document.cpp
    src/eez/gui/draw.cpp
    src/eez/gui/event.cpp
    src/eez/gui/font.cpp
    src/eez/gui/geometry.cpp
    src/eez/gui/gui.cpp
    src/eez/gui/overlay.cpp
    src/eez/gui/page.cpp
    src/eez/gui/touch.cpp
    src/eez/gui/touch_filter.cpp
    src/eez/gui/update.cpp
    src/eez/gui/widget.cpp
)
list (APPEND src_files ${src_eez_gui})
set(header_eez_gui
    src/eez/gui/app_context.h
    src/eez/gui/assets.h
    src/eez/gui/data.h
    src/eez/gui/document.h
    src/eez/gui/document_simulator.h
    src/eez/gui/document_stm32.h
    src/eez/gui/draw.h
    src/eez/gui/event.h
    src/eez/gui/font.h
    src/eez/gui/geometry.h
    src/eez/gui/gui.h
    src/eez/gui/overlay.h
    src/eez/gui/page.h
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
    src/eez/gui/widgets/canvas.cpp
    src/eez/gui/widgets/container.cpp
    src/eez/gui/widgets/display_data.cpp
    src/eez/gui/widgets/grid.cpp
    src/eez/gui/widgets/layout_view.cpp
    src/eez/gui/widgets/list.cpp
    src/eez/gui/widgets/list_graph.cpp
    src/eez/gui/widgets/multiline_text.cpp
    src/eez/gui/widgets/progress.cpp
    src/eez/gui/widgets/rectangle.cpp
    src/eez/gui/widgets/scroll_bar.cpp
    src/eez/gui/widgets/select.cpp
    src/eez/gui/widgets/text.cpp
    src/eez/gui/widgets/toggle_button.cpp
    src/eez/gui/widgets/up_down.cpp
    src/eez/gui/widgets/yt_graph.cpp
)
list (APPEND src_files ${src_eez_gui_widgets})
set(header_eez_gui_widgets
    src/eez/gui/widgets/button.h
    src/eez/gui/widgets/container.h
    src/eez/gui/widgets/display_data.h
    src/eez/gui/widgets/layout_view.h
    src/eez/gui/widgets/rectangle.h
    src/eez/gui/widgets/text.h
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

set(src_eez_libs_image
    src/eez/libs/image/bitmap.cpp
    src/eez/libs/image/image.cpp
    src/eez/libs/image/jpeg.cpp
    src/eez/libs/image/toojpeg.cpp
)
list (APPEND src_files ${src_eez_libs_image})
set(header_eez_libs_image
    src/eez/libs/image/bitmap.h
    src/eez/libs/image/image.h
    src/eez/libs/image/jpeg.h
    src/eez/libs/image/toojpeg.h
)
list (APPEND header_files ${src_eez_libs_image})
source_group("eez\\libs\\image" FILES ${src_eez_libs_image} ${header_eez_libs_image})
if(MSVC)
set_source_files_properties(src/eez/libs/image/toojpeg.cpp PROPERTIES COMPILE_FLAGS /W0)
endif()

set(src_eez_libs_mqtt
    src/eez/libs/mqtt/mqtt.c
    src/eez/libs/mqtt/mqtt_pal.c
)
list (APPEND src_files ${src_eez_libs_mqtt})
set(header_eez_libs_mqtt
    src/eez/libs/mqtt/mqtt.h
    src/eez/libs/mqtt/mqtt_pal.h
)
list (APPEND header_files ${header_eez_libs_mqtt})
source_group("eez\\libs\\mqtt" FILES ${src_eez_libs_mqtt} ${header_eez_libs_mqtt})


set(src_eez_modules_aux_ps
    src/eez/modules/aux_ps/fan.cpp
    src/eez/modules/aux_ps/pid.cpp
) 
list (APPEND src_files ${src_eez_modules_aux_ps})
set(header_eez_modules_aux_ps
    src/eez/modules/aux_ps/fan.h
    src/eez/modules/aux_ps/pid.h
) 
list (APPEND header_files ${header_eez_modules_aux_ps})
source_group("eez\\modules\\aux_ps" FILES ${src_eez_modules_aux_ps} ${header_eez_modules_aux_ps})

set(src_eez_modules_bp3c
    src/eez/modules/bp3c/comm.cpp
    src/eez/modules/bp3c/eeprom.cpp
    src/eez/modules/bp3c/io_exp.cpp
    src/eez/modules/bp3c/flash_slave.cpp
) 
list (APPEND src_files ${src_eez_modules_bp3c})
set(header_eez_modules_bp3c
    src/eez/modules/bp3c/comm.h
    src/eez/modules/bp3c/eeprom.h
    src/eez/modules/bp3c/io_exp.h
    src/eez/modules/bp3c/flash_slave.h
) 
list (APPEND header_files ${header_eez_modules_bp3c})
source_group("eez\\modules\\bp3c" FILES ${src_eez_modules_bp3c} ${header_eez_modules_bp3c})

set(src_eez_modules_fpga
    src/eez/modules/fpga/prog.cpp
) 
list (APPEND src_files ${src_eez_modules_fpga})
set(header_eez_modules_fpga
    src/eez/modules/fpga/prog.h
) 
list (APPEND header_files ${header_eez_modules_fpga})
source_group("eez\\modules\\fpga" FILES ${src_eez_modules_fpga} ${header_eez_modules_fpga})

set(src_eez_modules_mcu
    src/eez/modules/mcu/battery.cpp
    src/eez/modules/mcu/display.cpp
    src/eez/modules/mcu/eeprom.cpp
    src/eez/modules/mcu/ethernet.cpp
    src/eez/modules/mcu/encoder.cpp
    src/eez/modules/mcu/sdram.cpp) 
list (APPEND src_files ${src_eez_modules_mcu})
set(header_eez_modules_mcu
    src/eez/modules/mcu/battery.h
    src/eez/modules/mcu/display.h
    src/eez/modules/mcu/eeprom.h
    src/eez/modules/mcu/encoder.h
    src/eez/modules/mcu/ethernet.h
    src/eez/modules/mcu/sdram.h
    src/eez/modules/mcu/touch.h) 
list (APPEND header_files ${header_eez_modules_mcu})
source_group("eez\\modules\\mcu" FILES ${src_eez_modules_mcu} ${header_eez_modules_mcu})

set(src_eez_modules_mcu_simulator
    src/eez/modules/mcu/simulator/display.cpp
    src/eez/modules/mcu/simulator/touch.cpp

) 
list (APPEND src_files ${src_eez_modules_mcu_simulator})
source_group("eez\\modules\\mcu\\simulator" FILES ${src_eez_modules_mcu_simulator})

set(src_eez_modules_dib_dcp405
    src/eez/modules/dib-dcp405/adc.cpp
    src/eez/modules/dib-dcp405/dib-dcp405.cpp
    src/eez/modules/dib-dcp405/dac.cpp
    src/eez/modules/dib-dcp405/ioexp.cpp
) 
list (APPEND src_files ${src_eez_modules_dib_dcp405})
set(header_eez_modules_dib_dcp405
    src/eez/modules/dib-dcp405/adc.h
    src/eez/modules/dib-dcp405/dib-dcp405.h
    src/eez/modules/dib-dcp405/dac.h
    src/eez/modules/dib-dcp405/ioexp.h
) 
list (APPEND header_files ${header_eez_modules_dib_dcp405})
source_group("eez\\modules\\dib-dcp405" FILES ${src_eez_modules_dib_dcp405} ${header_eez_modules_dib_dcp405})

set(src_eez_modules_dib_dcm220
    src/eez/modules/dib-dcm220/dib-dcm220.cpp
) 
list (APPEND src_files ${src_eez_modules_dib_dcm220})
set(header_eez_modules_dib_dcm220
    src/eez/modules/dib-dcm220/dib-dcm220.h
) 
list (APPEND header_files ${header_eez_modules_dib_dcm220})
source_group("eez\\modules\\dib-dcm220" FILES ${src_eez_modules_dib_dcm220} ${header_eez_modules_dib_dcm220})

set(src_eez_modules_dib_dcm224
    src/eez/modules/dib-dcm224/dib-dcm224.cpp
) 
list (APPEND src_files ${src_eez_modules_dib_dcm224})
set(header_eez_modules_dib_dcm224
    src/eez/modules/dib-dcm224/dib-dcm224.h
) 
list (APPEND header_files ${header_eez_modules_dib_dcm224})
source_group("eez\\modules\\dib-dcm224" FILES ${src_eez_modules_dib_dcm224} ${header_eez_modules_dib_dcm224})

set(src_eez_modules_dib_mio168
    src/eez/modules/dib-mio168/dib-mio168.cpp
) 
list (APPEND src_files ${src_eez_modules_dib_mio168})
set(header_eez_modules_dib_mio168
    src/eez/modules/dib-mio168/dib-mio168.h
) 
list (APPEND header_files ${header_eez_modules_dib_mio168})
source_group("eez\\modules\\dib-mio168" FILES ${src_eez_modules_dib_mio168} ${header_eez_modules_dib_mio168})

set(src_eez_modules_dib_prel6
    src/eez/modules/dib-prel6/dib-prel6.cpp
) 
list (APPEND src_files ${src_eez_modules_dib_prel6})
set(header_eez_modules_dib_prel6
    src/eez/modules/dib-prel6/dib-prel6.h
) 
list (APPEND header_files ${header_eez_modules_dib_prel6})
source_group("eez\\modules\\dib-prel6" FILES ${src_eez_modules_dib_prel6} ${header_eez_modules_dib_prel6})

set(src_eez_modules_dib_smx46
    src/eez/modules/dib-smx46/dib-smx46.cpp
) 
list (APPEND src_files ${src_eez_modules_dib_smx46})
set(header_eez_modules_dib_smx46
    src/eez/modules/dib-smx46/dib-smx46.h
) 
list (APPEND header_files ${header_eez_modules_dib_smx46})
source_group("eez\\modules\\dib-smx46" FILES ${src_eez_modules_dib_smx46} ${header_eez_modules_dib_smx46})

set(src_eez_platform_simulator
    src/eez/platform/simulator/cmsis_os.cpp
    src/eez/platform/simulator/events.cpp
    src/eez/platform/simulator/front_panel.cpp
) 
list (APPEND src_files ${src_eez_platform_simulator})
set(header_eez_platform_simulator
    src/eez/platform/simulator/cmsis_os.h
    src/eez/platform/simulator/events.h
    src/eez/platform/simulator/front_panel.h
) 
list (APPEND header_files ${header_eez_platform_simulator})
source_group("eez\\platform\\simulator" FILES ${src_eez_platform_simulator} ${header_eez_platform_simulator})

set(src_eez_scpi
    src/eez/scpi/regs.cpp
    src/eez/scpi/scpi.cpp
) 
list (APPEND src_files ${src_eez_scpi})
set(header_eez_scpi
    src/eez/scpi/commands.h
    src/eez/scpi/commands_simulator.h
    src/eez/scpi/commands_stm32.h
    src/eez/scpi/regs.h
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
if(MSVC)
set_source_files_properties(${src_third_party_scpi} PROPERTIES COMPILE_FLAGS /W0)
endif()

set(src_third_party_micropython_extmod
    src/third_party/micropython/extmod/virtpin.c
    src/third_party/micropython/extmod/utime_mphal.c
)
list (APPEND src_files ${src_third_party_micropython_extmod})
set(header_third_party_micropython_extmod
    src/third_party/micropython/extmod/virtpin.h
    src/third_party/micropython/extmod/utime_mphal.h
)
list (APPEND header_files ${header_third_party_micropython_extmod})
source_group("third_party\\micropython\\extmod" FILES ${src_third_party_micropython_extmod} ${header_third_party_micropython_extmod})

set(src_third_party_micropython_ports_bb3
    src/third_party/micropython/ports/bb3/main.c
    src/third_party/micropython/ports/bb3/gccollect.cpp
    src/third_party/micropython/ports/bb3/mphalport.cpp
)
list (APPEND src_files ${src_third_party_micropython_ports_bb3})
set(header_third_party_micropython_ports_bb3
    src/third_party/micropython/ports/bb3/mpconfigport.h
    src/third_party/micropython/ports/bb3/mphalport.h
    src/third_party/micropython/ports/bb3/qstrdefsport.h
)
list (APPEND header_files ${header_third_party_micropython_ports_bb3})
source_group("third_party\\micropython\\ports\\bb3" FILES ${src_third_party_micropython_ports_bb3} ${header_third_party_micropython_ports_bb3})

set(header_third_party_micropython_ports_bb3_genhdr
    src/third_party/micropython/ports/bb3/genhdr/moduledefs.h
    src/third_party/micropython/ports/bb3/genhdr/mpversion.h
    src/third_party/micropython/ports/bb3/genhdr/qstrdefs.generated.h
)
list (APPEND header_files ${header_third_party_micropython_ports_bb3_genhdr})
source_group("third_party\\micropython\\ports\\bb3\\genhdr" FILES ${header_third_party_micropython_ports_bb3_genhdr})

set(src_third_party_micropython_ports_bb3_mod_eez
    src/third_party/micropython/ports/bb3/mod/eez/modeez_table.c
    src/third_party/micropython/ports/bb3/mod/eez/modeez.cpp
)
list (APPEND src_files ${src_third_party_micropython_ports_bb3_mod_eez})
set(header_third_party_micropython_ports_bb3_mod_eez
    src/third_party/micropython/ports/bb3/mod/eez/modeez.h
)
list (APPEND header_files ${header_third_party_micropython_ports_bb3_mod_eez})
source_group("third_party\\micropython\\ports\\bb3\\modeez" FILES ${src_third_party_micropython_ports_bb3_mod_eez} ${header_third_party_micropython_ports_bb3_mod_eez})

set(src_third_party_micropython_ports_bb3_mod_utime
    src/third_party/micropython/ports/bb3/mod/utime/modutime.c
)
list (APPEND src_files ${src_third_party_micropython_ports_bb3_mod_utime})
source_group("third_party\\micropython\\ports\\bb3\\utime" FILES ${src_third_party_micropython_ports_bb3_mod_utime})

set(src_third_party_micropython_py
    src/third_party/micropython/py/argcheck.c
    src/third_party/micropython/py/asmarm.c
    src/third_party/micropython/py/asmbase.c
    src/third_party/micropython/py/asmthumb.c
    src/third_party/micropython/py/asmx64.c
    src/third_party/micropython/py/asmx86.c
    src/third_party/micropython/py/asmxtensa.c
    src/third_party/micropython/py/bc.c
    src/third_party/micropython/py/binary.c
    src/third_party/micropython/py/builtinevex.c
    src/third_party/micropython/py/builtinhelp.c
    src/third_party/micropython/py/builtinimport.c
    src/third_party/micropython/py/compile.c
    src/third_party/micropython/py/emitbc.c
    src/third_party/micropython/py/emitcommon.c
    src/third_party/micropython/py/emitglue.c
    src/third_party/micropython/py/emitinlinethumb.c
    src/third_party/micropython/py/emitinlinextensa.c
    src/third_party/micropython/py/emitnarm.c
    src/third_party/micropython/py/emitnative.c
    src/third_party/micropython/py/emitnthumb.c
    src/third_party/micropython/py/emitnx64.c
    src/third_party/micropython/py/emitnx86.c
    src/third_party/micropython/py/emitnxtensa.c
    src/third_party/micropython/py/formatfloat.c
    src/third_party/micropython/py/frozenmod.c
    src/third_party/micropython/py/gc.c
    src/third_party/micropython/py/lexer.c
    src/third_party/micropython/py/malloc.c
    src/third_party/micropython/py/map.c
    src/third_party/micropython/py/modarray.c
    src/third_party/micropython/py/modbuiltins.c
    src/third_party/micropython/py/modcmath.c
    src/third_party/micropython/py/modcollections.c
    src/third_party/micropython/py/modgc.c
    src/third_party/micropython/py/modio.c
    src/third_party/micropython/py/modmath.c
    src/third_party/micropython/py/modmicropython.c
    src/third_party/micropython/py/modstruct.c
    src/third_party/micropython/py/modsys.c
    src/third_party/micropython/py/modthread.c
    src/third_party/micropython/py/moduerrno.c
    src/third_party/micropython/py/mpprint.c
    src/third_party/micropython/py/mpstate.c
    src/third_party/micropython/py/mpz.c
    src/third_party/micropython/py/nativeglue.c
    src/third_party/micropython/py/nlr.c
    src/third_party/micropython/py/nlrsetjmp.c
    src/third_party/micropython/py/nlrthumb.c
    src/third_party/micropython/py/nlrx64.c
    src/third_party/micropython/py/nlrx86.c
    src/third_party/micropython/py/nlrxtensa.c
    src/third_party/micropython/py/objarray.c
    src/third_party/micropython/py/objattrtuple.c
    src/third_party/micropython/py/objbool.c
    src/third_party/micropython/py/objboundmeth.c
    src/third_party/micropython/py/obj.c
    src/third_party/micropython/py/objcell.c
    src/third_party/micropython/py/objclosure.c
    src/third_party/micropython/py/objcomplex.c
    src/third_party/micropython/py/objdeque.c
    src/third_party/micropython/py/objdict.c
    src/third_party/micropython/py/objenumerate.c
    src/third_party/micropython/py/objexcept.c
    src/third_party/micropython/py/objfilter.c
    src/third_party/micropython/py/objfloat.c
    src/third_party/micropython/py/objfun.c
    src/third_party/micropython/py/objgenerator.c
    src/third_party/micropython/py/objgetitemiter.c
    src/third_party/micropython/py/objint.c
    src/third_party/micropython/py/objint_longlong.c
    src/third_party/micropython/py/objint_mpz.c
    src/third_party/micropython/py/objlist.c
    src/third_party/micropython/py/objmap.c
    src/third_party/micropython/py/objmodule.c
    src/third_party/micropython/py/objnamedtuple.c
    src/third_party/micropython/py/objnone.c
    src/third_party/micropython/py/objobject.c
    src/third_party/micropython/py/objpolyiter.c
    src/third_party/micropython/py/objproperty.c
    src/third_party/micropython/py/objrange.c
    src/third_party/micropython/py/objreversed.c
    src/third_party/micropython/py/objset.c
    src/third_party/micropython/py/objsingleton.c
    src/third_party/micropython/py/objslice.c
    src/third_party/micropython/py/objstr.c
    src/third_party/micropython/py/objstringio.c
    src/third_party/micropython/py/objstrunicode.c
    src/third_party/micropython/py/objtuple.c
    src/third_party/micropython/py/objtype.c
    src/third_party/micropython/py/objzip.c
    src/third_party/micropython/py/opmethods.c
    src/third_party/micropython/py/parse.c
    src/third_party/micropython/py/parsenumbase.c
    src/third_party/micropython/py/parsenum.c
    src/third_party/micropython/py/persistentcode.c
    src/third_party/micropython/py/pystack.c
    src/third_party/micropython/py/qstr.c
    src/third_party/micropython/py/reader.c
    src/third_party/micropython/py/repl.c
    src/third_party/micropython/py/runtime.c
    src/third_party/micropython/py/runtime_utils.c
    src/third_party/micropython/py/scheduler.c
    src/third_party/micropython/py/scope.c
    src/third_party/micropython/py/sequence.c
    src/third_party/micropython/py/showbc.c
    src/third_party/micropython/py/smallint.c
    src/third_party/micropython/py/stackctrl.c
    src/third_party/micropython/py/stream.c
    src/third_party/micropython/py/unicode.c
    src/third_party/micropython/py/vm.c
    src/third_party/micropython/py/vstr.c
    src/third_party/micropython/py/warning.c
)
list (APPEND src_files ${src_third_party_micropython_py})
set(header_third_party_micropython_py
    src/third_party/micropython/py/asmarm.h
    src/third_party/micropython/py/asmbase.h
    src/third_party/micropython/py/asmthumb.h
    src/third_party/micropython/py/asmx64.h
    src/third_party/micropython/py/asmx86.h
    src/third_party/micropython/py/asmxtensa.h
    src/third_party/micropython/py/bc0.h
    src/third_party/micropython/py/bc.h
    src/third_party/micropython/py/binary.h
    src/third_party/micropython/py/builtin.h
    src/third_party/micropython/py/compile.h
    src/third_party/micropython/py/emitglue.h
    src/third_party/micropython/py/emit.h
    src/third_party/micropython/py/formatfloat.h
    src/third_party/micropython/py/frozenmod.h
    src/third_party/micropython/py/gc.h
    src/third_party/micropython/py/grammar.h
    src/third_party/micropython/py/lexer.h
    src/third_party/micropython/py/misc.h
    src/third_party/micropython/py/mpconfig.h
    src/third_party/micropython/py/mperrno.h
    src/third_party/micropython/py/mphal.h
    src/third_party/micropython/py/mpprint.h
    src/third_party/micropython/py/mpstate.h
    src/third_party/micropython/py/mpthread.h
    src/third_party/micropython/py/mpz.h
    src/third_party/micropython/py/nlr.h
    src/third_party/micropython/py/objarray.h
    src/third_party/micropython/py/objexcept.h
    src/third_party/micropython/py/objfun.h
    src/third_party/micropython/py/objgenerator.h
    src/third_party/micropython/py/obj.h
    src/third_party/micropython/py/objint.h
    src/third_party/micropython/py/objlist.h
    src/third_party/micropython/py/objmodule.h
    src/third_party/micropython/py/objnamedtuple.h
    src/third_party/micropython/py/objstr.h
    src/third_party/micropython/py/objstringio.h
    src/third_party/micropython/py/objtuple.h
    src/third_party/micropython/py/objtype.h
    src/third_party/micropython/py/parse.h
    src/third_party/micropython/py/parsenumbase.h
    src/third_party/micropython/py/parsenum.h
    src/third_party/micropython/py/persistentcode.h
    src/third_party/micropython/py/pystack.h
    src/third_party/micropython/py/qstrdefs.h
    src/third_party/micropython/py/qstr.h
    src/third_party/micropython/py/reader.h
    src/third_party/micropython/py/repl.h
    src/third_party/micropython/py/ringbuf.h
    src/third_party/micropython/py/runtime0.h
    src/third_party/micropython/py/runtime.h
    src/third_party/micropython/py/scope.h
    src/third_party/micropython/py/smallint.h
    src/third_party/micropython/py/stackctrl.h
    src/third_party/micropython/py/stream.h
    src/third_party/micropython/py/unicode.h
    src/third_party/micropython/py/vmentrytable.h
)
list (APPEND header_files ${header_third_party_micropython_py})
source_group("third_party\\micropython\\py" FILES ${src_third_party_micropython_py} ${header_third_party_micropython_py})
if(MSVC)
set_source_files_properties(${src_third_party_micropython_py} PROPERTIES COMPILE_FLAGS /W0)
endif()

if(WIN32)
    set(src_win32_specific
        src/eez/platform/simulator/win32/icon.rc
    ) 
    list (APPEND src_files ${src_win32_specific})
    source_group("eez\\win32" FILES ${src_win32_specific})

    set(SOURCES src/eez/platform/simulator/win32/icon.rc ${src_files})
endif()

add_executable(modular-psu-firmware ${src_files} ${header_files})

if(MSVC)
    target_compile_options(modular-psu-firmware PRIVATE "/MP")
endif()

if (UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(modular-psu-firmware Threads::Threads)    
endif (UNIX)

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
