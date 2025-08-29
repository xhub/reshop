# Get some basic mingw information

if(MINGW)
    # Create a small source file for the compiler to preprocess
    file(WRITE "${CMAKE_BINARY_DIR}/mingw_info.c" "#include <_mingw.h>\nint main() { return 0; }")

    # Run the compiler's preprocessor to get the macro definitions
    execute_process(
        COMMAND
            ${CMAKE_C_COMPILER}
            -dM -E
            "${CMAKE_BINARY_DIR}/mingw_info.c"
        OUTPUT_VARIABLE COMPILER_DEFINES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Search the output for the MSVCRT version macro
    string(REGEX MATCH "__MINGW64_VERSION_MAJOR ([0-9]+)" MINGW64_VERSION_MATCH "${COMPILER_DEFINES}")
    if(MINGW64_VERSION_MATCH)
      set(MINGW64_VERSION_MAJOR "${CMAKE_MATCH_1}")
      message(STATUS "[MINGW] MINGW runtime version: ${MINGW64_VERSION_MAJOR}")
    else()
        message(STATUS "Could not find __MINGW64_VERSION_MAJOR macro.")
    endif()

    # Search the output for the MSVCRT version macro
    string(REGEX MATCH "__MSVCRT_VERSION__ 0x([0-9]+)" MSVCRT_MATCH "${COMPILER_DEFINES}")
    if(MSVCRT_MATCH)
        set(MINGW_CRT_VERSION_HEX "0x${CMAKE_MATCH_1}")
      message(STATUS "[MINGW] MSVCRT version (hex): ${MINGW_CRT_VERSION_HEX}")

        # The hex value can be parsed to get the major version.
        # e.g., 0x0700 is version 7, 0x1400 is version 14.
        math(EXPR MINGW_CRT_VERSION_MAJOR "(${MINGW_CRT_VERSION_HEX} >> 8) & 0xff")
        message(STATUS "[MINGW] MSVCRT major version: ${MINGW_CRT_VERSION_MAJOR}")
    else()
        message(STATUS "Could not find __MSVCRT_VERSION__ macro.")
    endif()

   foreach (_m __CRTDLL__ _UCRT)
      string(REGEX MATCH "${_m}" ${_m}_MATCH "${COMPILER_DEFINES}")
      if (${_m}_MATCH)
         message(STATUS "[MINGW] macro ${_m} is defined")
      endif()
   endforeach()

    # You could add similar logic to search for a macro indicating UCRT
    # string(REGEX MATCH "__UCRT_VERSION__ ([0-9]+)" UCRT_MATCH "${COMPILER_DEFINES}")
    # ...
else()
    message(STATUS "Not a MinGW environment.")
endif()
