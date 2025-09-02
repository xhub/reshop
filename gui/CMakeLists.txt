# WE use
# 3.21 file(COPY_FILE ..)
# 3.20: add_compile_definitions 
# Bump to 3.25 for LINUX, and kill compat code below
cmake_minimum_required (VERSION 3.21)

# This needs to be set at the top
# We need that for TLS
set(CMAKE_OSX_DEPLOYMENT_TARGET 12)

# Set cmake modules directory (i.e. the one which contains all user-defined FindXXX.cmake files among other things)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)

# include (CMakeDependentOption)

project (ReSHOP_GUI VERSION          "0.4.2"
                    LANGUAGES C CXX
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

# FIXME: remove intel classical compiler asap
if(CMAKE_C_COMPILER MATCHES "icl")
   set (CMAKE_C_STANDARD 11)
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
list(APPEND GUI_C_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../common/rhp_ipc_protocol.c)


file(GLOB_RECURSE IMGUI_FILES ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/*.cpp)

set(GUI_CPP_FILES "reshop_imgui.cpp"
                  "external/imguial/imguial_term.cpp"
)

add_executable(reshop_imgui ${GUI_C_FILES} ${GUI_CPP_FILES} ${IMGUI_FILES})

find_package(glfw3)

set (IMGUI_LIBRARIES ${EXTRA_LIBS})

if (glfw3_FOUND)

   target_include_directories(reshop_imgui PRIVATE glfw)
   target_link_libraries(reshop_imgui ${IMGUI_LIBRARIES} glfw)

else (glfw3_FOUND)

   include(ExternalProject)

   set(glfw3_DESTDIR ${CMAKE_CURRENT_BINARY_DIR}/glfw)

   if (LINUX)
      set(GLFW_EXTRA_CMAKE_ARGS -DGLFW_BUILD_X11=1 -DGLFW_BUILD_WAYLAND=0
            -DCMAKE_C_FLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}/external)
      target_compile_definitions(reshop_imgui PRIVATE GLFW_INCLUDE_NONE)
      list(APPEND IMGUI_LIBRARIES rt m dl pthread)
   endif()

   ExternalProject_Add(glfw_external
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw 
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${glfw3_DESTDIR}
               -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
               -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
               -DCMAKE_GENERATOR=${CMAKE_GENERATOR}
               -DGLFW_BUILD_EXAMPLES=0
               -DGLFW_BUILD_TESTS=0
               -DGLFW_BUILD_DOCS=0
               -DGLFW_LIBRARY_TYPE=STATIC
               -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
               -DCMAKE_INSTALL_LIBDIR=${glfw3_DESTDIR}
					-DCMAKE_INSTALL_BINDIR=${glfw3_DESTDIR}
					-DCMAKE_INSTALL_INCLUDEDIR=${glfw3_DESTDIR}
					-DCMAKE_INSTALL_PREFIX=${glfw3_DESTDIR}
               ${GLFW_EXTRA_CMAKE_ARGS}
    BUILD_ALWAYS OFF  # Avoid rebuilding every time
    INSTALL_DIR ${glfw3_DESTDIR}
   )

   set(glfw3_DIR ${glfw3_DESTDIR})

      # Add include directory for the external project
   target_include_directories(reshop_imgui PRIVATE "${glfw3_DESTDIR}")
 
   if (APPLE)
      list(APPEND IMGUI_LIBRARIES "-framework OpenGL" "-framework Cocoa"
                                  "-framework IOKit" "-framework CoreVideo")
   endif()
   if (WINDOWS)
      list(APPEND IMGUI_LIBRARIES gdi32.lib opengl32.lib)
   endif()

   target_link_libraries(reshop_imgui ${IMGUI_LIBRARIES} glfw3)
   target_link_directories(reshop_imgui PRIVATE ${glfw3_DESTDIR})

   add_dependencies(reshop_imgui glfw_external)

endif(glfw3_FOUND)

#This is for reshop_config.h and others
target_include_directories(reshop_imgui PRIVATE "${CMAKE_BINARY_DIR}/generated/"
                                                "${CMAKE_CURRENT_SOURCE_DIR}/../common"
                                                "${CMAKE_CURRENT_SOURCE_DIR}/../src/dag/"
                                                "${CMAKE_CURRENT_SOURCE_DIR}/external/imguial"
                                                "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui"
                                                "${CMAKE_CURRENT_SOURCE_DIR}/utils"
                                                "${IMGUI_INCLUDE_DIRS}"
)

install(TARGETS ${_T} DESTINATION ${OUTPUT_DIR})
