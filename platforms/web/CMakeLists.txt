#------------------------------------------------------------------------------
# Ecstatica — web build (Emscripten)
#------------------------------------------------------------------------------
#
#   emcmake cmake -S platforms/web -B platforms/web/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build platforms/web/build
#       → platforms/web/build/dist/ (index.html, ecstatica.js, ecstatica.wasm, coi-sw.js)
#
# Like vita/ and psp/, this compiles ../../src directly; the platform surface
# is ../../src/platforms/web.c plus the shared mixer, presenter and threads.

cmake_minimum_required(VERSION 3.16)
project(ecstatica_web C)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Configure with emcmake (Emscripten's CMake wrapper)")
endif()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
file(GLOB ENGINE_SRCS ${SRC_DIR}/*.c ${SRC_DIR}/tools/*.c)
set(PLATFORM_SRCS
    ${SRC_DIR}/platforms/web.c
    ${SRC_DIR}/platforms/threads.c
    ${SRC_DIR}/platforms/mixer.c
    ${SRC_DIR}/platforms/screen.c
)

set(DIST ${CMAKE_BINARY_DIR}/dist)

add_executable(ecstatica ${ENGINE_SRCS} ${PLATFORM_SRCS})
target_include_directories(ecstatica PRIVATE ${SRC_DIR})
set_property(TARGET ecstatica PROPERTY C_STANDARD 99)
set_target_properties(ecstatica PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${DIST}
    SUFFIX ".js")

# -fsigned-char: wasm's char is already signed; kept so every target states
# the x86 assumption the engine makes (see platforms/vita/CMakeLists.txt).
target_compile_options(ecstatica PRIVATE
    -O2 -pthread -fsigned-char -fwrapv
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
    -Wno-sign-compare -Wno-unused-but-set-variable -Wno-char-subscripts)

# PROXY_TO_PTHREAD     main() on a worker, so the engine may block.
# WASMFS + OPFS        the game folder lives in the Origin Private File System.
# INITIAL_MEMORY       fixed, no growth: the page reads HEAPU8 and HEAP16 from
#                      the main thread, and growth would detach those views.
#                      The Vita runs in a 192 MB heap.
# PTHREAD_POOL_SIZE    main, log writer, prefetch, up to three raster bands,
#                      and WASMFS's OPFS proxy, with a spare.
# STACK_SIZE           deep call chains through the renderers.
target_link_options(ecstatica PRIVATE
    -O2 -pthread
    -sPROXY_TO_PTHREAD=1
    -sWASMFS=1
    -sINITIAL_MEMORY=384MB
    -sALLOW_MEMORY_GROWTH=0
    -sPTHREAD_POOL_SIZE=8
    -sSTACK_SIZE=4MB
    -sDEFAULT_PTHREAD_STACK_SIZE=1MB
    -sEXIT_RUNTIME=1
    -sMODULARIZE=0
    -sENVIRONMENT=web,worker
    -sEXPORTED_RUNTIME_METHODS=ENV
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index.html ${DIST}/index.html COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/coi-sw.js ${DIST}/coi-sw.js COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/icon.png ${DIST}/icon.png COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/banner.png ${DIST}/banner.png COPYONLY)
