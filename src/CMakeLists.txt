cmake_minimum_required(VERSION 3.20)
project(ecstatica C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(C_SRCS
    init.c
    asm_f.c
    display.c
    debug_overlay.c
    edit.c
    game.c
    file.c
    move.c
    menu.c
    req.c
    music.c
    topo.c
    map.c
    anim.c
    ellipse.c
    tri.c
    icon.c
    win.c
    chars.c
    main.c
)

if(APPLE)
    enable_language(OBJC)
    list(APPEND C_SRCS platforms/macos.m)
elseif(WIN32)
    list(APPEND C_SRCS platforms/windows.c)
elseif(UNIX)
    list(APPEND C_SRCS platforms/linux.c)
endif()

add_executable(ecstatica ${C_SRCS})

target_include_directories(ecstatica PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(ecstatica PRIVATE /W3 /std:c17)
    target_compile_definitions(ecstatica PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(ecstatica PRIVATE
        -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -fwrapv
    )
    if(APPLE)
        target_compile_options(ecstatica PRIVATE
            $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>
        )
    endif()
endif()

target_compile_definitions(ecstatica PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

if(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit REQUIRED)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
    find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)

    target_link_libraries(ecstatica PRIVATE
        ${COCOA_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AVFOUNDATION_FRAMEWORK}
        m
    )
elseif(WIN32)
    target_link_libraries(ecstatica PRIVATE
        gdi32
        user32
        winmm
    )
elseif(UNIX)
    find_package(X11 REQUIRED)
    find_library(ALSA_LIB asound REQUIRED)

    target_include_directories(ecstatica PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(ecstatica PRIVATE
        ${X11_LIBRARIES}
        ${ALSA_LIB}
        pthread
        m
    )
endif()

set_target_properties(ecstatica PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
