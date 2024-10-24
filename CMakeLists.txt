cmake_minimum_required(VERSION 3.10)
project(gsc)
if("${CMAKE_C_COMPILER}" MATCHES "emcc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DEMSCRIPTEN -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_FUNCTIONS='[\'_main\',\'_gsc_execute_file\',\'_malloc\',\'_free\']' -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,stringToUTF8")
    # set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -O2")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -g -ggdb3")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-builtin-macro-redefined -D'__FILE__=\"\"'")

include_directories(../../../include)
set(SOURCES
    visitor.c
    gsc.c
    compiler.c
    traverse.c
    functions.c
    parse.c
    vm.c
    main.c
    platform.c
)
add_executable(gsc ${SOURCES})

include(basename.cmake)
define_file_basename_for_sources(gsc)

target_link_libraries(gsc m)
