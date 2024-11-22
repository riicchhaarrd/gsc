cmake_minimum_required(VERSION 3.10)
project(gsc VERSION 0.1 LANGUAGES C)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if("${CMAKE_C_COMPILER}" MATCHES "emcc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DEMSCRIPTEN -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_FUNCTIONS='[\'_main\',\'_gsc_execute_file\',\'_malloc\',\'_free\']' -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,stringToUTF8")
    # set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -O2")
else()
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -O2")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -g -ggdb3")
        # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -g -ggdb3 -fsanitize=address -O0 -D_DEBUG")
    endif()
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-builtin-macro-redefined -D'__FILE__=\"\"'")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-error=deprecated")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
endif()
#set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGSC_STANDALONE")

include_directories(../../../include)
set(SOURCES
    visitor.c
    compiler.c
    traverse.c
    parse.c
    vm.c
    main.c
    precedence.c
)
if (NOT MSVC)
add_library(libgsc STATIC ${SOURCES} library.c)
else()
add_library(libgsc SHARED ${SOURCES} library.c)
endif()
target_include_directories(libgsc PRIVATE include)
target_compile_definitions(libgsc PRIVATE BUILD_LIB)
set_property(TARGET libgsc PROPERTY OUTPUT_NAME gsc)

set_target_properties(libgsc PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "gsc"
)

target_include_directories(libgsc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if (MSVC)
    target_compile_definitions(libgsc PRIVATE GSC_EXPORTS)
    target_compile_options(libgsc PRIVATE /W4 /WX)  # Warning level 4, treat warnings as errors
else()
    target_compile_definitions(libgsc PRIVATE GSC_EXPORTS)
    #target_compile_options(libgsc PRIVATE -Wall -Wextra -Werror -fPIC -fvisibility=hidden)
    target_compile_options(libgsc PRIVATE -w -fPIC -fvisibility=hidden)
    target_link_libraries(libgsc PRIVATE m)
    
    # target_compile_options(gsc PRIVATE -Wall -Wextra -Werror -fPIC -fvisibility=hidden)
    # target_link_libraries(gsc m)
endif()

add_executable(gsc examples/gsc.c examples/functions.c)
target_link_libraries(gsc PRIVATE libgsc)
if (NOT MSVC)
	target_link_libraries(gsc PRIVATE m)
endif()

if (NOT EMSCRIPTEN AND NOT MSVC)
	if (CMAKE_BUILD_TYPE STREQUAL "Release")
	add_custom_command(
			TARGET libgsc
			POST_BUILD
			COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:libgsc>
			COMMENT "Stripping unneeded symbols from libgsc"
			)
	endif()
endif()
