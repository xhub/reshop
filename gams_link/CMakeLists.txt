


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

add_library(${RESHOP_GAMSLINK_LIB} SHARED ${RESHOP_GAMSLINK_LIB_C_FILES})


# This adds gmsrhp_ux.out
set(RESHOP_GAMSLINK_EXE "gmsrhp_ux.out")

# For a sanitizer build, we put everything in gmsrhp_ux
if (CMAKE_BUILD_TYPE MATCHES "san")
   add_executable(${RESHOP_GAMSLINK_EXE} "gmsrhp_ux.c;" ${RESHOP_C_FILES})
   target_link_libraries(${RESHOP_GAMSLINK_EXE} PRIVATE ${EXTRA_LIBS})
else()
   add_executable(${RESHOP_GAMSLINK_EXE} "gmsrhp_ux.c;"
                                      "${CMAKE_SOURCE_DIR}/src/gams/wrapped/xgmomcc.c;"
                                      "${CMAKE_SOURCE_DIR}/src/gams/wrapped/xgevmcc.c")
endif()

list(APPEND RESHOP_GAMSLINK_TARGETS ${RESHOP_GAMSLINK_EXE} ${RESHOP_GAMSLINK_LIB})

foreach(_T ${RESHOP_GAMSLINK_TARGETS})
  target_link_libraries (${_T} PRIVATE ${RESHOP_LIBNAME} ${CMAKE_DL_LIBS})
 
  target_include_directories (${_T} PRIVATE ".")
 
  SET_C_WARNINGS(${_T})
 
  if(HAS_IPO)
    set_target_properties(${_T} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endforeach()


if (CMAKE_SYSTEM_NAME MATCHES "Linux")
   target_link_options (${RESHOP_GAMSLINK_LIB} PRIVATE
      LINKER:--warn-common LINKER:--allow-shlib-undefined -nostartfiles LINKER:-Bsymbolic
      LINKER:--no-undefined LINKER:-z,defs LINKER:-z,now LINKER:-z,relro)
   target_link_options (${RESHOP_GAMSLINK_EXE} PRIVATE
   LINKER:--warn-common LINKER:--no-undefined LINKER:-z,defs LINKER:-z,now LINKER:-z,relro
   LINKER:--disable-new-dtags)
elseif(APPLE)
   target_link_options (${RESHOP_GAMSLINK_LIB} PRIVATE "-Wl,-rpath,@loader_path/")
   target_link_options (${RESHOP_GAMSLINK_EXE} PRIVATE "-Wl,-rpath,@loader_path/")
endif()

configure_file(${CMAKE_SOURCE_DIR}/gams_link/cfg/gamsconfig.yaml.in ${CMAKE_BINARY_DIR}/gamsconfig.yaml.in @ONLY)
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

else (BUILD_INSTALLER)
   INSTALL(TARGETS ${RESHOP_GAMSLINK_LIB} DESTINATION ${GAMSDIR})
   INSTALL(TARGETS ${RESHOP_GAMSLINK_EXE} DESTINATION ${GAMSDIR})
endif(BUILD_INSTALLER)

if (INSTALL_COMMON_MAKESELF)
   INSTALL(TARGETS ${RESHOP_GAMSLINK_LIB} DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   INSTALL(TARGETS ${RESHOP_GAMSLINK_EXE} DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   file(COPY ${CMAKE_SOURCE_DIR}/gams_link/optreshop.def DESTINATION ${CMAKE_BINARY_DIR}/makeself)
endif()
