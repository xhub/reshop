


set (RESHOP_GAMSLINK_LIB rhpcclib)
if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(RESHOP_GAMSLINK_LIB "${RESHOP_GAMSLINK_LIB}64")
endif ()

set (RESHOP_GAMSLINK_LIB ${RESHOP_GAMSLINK_LIB} PARENT_SCOPE)

list(APPEND RESHOP_GAMSLINK_LIB_C_FILES "reshop_main.c"
                                        "reshop_misc.c"
                                         "${CMAKE_SOURCE_DIR}/common/reshop_gams_optfile.c")

if (NOT GAMS_C_FILES)
  MESSAGE(FATAL_ERROR "GAMS_C_FILES must be defined before this project!")
endif ()

# Set GAMS_C_FILES properties
if (C_COMPILER_GNU_LIKE)
  set_source_files_properties(${GAMS_C_FILES} PROPERTIES COMPILE_OPTIONS
    "-Wno-error=shadow;-Wno-format-overflow;-Wno-shadow;-Wno-error=pedantic;-Wno-pedantic")
endif()

set_source_files_properties(${GAMS_C_FILES} PROPERTIES COMPILE_DEFINITIONS GC_NO_MUTEX)

set_source_files_properties(${GAMS_C_FILES} PROPERTIES INCLUDE_DIRECTORIES
                                            "${GAMSDIR}/apifiles/C/api")


list(APPEND RESHOP_GAMSLINK_LIB_C_FILES ${GAMS_C_FILES})

set_source_files_properties(${RESHOP_GAMSLINK_LIB_C_FILES} PROPERTIES INCLUDE_DIRECTORIES
                            "${CMAKE_SOURCE_DIR}/common")

# on Windows, can't link a shared library with sanitizer
if (NOT (WINDOWS AND CMAKE_BUILD_TYPE MATCHES "san"))
  add_library(${RESHOP_GAMSLINK_LIB} SHARED ${RESHOP_GAMSLINK_LIB_C_FILES})
  set(RESHOP_GAMSLINK_TARGETS ${RESHOP_GAMSLINK_LIB})

else()
   set(GAMSCONFIG_SKIP_GAMSLINK_LIB 1)
endif()

# This adds gmsrhp_ux.out
set(RESHOP_GAMSLINK_EXE "gmsrhp_ux.out")

if (_LIBRESHOP_TYPE STREQUAL "STATIC_LIBRARY")
   add_executable(${RESHOP_GAMSLINK_EXE} "gmsrhp_ux.c")
else()
   add_executable(${RESHOP_GAMSLINK_EXE} "gmsrhp_ux.c;"
                                      "${CMAKE_SOURCE_DIR}/src/gams/wrapped/xgmomcc.c;"
                                      "${CMAKE_SOURCE_DIR}/src/gams/wrapped/xgevmcc.c")
endif()

list(APPEND RESHOP_GAMSLINK_TARGETS ${RESHOP_GAMSLINK_EXE})

# The install dir depends on what we build
# This way, we don't hardcode the targets
if (BUILD_INSTALLER OR INSTALL_COMMON_MAKESELF)
  set(GAMSLINK_INSTALL_DIR ${CMAKE_BINARY_DIR}/makeself)
else()
  set(GAMSLINK_INSTALL_DIR ${OUTPUT_DIR})
endif()


