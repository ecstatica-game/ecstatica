#------------------------------------------------------------------------------
# Ecstatica — Android native library (libecstatica.so)
#------------------------------------------------------------------------------
#
# Built once per ABI by build.sh with the NDK's toolchain file, which then
# packs the libraries, the launcher's Java code and the manifest into an APK.
# Like vita/ and psp/, this compiles ../../src directly.

cmake_minimum_required(VERSION 3.16)
project(ecstatica_android C)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
file(GLOB ENGINE_SRCS ${SRC_DIR}/*.c ${SRC_DIR}/tools/*.c)
set(GLUE ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

add_library(ecstatica SHARED
    ${ENGINE_SRCS}
    ${SRC_DIR}/platforms/android.c
    ${SRC_DIR}/platforms/threads.c
    ${SRC_DIR}/platforms/mixer.c
    ${SRC_DIR}/platforms/screen.c
    ${SRC_DIR}/platforms/touch_controls.c
    ${GLUE})
target_include_directories(ecstatica PRIVATE ${SRC_DIR} ${ANDROID_NDK}/sources/android/native_app_glue)
set_property(TARGET ecstatica PROPERTY C_STANDARD 99)

# main() becomes ecs_main, called by android_main once there is a window.
set_source_files_properties(${SRC_DIR}/main.c PROPERTIES COMPILE_DEFINITIONS main=ecs_main)

# -fsigned-char: char is unsigned on ARM but signed on the x86 the original
# binary targeted, and the engine keeps signed data in plain char arrays.
target_compile_options(ecstatica PRIVATE
    -O2 -fsigned-char -fwrapv
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
    -Wno-sign-compare -Wno-unused-but-set-variable -Wno-char-subscripts)

# ANativeActivity_onCreate lives in the glue and must survive --gc-sections.
target_link_options(ecstatica PRIVATE -u ANativeActivity_onCreate)
target_link_libraries(ecstatica PRIVATE android aaudio log m)
