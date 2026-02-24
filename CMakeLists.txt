cmake_minimum_required(VERSION 3.15)
project(AVSLib VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release if not specified (avoids -O0 builds)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_EXAMPLES "Build example applications" ON)

# Auto-discover source files
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
    core/*.cpp
    core/script/*.cpp
)
file(GLOB EFFECT_SOURCES CONFIGURE_DEPENDS
    effects/*.cpp
)
# stb_image implementation
set(THIRD_PARTY_SOURCES third_party/stb_image_impl.cpp)

# Core library
add_library(avs_core STATIC ${CORE_SOURCES} ${THIRD_PARTY_SOURCES})
target_include_directories(avs_core PUBLIC . core third_party)

# Built-in effects library
add_library(avs_effects STATIC ${EFFECT_SOURCES})
target_link_libraries(avs_effects PUBLIC avs_core)
target_include_directories(avs_effects PUBLIC . core effects)

# Example application
if(BUILD_EXAMPLES)
    add_executable(avs_example
        example/src/main.cpp
    )
    target_link_libraries(avs_example PRIVATE avs_core avs_effects)
    target_include_directories(avs_example PRIVATE .)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(avs_core PRIVATE /W4)
    target_compile_options(avs_effects PRIVATE /W4)
else()
    target_compile_options(avs_core PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(avs_effects PRIVATE -Wall -Wextra -pedantic)
endif()
message(STATUS "AVS Library Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
