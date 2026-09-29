#------------------------------------------------------------------------------
# Ecstatica — PlayStation Vita build
#------------------------------------------------------------------------------
#
#   cmake -S vita -B vita/build && cmake --build vita/build   → ecstatica.vpk
#   cmake -S vita -B vita/build -DPROFILE=ON                   → + prof.log
#
# Needs VitaSDK with $VITASDK set. Like psp/ and dos/, this compiles ../src
# directly; the whole platform surface is ../src/platforms/vita.c.

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "VITASDK is not set — install VitaSDK and export VITASDK")
    endif()
endif()

project(ecstatica C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Ecstatica")
set(VITA_TITLEID  "ECST00001")
set(VITA_VERSION  "01.00")

option(PROFILE "Frame-phase profiler writing prof.log" OFF)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
file(GLOB ENGINE_SRCS ${SRC_DIR}/*.c ${SRC_DIR}/tools/*.c)

add_executable(ecstatica ${ENGINE_SRCS} ${SRC_DIR}/platforms/vita.c)
target_include_directories(ecstatica PRIVATE ${SRC_DIR})
set_property(TARGET ecstatica PROPERTY C_STANDARD 99)

# -fsigned-char: `char` is unsigned on ARM but signed on the x86 the original
# Watcom binary targeted, and the engine keeps signed data in plain char arrays
# (shade_tab, shadow_tab, the bitmap planes).
target_compile_options(ecstatica PRIVATE
    -O2 -Wall -Wextra -fwrapv -fsigned-char
    -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare
    -Wno-unused-but-set-variable -Wno-char-subscripts)
if(PROFILE)
    target_compile_definitions(ecstatica PRIVATE ECS_PROFILE)
endif()

target_link_libraries(ecstatica
    SceDisplay_stub
    SceCtrl_stub
    SceTouch_stub
    SceAudio_stub
    ScePower_stub
    m
)

vita_create_self(eboot.bin ecstatica UNSAFE)
vita_create_vpk(ecstatica.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    FILE sce_sys/icon0.png sce_sys/icon0.png
         sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
         sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
         sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
