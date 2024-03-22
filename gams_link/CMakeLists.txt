


set (RESHOP_GAMS_INTERFACE rhpcclib)
if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  set(RESHOP_GAMS_INTERFACE "${RESHOP_GAMS_INTERFACE}64")
endif ()

set (RESHOP_GAMS_INTERFACE ${RESHOP_GAMS_INTERFACE} PARENT_SCOPE)

set(RESHOP_GAMS_INTERFACE_C_FILES "apiwrap/reshop/rhpcclib.c")
list(APPEND RESHOP_GAMS_INTERFACE_C_FILES "reshop_main.c"
                                          "reshop_misc.c"
                                          "reshop_opt.c")

if (NOT GAMS_C_FILES)
  MESSAGE(FATAL_ERROR "GAMS_C_FILES must be defined before this project!")
endif ()

list(APPEND RESHOP_GAMS_INTERFACE_C_FILES ${GAMS_C_FILES})

add_library(${RESHOP_GAMS_INTERFACE} SHARED ${RESHOP_GAMS_INTERFACE_C_FILES})

target_link_libraries (${RESHOP_GAMS_INTERFACE} PRIVATE ${RESHOP_LIBNAME} ${CMAKE_DL_LIBS})
target_include_directories (${RESHOP_GAMS_INTERFACE} PRIVATE ".")

SET_C_WARNINGS(${RESHOP_GAMS_INTERFACE})

if (C_COMPILER_GNU_LIKE)
  set_source_files_properties(${GAMS_C_FILES} PROPERTIES COMPILE_OPTIONS
    "-Wno-error=shadow;-Wno-format-overflow;-Wno-shadow;-Wno-error=pedantic;-Wno-pedantic")
endif()

set_source_files_properties(${GAMS_C_FILES} PROPERTIES COMPILE_DEFINITIONS GC_NO_MUTEX)

set_source_files_properties(${GAMS_C_FILES} PROPERTIES INCLUDE_DIRECTORIES
   "${GAMSDIR}/apifiles/C/api")

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_options (${RESHOP_GAMS_INTERFACE} PRIVATE
    LINKER:--warn-common LINKER:--allow-shlib-undefined -nostartfiles LINKER:-Bsymbolic
    LINKER:--no-undefined LINKER:-z,defs LINKER:-z,now LINKER:-z,relro)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
  target_link_options (${RESHOP_GAMS_INTERFACE} PRIVATE
    LINKER:-def:${CMAKE_SOURCE_DIR}/gams_link/apiwrap/reshop/${RESHOP_GAMS_INTERFACE}.def)
elseif(APPLE)
  target_link_options (${RESHOP_GAMS_INTERFACE} PRIVATE "-Wl,-rpath,@loader_path/")
endif()

if(HAS_IPO)
  set_target_properties(${RESHOP_GAMS_INTERFACE} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if (BUILD_INSTALLER)
   SET(GAMS_SCRIPTNAME "gmsgenus.run")
   SET(GAMS_EXENAME "gmsgenux.out")

   configure_file(${CMAKE_SOURCE_DIR}/gams_link/cfg/gamsconfig.yaml.in ${CMAKE_BINARY_DIR}/gamsconfig.yaml.in @ONLY)
   if (CMAKE_SYSTEM_NAME MATCHES "Linux")
      SET(IS_LINUX 1)
   elseif (APPLE)
      SET(IS_LINUX 0)
   endif ()
   configure_file(${CMAKE_SOURCE_DIR}/gams_link/makeself/installer.sh.in ${CMAKE_BINARY_DIR}/installer.sh.in @ONLY)

   file(COPY ${CMAKE_SOURCE_DIR}/gams_link/optreshop.def DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   INSTALL(TARGETS ${RESHOP_GAMS_INTERFACE} DESTINATION ${CMAKE_BINARY_DIR}/makeself)

   file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/makeself/gamsconfig.yaml INPUT ${CMAKE_BINARY_DIR}/gamsconfig.yaml.in)
   file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/makeself/installer.sh INPUT ${CMAKE_BINARY_DIR}/installer.sh.in
          FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

  # target to build the makeself
   add_custom_target(makeself
      makeself --sha256 ${CMAKE_BINARY_DIR}/makeself/
      ${CMAKE_BINARY_DIR}/ReSHOP-${CMAKE_PROJECT_VERSION}-GAMS-${GAMS_VERSION}.run "ReSHOP GAMS installer" ./installer.sh
      DEPENDS ${RESHOP_GAMS_INTERFACE} ${RESHOP_LIBNAME}
              ${CMAKE_BINARY_DIR}/makeself/gamsconfig.yaml ${CMAKE_BINARY_DIR}/makeself/installer.sh)

else (BUILD_INSTALLER)
   INSTALL(TARGETS ${RESHOP_GAMS_INTERFACE} DESTINATION ${GAMSDIR})
endif(BUILD_INSTALLER)

if (INSTALL_COMMON_MAKESELF)
   INSTALL(TARGETS ${RESHOP_GAMS_INTERFACE} DESTINATION ${CMAKE_BINARY_DIR}/makeself)
   file(COPY ${CMAKE_SOURCE_DIR}/gams_link/optreshop.def DESTINATION ${CMAKE_BINARY_DIR}/makeself)
endif()
