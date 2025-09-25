# FindLLVMUnwind.cmake
#
# A custom CMake module to find the LLVM libunwind library.
#
# This module will define the following variables:
#   LLVMUnwind_FOUND           - True if the library and headers are found.
#   LLVMUnwind_LIBRARIES       - The path to the libunwind.a library.
#   LLVMUnwind_INCLUDE_DIRS    - The path to the libunwind headers.
#   LLVMUnwind_VERSION_STRING  - The version of the found library.
#
# This module will also define the following imported targets:
#   LLVM::Unwind               - The imported library target.

# Find the libunwind library. The LLVM build places it in a lib/ or lib64/ directory.
find_library(LLVMUnwind_LIBRARIES
  NAMES unwind
  HINTS
    ENV LLVM_ROOT
    ENV LLVM_BUILD_DIR
  PATHS
    /usr/lib
    /usr/lib64
    /usr/local/lib
    /usr/local/lib64
  PATH_SUFFIXES
    llvm/lib
    llvm/lib64
    lib
    lib64
)

# Find the libunwind headers. They are typically in an include/ directory.
find_path(LLVMUnwind_INCLUDE_DIRS
  NAMES unwind.h
  HINTS
    ENV LLVM_ROOT
    ENV LLVM_BUILD_DIR
  PATHS
    /usr/include
    /usr/local/include
  PATH_SUFFIXES
    llvm/include
    include
)

# Check if both the library and include paths were found.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMUnwind
  DEFAULT_MSG
  LLVMUnwind_LIBRARIES
  LLVMUnwind_INCLUDE_DIRS
)

# If found, set up the imported target.
if (LLVMUnwind_FOUND)
  add_library(LLVM::Unwind STATIC IMPORTED)
  set_target_properties(LLVM::Unwind PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LLVMUnwind_INCLUDE_DIRS}"
    IMPORTED_LOCATION "${LLVMUnwind_LIBRARIES}"
  )
endif()

mark_as_advanced(
  LLVMUnwind_LIBRARIES
  LLVMUnwind_INCLUDE_DIRS
)

