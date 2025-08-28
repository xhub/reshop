# WE use add_compile_definitions
cmake_minimum_required (VERSION 3.25)

# This needs to be set at the top
# We need that for TLS
set(CMAKE_OSX_DEPLOYMENT_TARGET 11)

include (CMakeDependentOption)

project (ReSHOPOpt LANGUAGES C CXX)
 

if (NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif ()

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_CXX_VISIBILITY_PRESET hidden)

# Start options
option(GamsOptionsSpecWriter_DIR
  "Directory containing the GamsOptionsSpecWriter files" "")
 
#################################################
# Platform-specific part
#################################################

if (LINUX) # GLIBC-specific for now
   SET(_GNU_SOURCE 1)
endif(LINUX)

if (APPLE)
   if (OSXCROSS_HOST)
      # This seems needed for the rpath madness -- 2024.01.23
      set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "-Wl,-rpath,")

      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,@loader_path")
      set(CMAKE_EXE_MODULE_FLAGS "${CMAKE_EXE_MODULE_FLAGS} -Wl,-rpath,@loader_path")
   endif()

   SET(_DARWIN_C_SOURCE 1)
   # Tell cmake to use "@rpath" as part of the install_name dir (INSTALL_NAME_DIR)
   SET(CMAKE_MACOSX_RPATH TRUE)
   # The above does not seem to be enough
   SET(CMAKE_INSTALL_NAME_DIR "@rpath")
   # Tell CMAKE to use INSTALL_NAME_DIR already at build time
   SET(CMAKE_BUILD_WITH_INSTALL_NAME_DIR TRUE)

   # TODO: is the CMAKE_BUILD_RPATH needed?
   set(CMAKE_BUILD_RPATH "@loader_path")
   set(CMAKE_INSTALL_RPATH "@loader_path")

endif(APPLE)


# include directories
if (NOT GamsOptionsSpecWriter_DIR)
   include(FetchContent)

   FetchContent_Declare(
      coinor_gamslinks
      GIT_REPOSITORY https://github.com/coin-or/GAMSlinks.git
      GIT_TAG        514173ea35eb56894183b7dcc12c90d9c3a20288
   )
   FetchContent_MakeAvailable(coinor_gamslinks)

   set(GamsOptionsSpecWriter_DIR "${coinor_gamslinks_SOURCE_DIR}/src/utils")
endif()


include_directories ("${GamsOptionsSpecWriter_DIR}")
include_directories ("${PROJECT_BINARY_DIR}")

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH TMP_PARENT)
cmake_path(GET TMP_PARENT PARENT_PATH RESHOP_TOPLEVEL)

MESSAGE(STATUS "RESHOP_TOPLEVEL: ${RESHOP_TOPLEVEL}")

# Required by reshop source files
configure_file(${RESHOP_TOPLEVEL}/cmake/reshop_config.h.in ${PROJECT_BINARY_DIR}/reshop_config.h)



# Add files
FILE(GLOB_RECURSE OPTGEN_FILES *.cpp)

LIST(APPEND OPTGEN_FILES "${RESHOP_TOPLEVEL}/src/ovf/ovf_options.c")
LIST(APPEND OPTGEN_FILES "${RESHOP_TOPLEVEL}/src/toplayer/rhp_options.c")
LIST(APPEND OPTGEN_FILES "${RESHOP_TOPLEVEL}/src/toplayer/rhp_options_data.c")
LIST(APPEND OPTGEN_FILES "${RESHOP_TOPLEVEL}/src/utils/option.c")
LIST(APPEND OPTGEN_FILES "${RESHOP_TOPLEVEL}/src/utils/env_utils.c")
LIST(APPEND OPTGEN_FILES "${GamsOptionsSpecWriter_DIR}/GamsOptionsSpecWriter.cpp")

MESSAGE(STATUS "OPTGEN_FILES: ${OPTGEN_FILES}")

include_directories ("${RESHOP_TOPLEVEL}/src")
include_directories ("${RESHOP_TOPLEVEL}/src/ovf")
include_directories ("${RESHOP_TOPLEVEL}/src/toplayer")
include_directories ("${RESHOP_TOPLEVEL}/src/utils")

add_executable(reshopopt "${OPTGEN_FILES}")

add_custom_command(TARGET reshopopt POST_BUILD
                   COMMAND reshopopt
                   COMMENT "Generating optreshop.def"
)

if(OPTRESHOP_DEF_OUTPUTDIR)
   add_custom_command(TARGET reshopopt POST_BUILD
                      COMMAND ${CMAKE_COMMAND} -E copy optreshop.def "${OPTRESHOP_DEF_OUTPUTDIR}"
                      COMMENT "Copying optreshop.def"
   )
endif(OPTRESHOP_DEF_OUTPUTDIR)
