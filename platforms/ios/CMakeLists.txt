#------------------------------------------------------------------------------
# Ecstatica — iOS / iPadOS app
#------------------------------------------------------------------------------
#
#   cmake -S platforms/ios -B platforms/ios/build -G Xcode -DCMAKE_SYSTEM_NAME=iOS
#   cmake --build platforms/ios/build --config Release -- -sdk iphonesimulator
#   cmake --build platforms/ios/build --config Release -- -sdk iphoneos CODE_SIGNING_ALLOWED=NO
#
# `make ios-sim` and `make ios` wrap these; see README.md. Like vita/ and
# psp/, this compiles ../../src directly.

cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "Configure with -G Xcode -DCMAKE_SYSTEM_NAME=iOS")
endif()

project(ecstatica_ios C OBJC)

set(GAME_VERSION "" CACHE STRING "Release version, e.g. v0.9.5")
set(BUNDLE_ID "io.github.ecstaticagame.ecstatica" CACHE STRING "Bundle identifier")
set(DEVELOPMENT_TEAM "" CACHE STRING "Apple team ID for signed device builds")

set(APP_VERSION "0.0.0")
if(GAME_VERSION MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(APP_VERSION ${CMAKE_MATCH_1})
endif()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
file(GLOB ENGINE_SRCS ${SRC_DIR}/*.c ${SRC_DIR}/tools/*.c)
set(ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/Assets.xcassets)

add_executable(Ecstatica MACOSX_BUNDLE
    ${ENGINE_SRCS}
    ${SRC_DIR}/platforms/ios.m
    ${SRC_DIR}/platforms/threads.c
    ${SRC_DIR}/platforms/mixer.c
    ${SRC_DIR}/platforms/screen.c
    ${SRC_DIR}/platforms/touch_controls.c
    ${ASSETS})
target_include_directories(Ecstatica PRIVATE ${SRC_DIR})
set_property(TARGET Ecstatica PROPERTY C_STANDARD 99)
set_source_files_properties(${ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

# main() becomes ecs_main; ios.m has the app's main and runs it on a thread.
set_source_files_properties(${SRC_DIR}/main.c PROPERTIES COMPILE_DEFINITIONS main=ecs_main)

# -fsigned-char: char is unsigned on ARM but signed on the x86 the original
# binary targeted, and the engine keeps signed data in plain char arrays.
target_compile_options(Ecstatica PRIVATE
    -fsigned-char -fwrapv
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
    -Wno-sign-compare -Wno-unused-but-set-variable -Wno-char-subscripts
    $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>)

target_link_libraries(Ecstatica PRIVATE
    "-framework UIKit" "-framework GameController" "-framework AVFoundation"
    "-framework AudioToolbox" "-framework QuartzCore" "-framework CoreGraphics"
    "-framework UniformTypeIdentifiers")

set_target_properties(Ecstatica PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${APP_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${APP_VERSION}
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID}
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 15.0
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
    XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL[variant=Release] 2
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${DEVELOPMENT_TEAM}"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
