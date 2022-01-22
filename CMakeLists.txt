cmake_minimum_required(VERSION 3.10)

project(modular-psu-firmware)

set (CMAKE_CXX_STANDARD 14)

if(MSVC)
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /W4 /wd4100 /wd4214 /wd4200 /wd4244 /wd4456 /wd4457 /wd4458 /wd4459 /wd4245 /wd4389 /wd4706 /wd4611 /wd4310")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wunused-const-variable -O2 -s DEMANGLE_SUPPORT=1 -s FORCE_FILESYSTEM=1 -s ALLOW_MEMORY_GROWTH=1 -s TOTAL_MEMORY=83886080 -lidbfs.js")
    #set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} --preload-file ../../images/eez.png")
endif()

add_definitions(-DFIRMWARE_VERSION_MAJOR=1)
add_definitions(-DFIRMWARE_VERSION_MINOR=8.alpha3)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='[png]'")
else()
    set(SDL2_BUILDING_LIBRARY 1)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)
    include_directories(SYSTEM ${SDL2_INCLUDE_DIRS})
    include_directories(SYSTEM ${SDL2IMAGE_INCLUDE_DIR})
    add_definitions(-DOPTION_ETHERNET=1)
endif()

if (UNIX)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fpermissive -pedantic")
endif (UNIX)

add_definitions(-DDEBUG)

add_definitions(-DHAVE_STRTOLL)
add_definitions(-DHAVE_STDBOOL)
add_definitions(-DSCPI_USER_CONFIG)

add_definitions(-DOPTION_DISPLAY=1)
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
    src/eez/libs/agg
    src/eez/libs/libscpi/inc
    src/bb3/libs/mqtt
    src/eez/platform/simulator
    src/bb3/scpi
    src/bb3/conf
)

include_directories(SYSTEM
    src/third_party/micropython
    src/third_party/micropython/ports/bb3
)
if(WIN32)
    include_directories(src/third_party/micropython/ports/bb3/win32)
endif()


set(src_files)
set(header_files)


