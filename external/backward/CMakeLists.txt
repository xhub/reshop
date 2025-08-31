# Ww use add_compile_definitions
cmake_minimum_required (VERSION 3.12)
project (backward CXX)

# The version number.
set (backward_VERSION_MAJOR 0)
set (backward_VERSION_MINOR 0)

# Use C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Do not link against libstdc++
# For clang, other flags might have to be used
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -static-libstdc++")
endif()

# put visibility to hidden
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

find_package (DWARF REQUIRED)

# add the library
add_library (backward SHARED backward.cpp)

target_include_directories (backward PRIVATE ${DWARF_INCLUDE_DIR} ${LIBDW_INCLUDE_DIR} ${ELF_INCLUDE_DIR})
target_link_libraries (backward PRIVATE "${DWARF_LIBRARIES}")

if (BUILD_INSTALLER)
   INSTALL(TARGETS backward DESTINATION ${CMAKE_BINARY_DIR}/makeself)
endif()

#Set output directory
set (OUTPUT_DIR ${CMAKE_BINARY_DIR}/output)
file(MAKE_DIRECTORY ${OUTPUT_DIR})

INSTALL(TARGETS backward DESTINATION ${OUTPUT_DIR})

cpy4test("backward")
