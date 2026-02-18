cmake_minimum_required(VERSION 3.16)

# =============================================================================
# AstroSpike Cross-Platform CMakeLists.txt
# =============================================================================
# Supports both Windows (MinGW) and macOS (Homebrew)
# =============================================================================

# Read version from changelog.txt
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/changelog.txt")
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/changelog.txt" VERSION_LINE REGEX "^Version [0-9.]+")
    if(VERSION_LINE)
        list(GET VERSION_LINE 0 FIRST_VERSION_LINE)
        string(REGEX MATCH "[0-9.]+" APP_VERSION "${FIRST_VERSION_LINE}")
    else()
        set(APP_VERSION "1.0.0")
    endif()
else()
    set(APP_VERSION "1.0.0")
endif()

project(AstroSpike LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# =============================================================================
# Platform-Specific Settings
# =============================================================================
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_MACOSX_BUNDLE ON)
    message(STATUS "Building for macOS")
elseif(WIN32)
    # Disable AutoMoc predefs on Windows
    set(CMAKE_AUTOMOC_COMPILER_PREDEFS OFF CACHE BOOL "" FORCE)
    message(STATUS "Building for Windows")
endif()

# =============================================================================
# Homebrew Detection (macOS)
# =============================================================================
if(APPLE)
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    if(NOT HOMEBREW_PREFIX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(HOMEBREW_PREFIX "/opt/homebrew")
        else()
            set(HOMEBREW_PREFIX "/usr/local")
        endif()
    endif()
    
    message(STATUS "Homebrew prefix: ${HOMEBREW_PREFIX}")
    
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}/opt/qt")
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}/opt/qt@6")
    
    # Qt6 CMake Dir Helper
    if(NOT Qt6_DIR)
        if(EXISTS "${HOMEBREW_PREFIX}/opt/qt/lib/cmake/Qt6/Qt6Config.cmake")
            set(Qt6_DIR "${HOMEBREW_PREFIX}/opt/qt/lib/cmake/Qt6" CACHE PATH "Path to Qt6")
        elseif(EXISTS "${HOMEBREW_PREFIX}/opt/qtbase/lib/cmake/Qt6/Qt6Config.cmake")
            set(Qt6_DIR "${HOMEBREW_PREFIX}/opt/qtbase/lib/cmake/Qt6" CACHE PATH "Path to Qt6")
        endif()
    endif()
endif()

# =============================================================================
# Find Qt6
# =============================================================================
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core Concurrent Xml Network)
message(STATUS "Qt6 found")

# =============================================================================
# OpenMP
# =============================================================================
if(APPLE)
    execute_process(
        COMMAND brew --prefix libomp
        OUTPUT_VARIABLE LIBOMP_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    if(LIBOMP_PREFIX AND EXISTS "${LIBOMP_PREFIX}")
        message(STATUS "Using Homebrew libomp from: ${LIBOMP_PREFIX}")
        add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
        set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
            INTERFACE_COMPILE_OPTIONS "-Xpreprocessor;-fopenmp"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBOMP_PREFIX}/include"
            INTERFACE_LINK_LIBRARIES "${LIBOMP_PREFIX}/lib/libomp.dylib"
        )
    else()
        find_package(OpenMP REQUIRED)
    endif()
else()
    find_package(OpenMP REQUIRED)
endif()

# =============================================================================
# Dependencies Setup
# =============================================================================
set(DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")


# CFITSIO
if(APPLE)
    execute_process(COMMAND brew --prefix cfitsio OUTPUT_VARIABLE CFITSIO_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(CFITSIO_PREFIX AND EXISTS "${CFITSIO_PREFIX}")
        set(CFITSIO_INCLUDE_DIR "${CFITSIO_PREFIX}/include")
        set(CFITSIO_LIBRARY "${CFITSIO_PREFIX}/lib/libcfitsio.dylib")
        message(STATUS "Using Homebrew CFITSIO")
    else()
        message(FATAL_ERROR "CFITSIO not found via Homebrew")
    endif()
else()
    # Windows/Local
    set(CFITSIO_DIR "${DEPS_DIR}/cfitsio")
    if(EXISTS "${CFITSIO_DIR}")
        set(CFITSIO_INCLUDE_DIR "${CFITSIO_DIR}/include")
        set(CFITSIO_LIBRARY_DIR "${CFITSIO_DIR}/lib")
        set(CFITSIO_LIBRARY cfitsio)
        link_directories(${CFITSIO_LIBRARY_DIR})
    else()
        message(FATAL_ERROR "CFITSIO not found in deps/cfitsio")
    endif()
endif()

# OpenCV
if(APPLE)
    execute_process(COMMAND brew --prefix opencv OUTPUT_VARIABLE OPENCV_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(OPENCV_PREFIX AND EXISTS "${OPENCV_PREFIX}")
        set(OpenCV_DIR "${OPENCV_PREFIX}/lib/cmake/opencv4")
    endif()
    find_package(OpenCV REQUIRED)
else()
    set(OpenCV_DIR "${DEPS_DIR}/opencv")
    if(EXISTS "${OpenCV_DIR}")
        find_package(OpenCV REQUIRED PATHS "${OpenCV_DIR}" NO_DEFAULT_PATH)
    else()
        find_package(OpenCV REQUIRED)
    endif()
endif()

# =============================================================================
# Source Files
# =============================================================================
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.h")

set(RESOURCE_FILES "")
if(WIN32)
    set(RESOURCE_FILES "src/AstroSpike.rc")
endif()

set(QRCS "src/resources.qrc")

# =============================================================================
# Executable Target
# =============================================================================
if(APPLE)
    # macOS App Bundle
    set(MACOSX_BUNDLE_ICON_FILE AstroSpike.icns)
    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/src/images/AstroSpike.icns")
    
    # Bundle config
    set(BUNDLE_SOURCES ${SOURCES} ${HEADERS} ${QRCS})
    if(EXISTS "${APP_ICON_MACOS}")
        set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        list(APPEND BUNDLE_SOURCES ${APP_ICON_MACOS})
    endif()
    
    add_executable(AstroSpike MACOSX_BUNDLE ${BUNDLE_SOURCES})
    
    set_target_properties(AstroSpike PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "AstroSpike"
        MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.astrospike"
    )
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist")
        set_target_properties(AstroSpike PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist"
        )
    endif()
else()
    # Windows
    add_executable(AstroSpike ${SOURCES} ${HEADERS} ${RESOURCE_FILES} ${QRCS})
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(AstroSpike PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/io"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/algos"
    ${CFITSIO_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)


# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(AstroSpike PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::Concurrent
    Qt6::Xml
    Qt6::Network
    OpenMP::OpenMP_CXX
    ${CFITSIO_LIBRARY}
    ${OpenCV_LIBS}
    z
)

message(STATUS "AstroSpike build configured successfully")