foreach(_T ${RESHOP_GAMSLINK_TARGETS})
  SET_C_WARNINGS(${_T})
  target_include_directories (${_T} PRIVATE ".")
 
  target_link_libraries (${_T} PRIVATE ${RESHOP_LIBNAME} ${CMAKE_DL_LIBS})
 
   target_link_options(${_T} PRIVATE ${LINKER_FLAGS})

  if(HAS_IPO)
    set_target_properties(${_T} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

   install(TARGETS ${_T} DESTINATION ${GAMSLINK_INSTALL_DIR})
endforeach()


if (CMAKE_SYSTEM_NAME MATCHES "Linux")
   # There are differences in where linked library are search between the old and new dtags
   # Right now, GAMS relies on the old dtags behavior
   target_link_options (${RESHOP_GAMSLINK_EXE} PRIVATE LINKER:--disable-new-dtags)

   # 2024.11.29: this is most likely not needed. TODO: delete it in 2025
##elseif(APPLE)
##   target_link_options (${RESHOP_GAMSLINK_LIB} PRIVATE "-Wl,-rpath,@loader_path/")
##   target_link_options (${RESHOP_GAMSLINK_EXE} PRIVATE "-Wl,-rpath,@loader_path/")
endif()


# GAMS config

if (WINDOWS)
   set(GAMS_SCRIPTNAME "gmsgennt.cmd")
   SET(GAMS_EXENAME "gmsgennx.exe")
   set(ASAN_SCRIPTNAME ${OUTPUT_DIR}/script.cmd)
else (WINDOWS)
   SET(GAMS_SCRIPTNAME "gmsgenus.run")
   SET(GAMS_EXENAME "gmsgenux.out")
   set(ASAN_SCRIPTNAME ${OUTPUT_DIR}/script.sh)
endif(WINDOWS)



if (BUILD_INSTALLER)

   if (LINUX)
      SET(IS_LINUX 1)
   elseif (APPLE)
      SET(IS_LINUX 0)
   endif ()
   configure_file(${CMAKE_SOURCE_DIR}/gams_link/makeself/installer.sh.in ${CMAKE_BINARY_DIR}/installer.sh.in @ONLY)

   file(COPY ${CMAKE_SOURCE_DIR}/gams_link/optreshop.def DESTINATION ${CMAKE_BINARY_DIR}/makeself)

   file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/makeself/gamsconfig.yaml INPUT ${CMAKE_BINARY_DIR}/gamsconfig.yaml.in)
   file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/makeself/installer.sh INPUT ${CMAKE_BINARY_DIR}/installer.sh.in
          FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

  # target to build the makeself
   add_custom_target(makeself
      makeself --sha256 ${CMAKE_BINARY_DIR}/makeself/
      ${CMAKE_BINARY_DIR}/ReSHOP-${CMAKE_PROJECT_VERSION}-GAMS-${GAMS_VERSION}.run "ReSHOP GAMS installer" ./installer.sh
      DEPENDS ${RESHOP_GAMSLINK_LIB} ${RESHOP_LIBNAME}
              ${CMAKE_BINARY_DIR}/makeself/gamsconfig.yaml ${CMAKE_BINARY_DIR}/makeself/installer.sh)
endif (BUILD_INSTALLER)

if (INSTALL_COMMON_MAKESELF)
   file(COPY ${CMAKE_SOURCE_DIR}/gams_link/optreshop.def DESTINATION ${CMAKE_BINARY_DIR}/makeself)
endif()




set(GAMSLINK_EXE_FULLPATH $<TARGET_FILE:${RESHOP_GAMSLINK_EXE}>)
set(VALGRIND_SCRIPTNAME ${OUTPUT_DIR}/valgrind.run)
set(RESHOP_DEFNAME ${CMAKE_CURRENT_SOURCE_DIR}/optreshop.def)


set(GAMSCONFIG_YAML_FILES_DIR ${CMAKE_SOURCE_DIR}/gams_link/cfg/)
file (GLOB GAMSCONFIG_YAML_FILES LIST_DIRECTORIES false ${CMAKE_CURRENT_SOURCE_DIR}/cfg/gamsconfig*.yaml.in)
file (GLOB GAMSCONFIG_TEST_YAML_FILES LIST_DIRECTORIES false ${CMAKE_SOURCE_DIR}/test/gams/cfg/*.in)

foreach(_f ${GAMSCONFIG_YAML_FILES} ${GAMSCONFIG_TEST_YAML_FILES})
   # If we don't build rhpcclib64, the target doesn't exists and the file(GENERATE ...) fails
   if (GAMSCONFIG_SKIP_GAMSLINK_LIB AND _f MATCHES "gamsconfig.yaml.in")
      continue()
   endif()

   cmake_path(GET _f FILENAME _outfile)
   configure_file(${_f} ${CMAKE_BINARY_DIR}/generated/${_outfile} @ONLY)
   set(_infile ${_outfile})
   cmake_path(REMOVE_EXTENSION _outfile LAST_ONLY)
   file(GENERATE OUTPUT ${OUTPUT_DIR}/${_outfile} INPUT ${CMAKE_BINARY_DIR}/generated/${_infile})
endforeach()

