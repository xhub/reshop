# From https://stackoverflow.com/questions/44320465/whats-the-proper-way-to-enable-addresssanitizer-in-cmake-that-works-in-xcode

if (NOT "${CMAKE_C_COMPILER_FRONTEND_VARIANT}" MATCHES "GNU" AND NOT CMAKE_C_COMPILER_ID MATCHES "IntelLLVM")
  return()
endif()

get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

SET(SANITIZER_PROFILES "Asan;Msan;UBsan;HwAsan" CACHE STRING
  "All sanitizer profiles" FORCE)

if(isMultiConfig)
  foreach(profile in SANITIZER_PROFILES)
    if(NOT ${profile} IN_LIST CMAKE_CONFIGURATION_TYPES)
      list(APPEND CMAKE_CONFIGURATION_TYPES ${profile})
    endif()
    endforeach()
else()
  set(allowedBuildTypes Asan Msan UBsan HwAsan Debug Release RelWithDebInfo MinSizeRel)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${allowedBuildTypes}")

  if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE IN_LIST allowedBuildTypes)
    message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
  endif()
endif()

if("x${CMAKE_C_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC")
  set(LIBSAN_SHARED_FLAGS "/Qoption,link,/force -v")

ELSEIF(CMAKE_C_COMPILER_ID MATCHES "Clang" AND LINUX)
   execute_process (COMMAND ${CMAKE_C_COMPILER} --print-file-name libclang_rt.asan-x86_64.so
      OUTPUT_VARIABLE LIBCLANG_RT OUTPUT_STRIP_TRAILING_WHITESPACE)
   get_filename_component(LIBSAN_RPATH ${LIBCLANG_RT} DIRECTORY)
   SET(LIBSAN_SHARED_FLAGS "-shared-libsan -Wl,-rpath=${LIBSAN_RPATH}")

ENDIF()

if("x${CMAKE_C_COMPILER_FRONTEND_VARIANT}" STREQUAL "xMSVC")
  set(ASAN_COMPILE_FLAGS "/fsanitize=address")
  set(ASAN_LINKER_FLAGS "/fsanitize=address")
else()
  set(ASAN_COMPILE_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
  set(ASAN_LINKER_FLAGS "-fsanitize=address")
endif()


set(CMAKE_C_FLAGS_ASAN
  "${CMAKE_C_FLAGS_DEBUG} ${ASAN_COMPILE_FLAGS}" CACHE STRING
  "Flags used by the C compiler for Asan build type or configuration." FORCE)

set(CMAKE_CXX_FLAGS_ASAN
  "${CMAKE_CXX_FLAGS_DEBUG} ${ASAN_COMPILE_FLAGS}" CACHE STRING
  "Flags used by the C++ compiler for Asan build type or configuration." FORCE)

set(CMAKE_EXE_LINKER_FLAGS_ASAN
  "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} ${ASAN_LINKER_FLAGS} ${LIBSAN_SHARED_FLAGS}" CACHE STRING
  "Linker flags to be used to create executables for Asan build type." FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_ASAN
  "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} ${ASAN_LINKER_FLAGS} ${LIBSAN_SHARED_FLAGS}" CACHE STRING
  "Linker lags to be used to create shared libraries for Asan build type." FORCE)

# -fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer

set(CMAKE_C_FLAGS_MSAN
  "${CMAKE_C_FLAGS_DEBUG} -fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer" CACHE STRING
  "Flags used by the C compiler for Msan build type or configuration." FORCE)

set(CMAKE_CXX_FLAGS_MSAN
  "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer" CACHE STRING
  "Flags used by the C++ compiler for Msan build type or configuration." FORCE)

set(CMAKE_EXE_LINKER_FLAGS_MSAN
  "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=memory -fsanitize-memory-track-origins=2 ${LIBSAN_SHARED_FLAGS}" CACHE STRING
  "Linker flags to be used to create executables for Msan build type." FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_MSAN
  "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=memory -fsanitize-memory-track-origins=2 ${LIBSAN_SHARED_FLAGS}" CACHE STRING
  "Linker lags to be used to create shared libraries for Msan build type." FORCE)

# -fsanitize=undefined

IF(CMAKE_C_COMPILER_ID MATCHES "Clang")
   SET(CLANG_UBSAN "-fsanitize=nullability -fsanitize=local-bounds")
ENDIF()

set(CMAKE_C_FLAGS_UBSAN
  "${CMAKE_C_FLAGS_DEBUG} -fsanitize=undefined ${CLANG_UBSAN} -fno-omit-frame-pointer" CACHE STRING
  "Flags used by the C compiler for UBsan build type or configuration." FORCE)

set(CMAKE_CXX_FLAGS_UBSAN
  "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined ${CLANG_UBSAN} -fno-omit-frame-pointer" CACHE STRING
  "Flags used by the C++ compiler for UBsan build type or configuration." FORCE)

set(CMAKE_EXE_LINKER_FLAGS_UBSAN
  "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=undefined ${CLANG_UBSAN} ${LIBSAN_SHARED_FLAGS}" CACHE STRING
  "Linker flags to be used to create executables for UBsan build type." FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_UBSAN
  "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=undefined ${CLANG_UBSAN} ${LIBSAN_SHARED_FLAGS}" CACHE STRING
  "Linker lags to be used to create shared libraries for UBsan build type." FORCE)

# HWASAN

set(CMAKE_C_FLAGS_HWASAN
  "${CMAKE_C_FLAGS_DEBUG} -fsanitize=hwaddress -fno-omit-frame-pointer" CACHE STRING
  "Flags used by the C compiler for Asan build type or configuration." FORCE)

set(CMAKE_CXX_FLAGS_HWASAN
  "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=hwaddress -fno-omit-frame-pointer" CACHE STRING
  "Flags used by the C++ compiler for Asan build type or configuration." FORCE)

set(CMAKE_EXE_LINKER_FLAGS_HWASAN
  "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=hwaddress ${LIBSAN_SHARED_FLAGS}" CACHE STRING
  "Linker flags to be used to create executables for Asan build type." FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_HWASAN
  "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=hwaddress ${LIBSAN_SHARED_FLAGS}" CACHE STRING
  "Linker lags to be used to create shared libraries for Asan build type." FORCE)


