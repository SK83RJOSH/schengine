cmake_minimum_required(VERSION 3.18)
project(Schengine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/publish")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


# --------------------------------------------------
# Add formatting target
include(cmake/clang-format.cmake)
setup_format_target(format_schengine)


# --------------------------------------------------
# Get version tag
find_package(Git)
if (NOT GIT_FOUND)
    SET(GIT_EXECUTABLE git)
endif()

execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty=-modified
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE SCHENGINE_GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ECHO_ERROR_VARIABLE
    COMMAND_ERROR_IS_FATAL ANY)


# --------------------------------------------------
# Run conan
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})
get_property(IS_MULTI_GENERATOR GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT IS_MULTI_GENERATOR)
    # The default option is only useful to write package functions for binary libraries
    # Unsetting the default here does not make any difference for multi-generators, yet causes CI to fail
    SET(CONAN_EXTRA_OPTIONS NO_OUTPUT_DIRS)
endif()

include(cmake/conan.cmake)
run_conan()


# --------------------------------------------------
# Add submodules
add_subdirectory(submodules)


# --------------------------------------------------
# Find packages
find_package(fmt REQUIRED)
find_package(structopt REQUIRED)

# --------------------------------------------------
# Create interface libs
add_library(schengine_warnings INTERFACE)
if(MSVC)
    target_compile_options(schengine_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(schengine_warnings INTERFACE -Wall -Wextra -pedantic -Werror)
endif()

add_library(schengine_definitions INTERFACE)
target_compile_definitions(schengine_definitions INTERFACE
    #WIN32_LEAN_AND_MEAN
    NOMINMAX)

add_library(schengine_version STATIC
        "source/version/version.h" "source/version/version.cpp")
target_include_directories(schengine_version PUBLIC "source/version")
target_compile_definitions(schengine_version PRIVATE
    SCHENGINE_VERSION=${SCHENGINE_GIT_VERSION})

add_library(schengine_dependencies INTERFACE)
target_link_libraries(schengine_dependencies INTERFACE
    fmt::fmt
    flecs)

add_library(schengine_pch INTERFACE)
target_precompile_headers(schengine_pch INTERFACE
    <string>
    <string_view>
    <array>
    <vector>
    <deque>
    <map>
    <unordered_map>
    <set>
    <unordered_set>
    <fstream>
    <regex>
    <filesystem>
    <variant>
    <functional>
    <algorithm>
    <codecvt>
    <iomanip>
    <locale>
    <mutex>)


# --------------------------------------------------
# Create engine lib
file(GLOB_RECURSE schengine_sources CONFIGURE_DEPENDS "source/engine/*.cpp")
file(GLOB_RECURSE schengine_headers CONFIGURE_DEPENDS "source/engine/*.h" "source/engine/*.inl")
add_library(schengine_lib STATIC ${schengine_sources} ${schengine_headers})
target_link_libraries(schengine_lib PRIVATE
    schengine_warnings
    schengine_definitions
    schengine_dependencies
    schengine_pch
    schengine_version)
target_include_directories(schengine_lib PUBLIC "source/engine")


# --------------------------------------------------
# Create executbale
file(GLOB_RECURSE schengine_app_sources CONFIGURE_DEPENDS "source/app/*.cpp")
file(GLOB_RECURSE schengine_app_headers CONFIGURE_DEPENDS "source/app/*.h" "source/app/*.inl")
xwin_add_executable(schengine_app "${schengine_app_sources};${schengine_app_headers}")
target_link_libraries(schengine_app PRIVATE
    schengine_warnings
    schengine_definitions
    schengine_dependencies
    schengine_pch
    schengine_lib
    schengine_version
    structopt::structopt
    CrossWindow)
target_include_directories(schengine_app PRIVATE "source/app")
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT schengine_app)
configure_diligent(schengine_app)


# --------------------------------------------------
# Merge files from source and include in the IDE
function(group_files sources)
    foreach(FILE ${sources})
        # Get the directory of the source file
        get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

        # Remove common directory prefix to make the group
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")

        # Make sure we are using windows slashes
        string(REPLACE "/" "\\" GROUP "${GROUP}")

        # Strip the root parts for each possible component
        if("${FILE}" MATCHES "source/app/.*")
            string(SUBSTRING ${GROUP} 11 -1 GROUP)
        elseif("${FILE}" MATCHES "source/engine/.*")
            string(SUBSTRING ${GROUP} 14 -1 GROUP)
        endif()

        # Do the grouping
        source_group("${GROUP}" FILES "${FILE}")
    endforeach()
endfunction()
group_files("${schengine_sources};${schengine_app_sources};${schengine_headers};${schengine_app_headers}")


# --------------------------------------------------
# Install app
install(TARGETS
    schengine_app
    RUNTIME
    DESTINATION .)


# --------------------------------------------------
# Map Release config to MinSizeRel and RelWithDebInfo because conan is borked
set_target_properties(schengine_lib PROPERTIES
    MAP_IMPORTED_CONFIG_MINSIZEREL Release
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
set_target_properties(schengine_app PROPERTIES
    MAP_IMPORTED_CONFIG_MINSIZEREL Release
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)