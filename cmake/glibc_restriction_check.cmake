if(NOT GLIBC_MAX_VER)
   message(FATAL_ERROR "GLIBC_MAX_VER must be defined")
endif()

file(READ ${CMAKE_BINARY_DIR}/glibc-num-non-conforming-symbols.txt NUM_GLIBC_SYMBOLS)
file(READ ${CMAKE_BINARY_DIR}/glibc-non-conforming-symbols.txt GLIBC_SYMBOLS)
if(NUM_GLIBC_SYMBOLS GREATER 0)
   message(FATAL_ERROR "Failed to produce a library compatible with GLIBC 2.${GLIBC_MAX_VER}: ${GLIBC_SYMBOLS}")
endif()
