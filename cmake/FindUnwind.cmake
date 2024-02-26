# Find the libunwind library
#
#  LIBUNWIND_FOUND       - True if libunwind was found.
#  LIBUNWIND_LIBRARIES   - The libraries needed to use libunwind
#  LIBUNWIND_INCLUDE_DIR - Location of unwind.h and libunwind.h

#  INPUT: LIBUNWIND_ROOT - path where include + lib of libunwind installation is located

FIND_PATH(LIBUNWIND_INCLUDE_DIR libunwind.h)

FIND_LIBRARY(LIBUNWIND_GENERIC_LIBRARY unwind)

if (LIBUNWIND_INCLUDE_DIR) 
   # nothing
   if (LIBUNWIND_GENERIC_LIBRARY) 
      SET(LIBUNWIND_LIBRARIES ${LIBUNWIND_GENERIC_LIBRARY})

      # For some reason, we have to link to two libunwind shared object files:
      # one arch-specific and one not.
      if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
         SET(LIBUNWIND_ARCH "arm")
      elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
         SET(LIBUNWIND_ARCH "x86_64")
      elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
         SET(LIBUNWIND_ARCH "x86")
      endif()

      if (LIBUNWIND_ARCH)
         FIND_LIBRARY(LIBUNWIND_SPECIFIC_LIBRARY "unwind-${LIBUNWIND_ARCH}")
         if (LIBUNWIND_SPECIFIC_LIBRARY)
            SET(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIBRARIES} ${LIBUNWIND_SPECIFIC_LIBRARY})
         endif ()
      endif(LIBUNWIND_ARCH)

      include(FindPackageHandleStandardArgs)
      find_package_handle_standard_args(Unwind DEFAULT_MSG LIBUNWIND_INCLUDE_DIR)

      MARK_AS_ADVANCED(LIBUNWIND_LIBRARIES LIBUNWIND_INCLUDE_DIR)

      MESSAGE(STATUS "Found libunwind.h in: ${LIBUNWIND_INCLUDE_DIR}")
      MESSAGE(STATUS "Found libunwind: ${LIBUNWIND_LIBRARIES}")

   else (LIBUNWIND_GENERIC_LIBRARY)
      MESSAGE("-- Could NOT find libunwind ")
   endif (LIBUNWIND_GENERIC_LIBRARY)
else (LIBUNWIND_INCLUDE_DIR)
   MESSAGE("-- Could NOT find libunwind.h")
endif (LIBUNWIND_INCLUDE_DIR)

