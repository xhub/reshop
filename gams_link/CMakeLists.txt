


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
endif()

# This adds gmsrhp_ux.out
set(RESHOP_GAMSLINK_EXE "gmsrhp_ux.out")

get_target_property(_LIBRESHOP_TYPE ${RESHOP_LIBNAME} TYPE)

if (_LIBRESHOP_TYPE STREQUAL "STATIC_LIBRARY")
   add_executable(${RESHOP_GAMSLINK_EXE} "gmsrhp_ux.c")
else()
   add_executable(${RESHOP_GAMSLINK_EXE} "gmsrhp_ux.c;"
                                      "${CMAKE_SOURCE_DIR}/src/gams/wrapped/xgmomcc.c;"
                                      "${CMAKE_SOURCE_DIR}/src/gams/wrapped/xgevmcc.c")
endif()

list(APPEND RESHOP_GAMSLINK_TARGETS ${RESHOP_GAMSLINK_EXE})

foreach(_T ${RESHOP_GAMSLINK_TARGETS})
  SET_C_WARNINGS(${_T})
  target_include_directories (${_T} PRIVATE ".")
 
  target_link_libraries (${_T} PRIVATE ${RESHOP_LIBNAME} ${CMAKE_DL_LIBS})
 
   target_link_options(${_T} PRIVATE ${LINKER_FLAGS})

  if(HAS_IPO)
    set_target_properties(${_T} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
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

SET(GAMSCONFIG_YAML_FILES )

configure_file(${CMAKE_SOURCE_DIR}/gams_link/cfg/gamsconfig.yaml.in ${CMAKE_BINARY_DIR}/gamsconfig.yaml.in @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/gams_link/cfg/gamsconfig_valgrind.yaml.in ${CMAKE_BINARY_DIR}/gamsconfig_valgrind.yaml @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/gams_link/cfg/gamsconfig_valgrind.yaml.in ${CMAKE_BINARY_DIR}/gamsconfig_valgrind.yaml @ONLY)

if (BUILD_INSTALLER)
   SET(GAMS_SCRIPTNAME "gmsgenus.run")
   SET(GAMS_EXENAME "gmsgenux.out")

   if (CMAKE_SYSTEM_NAME MATCHES "Linux")
      SET(IS_LINUX 1)
   elseif (APPLE)
      SET(IS_LINUX 0)
   endif ()
   configure_file(${CMAKE_SOURCE_DIR}/gams_link/makeself/installer.sh.in ${CMAKE_BINARY_DIR}/installer.sh.in @ONLY)

   file(COPY ${CMAKE_SOURCE_DIR}/gams_link/optreshop.def DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   INSTALL(TARGETS ${RESHOP_GAMSLINK_LIB} DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   INSTALL(TARGETS ${RESHOP_GAMSLINK_EXE} DESTINATION ${CMAKE_BINARY_DIR}/makeself)

   file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/makeself/gamsconfig.yaml INPUT ${CMAKE_BINARY_DIR}/gamsconfig.yaml.in)
   file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/makeself/installer.sh INPUT ${CMAKE_BINARY_DIR}/installer.sh.in
          FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

  # target to build the makeself
   add_custom_target(makeself
      makeself --sha256 ${CMAKE_BINARY_DIR}/makeself/
      ${CMAKE_BINARY_DIR}/ReSHOP-${CMAKE_PROJECT_VERSION}-GAMS-${GAMS_VERSION}.run "ReSHOP GAMS installer" ./installer.sh
      DEPENDS ${RESHOP_GAMSLINK_LIB} ${RESHOP_LIBNAME}
              ${CMAKE_BINARY_DIR}/makeself/gamsconfig.yaml ${CMAKE_BINARY_DIR}/makeself/installer.sh)

      #else (BUILD_INSTALLER)
      #   INSTALL(TARGETS ${RESHOP_GAMSLINK_LIB} DESTINATION ${GAMSDIR})
      #   INSTALL(TARGETS ${RESHOP_GAMSLINK_EXE} DESTINATION ${GAMSDIR})
endif(BUILD_INSTALLER)

if (INSTALL_COMMON_MAKESELF)
   INSTALL(TARGETS ${RESHOP_GAMSLINK_LIB} DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   INSTALL(TARGETS ${RESHOP_GAMSLINK_EXE} DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   file(COPY ${CMAKE_SOURCE_DIR}/gams_link/optreshop.def DESTINATION ${CMAKE_BINARY_DIR}/makeself)
endif()
