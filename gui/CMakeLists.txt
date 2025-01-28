# WE use
# 3.21 file(COPY_FILE ..)
# 3.20: add_compile_definitions 
# Bump to 3.25 for LINUX, and kill compat code below
cmake_minimum_required (VERSION 3.21)

# This needs to be set at the top
# We need that for TLS
set(CMAKE_OSX_DEPLOYMENT_TARGET 12)

# Set cmake modules directory (i.e. the one which contains all user-defined FindXXX.cmake files among other things)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# include (CMakeDependentOption)

project (ReSHOP_GUI LANGUAGES C CXX
                    VERSION          "0.3.2"
                    HOMEPAGE_URL     "https://reshop.eu/")

#custom stuff
#include(compiler_flags)
#include(linker_options)

# Keep that on top
set (CMAKE_C_STANDARD 17)
set (CMAKE_C_STANDARD_REQUIRED ON)
set (CMAKE_C_EXTENSIONS OFF)
set (CMAKE_C_VISIBILITY_PRESET hidden)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
   set(WINDOWS 1)
   # Poor man's detection of not using MSVC libc
   # Can't use MSVC as the compiler may try not to emulate cl.exe
   if (NOT CYGWIN AND NOT MINGW)
      add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
      add_compile_definitions(_CRT_NONSTDC_NO_WARNINGS)
   endif()
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (CMAKE_VERSION VERSION_LESS 3.25)
   if (CMAKE_SYSTEM_NAME MATCHES "Linux")
      set(LINUX 1)
   endif()
endif()

#Set output directory
set (OUTPUT_DIR ${CMAKE_BINARY_DIR}/output)
file(MAKE_DIRECTORY ${OUTPUT_DIR})

# Use C++14 (for PPL and backward)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_CXX_VISIBILITY_PRESET hidden)

include(sanitizers-profile)

# Some sanity
# TODO: use add_link_options
if (NOT WINDOWS AND NOT APPLE)
   set(CMAKE_LINK_WHAT_YOU_USE TRUE)
endif ()

# Maybe disable that for CC (esp. windows)
# Disable ipo on windows + sanitizer build (intel openapi 2024 chokes on it)
if (WINDOWS AND CMAKE_BUILD_TYPE MATCHES "san")
  set(HAS_IPO 0)
else ()
  include(CheckIPOSupported)
  check_ipo_supported(RESULT HAS_IPO)
endif ()

# Test for unistd.h
# FIXME Is this really needed?
include(CheckIncludeFile)
CHECK_INCLUDE_FILE(unistd.h HAS_UNISTD)


# include_directories ("${PROJECT_BINARY_DIR}")


file(GLOB_RECURSE GUI_C_FILES ${CMAKE_CURRENT_SOURCE_DIR}/utils/*.c)
list(APPEND GUI_C_FILES ${CMAKE_SOURCE_DIR}/common/rhp_ipc_protocol.c)


file(GLOB_RECURSE IMGUI_FILES ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/*.cpp)

set(GUI_CPP_FILES "reshop_imgui.cpp"
                  "external/imguial/imguial_term.cpp"
)

add_executable(reshop_imgui ${GUI_C_FILES} ${GUI_CPP_FILES} ${IMGUI_FILES})

find_package(glfw3 REQUIRED)

# find_package(PkgConfig REQUIRED)
# pkg_check_modules(IMGUI REQUIRED imgui)
# pkg_check_modules(IMGUI REQUIRED imgui)


#This is for reshop_config.h and others
target_include_directories(reshop_imgui PRIVATE "${CMAKE_BINARY_DIR}/generated/"
                                                "${CMAKE_SOURCE_DIR}/common"
                                                "${CMAKE_SOURCE_DIR}/src/dag/"
                                                "${CMAKE_CURRENT_SOURCE_DIR}/external/imguial"
                                                "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui"
                                                "${CMAKE_CURRENT_SOURCE_DIR}/utils"
                                                glfw
                                                "${IMGUI_INCLUDE_DIRS}"
)

target_link_libraries(reshop_imgui ${IMGUI_LIBRARIES} glfw)
