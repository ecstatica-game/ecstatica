#------------------------------------------------------------------------------
# Ecstatica — PlayStation Vita build
#------------------------------------------------------------------------------
#
#   cmake -S vita -B vita/build && cmake --build vita/build
#       → ecstatica-e1.vpk (ECST00001, "Ecstatica",    ux0:data/ecstatica/e1)
#       → ecstatica-e2.vpk (ECST00002, "Ecstatica II", ux0:data/ecstatica/e2)
#   cmake -S vita -B vita/build -DPROFILE=ON   → both, writing prof.log
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

set(VITA_VERSION "01.00")

option(PROFILE "Frame-phase profiler writing prof.log" OFF)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
file(GLOB ENGINE_SRCS ${SRC_DIR}/*.c ${SRC_DIR}/tools/*.c)

# One VPK per game, each its own LiveArea bubble opening its own data folder.
# The engine is the same; only VITA_GAME_DIR and the package metadata differ.
function(ecstatica_vpk game title_id app_name)
    set(exe ecstatica_${game})
    add_executable(${exe} ${ENGINE_SRCS} ${SRC_DIR}/platforms/vita.c)
    target_include_directories(${exe} PRIVATE ${SRC_DIR})
    set_property(TARGET ${exe} PROPERTY C_STANDARD 99)
    target_compile_definitions(${exe} PRIVATE VITA_GAME_DIR="${game}")

    # -fsigned-char: `char` is unsigned on ARM but signed on the x86 the
    # original Watcom binary targeted, and the engine keeps signed data in
    # plain char arrays (shade_tab, shadow_tab, the bitmap planes).
    target_compile_options(${exe} PRIVATE
        -O2 -Wall -Wextra -fwrapv -fsigned-char
        -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare
        -Wno-unused-but-set-variable -Wno-char-subscripts)
    if(PROFILE)
        target_compile_definitions(${exe} PRIVATE ECS_PROFILE)
    endif()

    target_link_libraries(${exe}
        SceDisplay_stub
        SceCtrl_stub
        SceTouch_stub
        SceAudio_stub
        ScePower_stub
        m
    )

    set(sys sce_sys/${game})
    vita_create_self(eboot_${game}.bin ${exe} UNSAFE)
    vita_create_vpk(ecstatica-${game}.vpk ${title_id} eboot_${game}.bin
        VERSION ${VITA_VERSION}
        NAME "${app_name}"
        FILE ${sys}/icon0.png sce_sys/icon0.png
             ${sys}/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
             ${sys}/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
             ${sys}/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
    )
endfunction()

ecstatica_vpk(e1 ECST00001 "Ecstatica")
ecstatica_vpk(e2 ECST00002 "Ecstatica II")
