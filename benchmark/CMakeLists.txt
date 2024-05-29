if (APPLE OR UNIX)
   LIST(APPEND benchmark_EXTRA_LIBS "m")
endif (APPLE OR UNIX)

function (my_add_bench benchmark_name benchmark_exe)
   if (DARLING)
      add_benchmark (NAME ${benchmark_name} COMMAND ${DARLING} ${CMAKE_CURRENT_BINARY_DIR}/${benchmark_exe} ${ARGN})
   elseif (NEED_WINE)
      add_benchmark (NAME ${benchmark_name} COMMAND wine ${benchmark_exe} ${ARGN})
else()
      add_benchmark (NAME ${benchmark_name} COMMAND ${benchmark_exe} ${ARGN})
   endif()
endfunction()

function (add_benchmark c_file)

   string (REGEX REPLACE "^.*/" "" _c_file ${c_file})
   string (REPLACE ".c" "" _benchmark_exe ${_c_file})
   string (REPLACE "benchmark_" "" _benchmark_name ${_benchmark_exe})

   add_executable (${_benchmark_exe} ${c_file} utils/bench_utils.c ${ARGN})
   SET_C_WARNINGS(${_benchmark_exe})

   target_link_libraries (${_benchmark_exe} ${RESHOP_LIBNAME} ${benchmark_EXTRA_LIBS})
   target_compile_definitions (${_benchmark_exe} PRIVATE GAMSDIR="${GAMSDIR_TARGET}")
   target_compile_definitions (${_benchmark_exe} PRIVATE GAMSCNTR_DAT_FILE="${GAMSCNTR_DAT_FILE}")
   target_include_directories (${_benchmark_exe} PRIVATE utils)

   target_include_directories (${_benchmark_exe} PRIVATE ${CMAKE_SOURCE_DIR}/test/ ${CMAKE_SOURCE_DIR}/test/utils)


   # my_add_bench (${_benchmark_name} ${_benchmark_exe})

endfunction ()


add_benchmark(ovf/linear_quantile_regression.c ${CMAKE_SOURCE_DIR}/test/test_ovf.c ${CMAKE_SOURCE_DIR}/test/test_common.c ${CMAKE_SOURCE_DIR}/test/utils/test_gams_utils.c)
