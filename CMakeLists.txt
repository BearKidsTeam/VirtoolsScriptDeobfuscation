cmake_minimum_required(VERSION 3.12)

project(VirtoolsLoader VERSION 2.0.0.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED NO)
set(CMAKE_CXX_EXTENSIONS YES)

# Add path for custom modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Use folders to organize targets in an IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

if (NOT WIN32)
    message(FATAL_ERROR "Only support Windows.")
endif ()

# Use relative paths
if (WIN32)
    set(CMAKE_USE_RELATIVE_PATHS TRUE)
    set(CMAKE_SUPPRESS_REGENERATION TRUE)
endif ()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as no build type was specified")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type (Debug/Release)" FORCE)
endif ()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "The root directory of the installation" FORCE)
    message(STATUS "Setting default install directory to ${CMAKE_INSTALL_PREFIX} as no install directory was specified")
endif ()

# Generate a CompilationDatabase (compile_commands.json)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_package(VirtoolsSDK REQUIRED)

add_subdirectory(minhook EXCLUDE_FROM_ALL)

configure_file(Version.h.in "${CMAKE_CURRENT_SOURCE_DIR}/Version.h")

add_library(VirtoolsLoader SHARED
        NemoLoader.cpp
        InterfaceData.cpp InterfaceData.h
        Decorator.cpp Decorator.h
        GraphBuilder.cpp GraphBuilder.h
        LayoutCalculator.cpp LayoutCalculator.h
        VirtoolsLoader.rc)
target_include_directories(VirtoolsLoader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VirtoolsLoader PRIVATE CK2 VxMath minhook)