################################################################################
# EEZ Framework files
file(GLOB_RECURSE src_eez
    src/eez/*.cpp
    src/eez/*.c
)
file(GLOB_RECURSE header_eez
    src/eez/*.h
)

# exclude STM32 platform specific files
set (EXCLUDE_DIR "src/eez/platform/stm32")
foreach (TMP_PATH ${src_eez})
    string (FIND ${TMP_PATH} ${EXCLUDE_DIR} EXCLUDE_DIR_FOUND)
    if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
        list (REMOVE_ITEM src_eez ${TMP_PATH})
    endif ()
endforeach(TMP_PATH)
foreach (TMP_PATH ${header_eez})
    string (FIND ${TMP_PATH} ${EXCLUDE_DIR} EXCLUDE_DIR_FOUND)
    if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
        list (REMOVE_ITEM header_eez ${TMP_PATH})
    endif ()
endforeach(TMP_PATH)

list (APPEND src_files ${src_eez})
list (APPEND header_files ${header_eez})

source_group(TREE "../../src/eez" PREFIX "eez" FILES ${src_eez} ${header_eez})

# TODO
#if(MSVC)
#    set_source_files_properties(${src_eez_libs_libscpi} PROPERTIES COMPILE_FLAGS /W0)
#endif()

################################################################################


################################################################################
# BB3 files
file(GLOB_RECURSE src_bb3
    src/bb3/*.cpp
    src/bb3/*.c
)
file(GLOB_RECURSE header_bb3
    src/bb3/*.h
)

# exclude STM32 platform specific files
set (EXCLUDE_DIR "src/bb3/platform/stm32")
foreach (TMP_PATH ${src_bb3})
    string (FIND ${TMP_PATH} ${EXCLUDE_DIR} EXCLUDE_DIR_FOUND)
    if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
        list (REMOVE_ITEM src_bb3 ${TMP_PATH})
    endif ()
endforeach(TMP_PATH)
foreach (TMP_PATH ${header_bb3})
    string (FIND ${TMP_PATH} ${EXCLUDE_DIR} EXCLUDE_DIR_FOUND)
    if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
        list (REMOVE_ITEM header_bb3 ${TMP_PATH})
    endif ()
endforeach(TMP_PATH)

# exclude nanojpeg.c
get_filename_component(full_path_nanojpeg_c ${CMAKE_CURRENT_SOURCE_DIR}/src/bb3/libs/image/nanojpeg.c ABSOLUTE)
list(REMOVE_ITEM src_bb3 "${full_path_nanojpeg_c}")

list (APPEND src_files ${src_bb3})
list (APPEND header_files ${header_bb3})

source_group(TREE "../../src/bb3" PREFIX "bb3" FILES ${src_bb3} ${header_bb3})

#TODO
#if(MSVC)
#    set_source_files_properties(src/bb3/libs/image/toojpeg.cpp PROPERTIES COMPILE_FLAGS /W0)
#endif()

################################################################################


################################################################################
# MicroPython files

set(micropython_src_files)
set(micropython_header_files)

set(src_third_party_micropython_extmod
    src/third_party/micropython/extmod/virtpin.c
    src/third_party/micropython/extmod/utime_mphal.c
)
list (APPEND micropython_src_files ${src_third_party_micropython_extmod})
set(header_third_party_micropython_extmod
    src/third_party/micropython/extmod/virtpin.h
    src/third_party/micropython/extmod/utime_mphal.h
)
list (APPEND micropython_header_files ${header_third_party_micropython_extmod})

set(src_third_party_micropython_ports_bb3
    src/third_party/micropython/ports/bb3/main.c
    src/third_party/micropython/ports/bb3/gccollect.cpp
    src/third_party/micropython/ports/bb3/mphalport.cpp
)
list (APPEND micropython_src_files ${src_third_party_micropython_ports_bb3})
set(header_third_party_micropython_ports_bb3
    src/third_party/micropython/ports/bb3/mpconfigport.h
    src/third_party/micropython/ports/bb3/mphalport.h
    src/third_party/micropython/ports/bb3/qstrdefsport.h
)
list (APPEND micropython_header_files ${header_third_party_micropython_ports_bb3})

set(header_third_party_micropython_ports_bb3_genhdr
    src/third_party/micropython/ports/bb3/genhdr/moduledefs.h
    src/third_party/micropython/ports/bb3/genhdr/mpversion.h
    src/third_party/micropython/ports/bb3/genhdr/qstrdefs.generated.h
)
list (APPEND micropython_header_files ${header_third_party_micropython_ports_bb3_genhdr})

set(src_third_party_micropython_ports_bb3_mod_eez
    src/third_party/micropython/ports/bb3/mod/eez/modeez_table.c
    src/third_party/micropython/ports/bb3/mod/eez/modeez.cpp
)
list (APPEND micropython_src_files ${src_third_party_micropython_ports_bb3_mod_eez})
set(header_third_party_micropython_ports_bb3_mod_eez
    src/third_party/micropython/ports/bb3/mod/eez/modeez.h
)
list (APPEND micropython_header_files ${header_third_party_micropython_ports_bb3_mod_eez})

set(src_third_party_micropython_ports_bb3_mod_utime
    src/third_party/micropython/ports/bb3/mod/utime/modutime.c
)
list (APPEND micropython_src_files ${src_third_party_micropython_ports_bb3_mod_utime})

file(GLOB src_third_party_micropython_py
    src/third_party/micropython/py/*.c
)
list (APPEND micropython_src_files ${src_third_party_micropython_py})
file(GLOB header_third_party_micropython_py
    *.h
)
list (APPEND micropython_header_files ${header_third_party_micropython_py})

if (UNIX)
    set_source_files_properties(${micropython_src_files} PROPERTIES COMPILE_FLAGS -fpermissive)
    set_source_files_properties(${micropython_src_files} PROPERTIES COMPILE_FLAGS -Wpointer-to-int-cast)
    set_source_files_properties(${micropython_src_files} PROPERTIES COMPILE_FLAGS -Wint-to-pointer-cast)
endif()
if(MSVC)
    set_source_files_properties(${micropython_src_files} PROPERTIES COMPILE_FLAGS /W0)
endif()

list (APPEND src_files ${micropython_src_files})
list (APPEND header_files ${micropython_header_files})

source_group(TREE "../../src/third_party/micropython" PREFIX "third_party/micropython" FILES ${micropython_src_files} ${micropython_header_files})

################################################################################

if(WIN32)
    set(src_win32_specific
        src/bb3/platform/simulator/win32/icon.rc
    ) 
    list (APPEND src_files ${src_win32_specific})
    source_group("win32" FILES ${src_win32_specific})

    set(SOURCES src/bb3/platform/simulator/win32/icon.rc ${src_files})
endif()

add_executable(modular-psu-firmware ${src_files} ${header_files})

if(MSVC)
    target_compile_options(modular-psu-firmware PRIVATE "/MP")
endif()

if (UNIX AND NOT(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten"))
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(modular-psu-firmware Threads::Threads bsd)
endif ()

target_link_libraries(modular-psu-firmware ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})

if(WIN32)
    target_link_libraries(modular-psu-firmware wsock32 ws2_32)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_custom_command(TARGET modular-psu-firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/../SDL2-2.0.14/lib/x64/SDL2.dll"
                $<TARGET_FILE_DIR:modular-psu-firmware>)

        add_custom_command(TARGET modular-psu-firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x64/libjpeg-9.dll"
                $<TARGET_FILE_DIR:modular-psu-firmware>)
        
        add_custom_command(TARGET modular-psu-firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x64/libpng16-16.dll"
                $<TARGET_FILE_DIR:modular-psu-firmware>)

        add_custom_command(TARGET modular-psu-firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x64/libtiff-5.dll"
                $<TARGET_FILE_DIR:modular-psu-firmware>)

        add_custom_command(TARGET modular-psu-firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x64/libwebp-7.dll"
                $<TARGET_FILE_DIR:modular-psu-firmware>)

        add_custom_command(TARGET modular-psu-firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x64/SDL2_image.dll"
                $<TARGET_FILE_DIR:modular-psu-firmware>)

        add_custom_command(TARGET modular-psu-firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/../SDL2_image-2.0.4/lib/x64/zlib1.dll"
                $<TARGET_FILE_DIR:modular-psu-firmware>)
    else()
        add_custom_command(TARGET modular-psu-firmware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/../SDL2-2.0.14/lib/x86/SDL2.dll"
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
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    add_custom_command(TARGET modular-psu-firmware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/src/bb3/platform/simulator/emscripten"
        $<TARGET_FILE_DIR:modular-psu-firmware>)
endif()
