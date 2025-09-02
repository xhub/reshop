

if (APPLE OR UNIX)
   LIST(APPEND TEST_EXTRA_LIBS "m")
endif (APPLE OR UNIX)

function (my_add_test test_name test_exe)
   if (DARLING)
      add_test (NAME ${test_name} COMMAND ${DARLING} ${CMAKE_CURRENT_BINARY_DIR}/${test_exe} ${ARGN})
   elseif (NEED_WINE)
      add_test (NAME ${test_name} COMMAND wine ${test_exe} ${ARGN})
else()
      add_test (NAME ${test_name} COMMAND ${test_exe} ${ARGN})
   endif()
endfunction()

function (ADD_PUBLIC_TEST c_file)

   string (REGEX REPLACE "^.*/" "" _c_file ${c_file})
   string (REPLACE ".c" "" _test_exe ${_c_file})
   string (REPLACE "test_" "" _test_name ${_test_exe})

   add_executable (${_test_exe} ${c_file} ${ARGN})
   SET_C_WARNINGS(${_test_exe})

   target_link_libraries (${_test_exe} ${RESHOP_LIBNAME} ${TEST_EXTRA_LIBS})

   target_link_options(${_test_exe} PRIVATE ${LINKER_FLAGS})

   target_compile_definitions (${_test_exe} PRIVATE GAMSDIR="${GAMSDIR_TARGET}")
   target_compile_definitions (${_test_exe} PRIVATE GAMSCNTR_DAT_FILE="${GAMSCNTR_DAT_FILE}")


   if (BUILD_VREPR) # Useful for conjugate reformulation
      target_compile_definitions (${_test_exe} PRIVATE HAS_VREPR)
   endif(BUILD_VREPR)
   if (DARLING) # Disable some tests
      target_compile_definitions (${_test_exe} PRIVATE USES_DARLING)
   elseif (NEED_WINE)
      target_compile_definitions (${_test_exe} PRIVATE USES_WINE)
   endif()
   target_include_directories (${_test_exe} PRIVATE utils)

   set(CNT 1)
   # This is broken with multiple files

   #  foreach (_opt_arg ${ARGN})
   #    separate_arguments(_args NATIVE_COMMAND ${_opt_arg})
   #
   #    add_test (${_test_name}_${CNT} ${_test_exe} ${_args})
   #
   #    math (EXPR CNT "${CNT} + 1")
   #  endforeach()

   if (CNT EQUAL 1)
      my_add_test (${_test_name} ${_test_exe})
   endif()

   if (${_test_name} MATCHES "gams")
      #  This is better than run_serial as it allows for parallelization with
      #  non-GAMS tests
      set_tests_properties(${_test_name} PROPERTIES RESOURCE_LOCK gams)
   endif()
endfunction ()

function (ADD_INTERNAL_TEST c_file)

   string (REGEX REPLACE "^.*/" "" _c_file ${c_file})
   string (REPLACE ".c" "" _test_exe ${_c_file})
   string (REPLACE "test_" "" _test_name ${_test_exe})

   add_executable (${_test_exe} ${c_file} $<TARGET_OBJECTS:${RESHOP_LIBNAME_OBJ}>)
   target_link_libraries(${_test_exe} $<TARGET_PROPERTY:${RESHOP_LIBNAME},LINK_LIBRARIES>)

   SET_C_WARNINGS(${_test_exe})

   set(CNT 1)
   foreach (_opt_arg ${ARGN})
      separate_arguments(_args NATIVE_COMMAND ${_opt_arg})

      my_add_test (${_test_name}_${CNT} ${_test_exe} ${_args})

      math (EXPR CNT "${CNT} + 1")
   endforeach()

   if (CNT EQUAL 1)
      my_add_test (${_test_name} ${_test_exe})
   endif()
endfunction ()



# Poor man's test of GAMS availability
if (GAMSDIR_TARGET AND GAMSCNTR_DAT_FILE)

   # Add a cleanup fixture to get rid of these 225* directories
   ADD_PUBLIC_TEST(test_ovf_fitting_gams.c test_ovf_fitting_common.c utils/test_gams_utils.c)
   #add_test(ovf_fitting_gams_initvar test_ovf_fitting_gams)
   #set_tests_properties(ovf_fitting_gams_initvar PROPERTIES ENVIRONMENT "RHP_OVF_INIT_NEW_VARIABLES=1" )
   ADD_PUBLIC_TEST(test_basic_gams.c test_basic.c test_common.c utils/test_gams_utils.c)
   ADD_PUBLIC_TEST(test_crm_gams.c test_crm.c test_common.c ../src/utils/os/asprintf.c utils/test_gams_utils.c)
   ADD_PUBLIC_TEST(test_ovf_gams.c test_ovf.c test_common.c utils/test_gams_utils.c)
   set_tests_properties(ovf_fitting_gams PROPERTIES LABELS "darling_fails")
   set_tests_properties(basic_gams PROPERTIES LABELS "darling_fails")
endif (GAMSDIR_TARGET AND GAMSCNTR_DAT_FILE)

# usually this means no sanitizer are active
if (RESHOP_INTERNAL_TESTS)
   ADD_INTERNAL_TEST(internal/test_diff.c)
   ADD_INTERNAL_TEST(internal/test_nlopcode.c)
if (NOT DARLING AND NOT NEED_WINE)
   ADD_INTERNAL_TEST(internal/test_tree.c
      "${CMAKE_SOURCE_DIR}/test/data/dat1.dat ${CMAKE_SOURCE_DIR}/test/data/dat2.dat")
else()
   ADD_INTERNAL_TEST(internal/test_tree.c)
endif()

   #set_tests_properties(tree_1 PROPERTIES LABELS "darling_fails")
endif()

# If not GAMS install has been detected, rely on the "julia" binaries to get PATH
if (NOT PATH_SOLVER_COPIED)
   download_path_libs()
endif()

# We have ensured that the PATH libraries are available

ADD_PUBLIC_TEST(test_ovf_fitting_reshop.c test_ovf_fitting_common.c)
#add_test(ovf_fitting_reshop_initvar test_ovf_fitting_reshop)
#set_tests_properties(ovf_fitting_reshop_initvar PROPERTIES ENVIRONMENT "RHP_OVF_INIT_NEW_VARIABLES=1" )
ADD_PUBLIC_TEST(test_basic_reshop.c test_basic.c test_common.c)
ADD_PUBLIC_TEST(test_crm_reshop.c test_crm.c test_common.c ../src/utils/os/asprintf.c)

ADD_PUBLIC_TEST(test_ovf_reshop.c test_ovf.c test_common.c)
