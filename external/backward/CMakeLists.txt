# Ww use add_compile_definitions
cmake_minimum_required (VERSION 3.25)
project (backward CXX)

# The version number.
set (backward_VERSION_MAJOR 0)
set (backward_VERSION_MINOR 0)

# Use C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Do not link against libstdc++
# For clang, other flags might have to be used
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -static-libstdc++")
endif()

# put visibility to hidden
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# add the library
add_library (backward SHARED backward.cpp)

if (LINUX)

   find_package (DWARF REQUIRED)
   target_include_directories (backward PRIVATE ${DWARF_INCLUDE_DIR} ${LIBDW_INCLUDE_DIR} ${ELF_INCLUDE_DIR})
   target_link_libraries (backward PRIVATE "${DWARF_LIBRARIES}")
   target_compile_definitions(backward PRIVATE BACKWARD_HAS_DW=1)

elseif (APPLE)

   find_package(LLVMUnwind)
   if (LLVMUnwind_FOUND)
      target_include_directories (backward PRIVATE LLVM::Unwind)
      target_link_libraries (backward PRIVATE LLVM::Unwind)
      target_compile_definitions(backward PRIVATE BACKWARD_HAS_LIBUNWIND=1)
   endif()

endif()

if (BUILD_INSTALLER)
   INSTALL(TARGETS backward DESTINATION ${CMAKE_BINARY_DIR}/makeself)
endif()

#Set output directory
set (OUTPUT_DIR ${CMAKE_BINARY_DIR}/output)
file(MAKE_DIRECTORY ${OUTPUT_DIR})

INSTALL(TARGETS backward DESTINATION ${OUTPUT_DIR})

cpy4test("backward")
