include(CheckCCompilerFlag)

if ("${CMAKE_C_COMPILER_FRONTEND_VARIANT}" MATCHES "GNU")
  set(C_COMPILER_GNU_LIKE 1)  
endif()


# --- Append some options (list OPT) to c compiler flags. ---
# Update :
# - CMAKE_C_FLAGS
# -
# parameters :
# OPT : the flags
# extra parameters : a list of compilers ids
# (see http://www.cmake.org/cmake/help/v3.3/variable/CMAKE_LANG_COMPILER_ID.html#variable:CMAKE_<LANG>_COMPILER_ID)
# If extra parameter is set, the option will be applied
# only for those compilers, if the option is accepted.
# 
macro(ADD_C_OPTIONS OPT _TARGETS)
  
  STRING(REGEX REPLACE " " "" OPT_SANE "${OPT}")
  STRING(REGEX REPLACE "=" "_" OPT_SANE "${OPT}")
  CHECK_C_COMPILER_FLAG("${OPT} ${_EXTRA_WARNING_FLAGS}" C_HAVE_${OPT_SANE})
  set(_compilers ${ARGN})
  IF(_compilers)
    SET(ADD_OPTION FALSE)
    FOREACH(_compiler ${_compilers})
      IF (${CMAKE_C_COMPILER_ID} MATCHES ${_compiler})
        MESSAGE(STATUS "Adding option for compiler ${_compiler}")

        SET(ADD_OPTION TRUE)
      ENDIF()
    ENDFOREACH()
  ELSE(_compilers)
    SET(ADD_OPTION TRUE)
  ENDIF(_compilers)

  IF(ADD_OPTION AND C_HAVE_${OPT_SANE})
    FOREACH(_T ${_TARGETS})
      target_compile_options(${_T} PRIVATE ${OPT})
    ENDFOREACH()
  ENDIF(ADD_OPTION AND C_HAVE_${OPT_SANE})
endmacro()

macro(SET_C_WARNINGS _TARGETS)
  if (NOT C_COMPILER_GNU_LIKE)
    add_compile_options (/Wall /wd4061 /wd4710 /wd4100 /wd4820 /wd5045)
  else ()
    # to enable: cast-function-type
    add_compile_options (-Wall -Wuninitialized -Wextra)
    set (_just_warnings  "no-cast-function-type;format-overflow=2;format=2")
    list(APPEND _just_warnings "no-c11-extensions;documentation;pointer-to-int-cast;uninitialized-const-reference")
    list(APPEND _just_warnings "pragma-pack;pragma-pack-suspicious-include;tautological-compare;null-pointer-arithmetic")
    list(APPEND _just_warnings "maybe-uninitialized;stringop-truncation;stringop-overflow=4;unused-result")
    #    list(APPEND _just_warnings "suggest-attribute=pure;suggest-attribute=const")
    #    list(APPEND _just_warnings "suggest-attribute=cold")
    list(APPEND _just_warnings "suggest-attribute=malloc;suggest-attribute=format")
    list(APPEND _just_warnings "cast-align=strict;missing-include-dirs;pointer-arith;undef;parentheses")
    #list(APPEND _just_warnings "jump-misses-init;") # too verbose
    #list(APPEND _just_warnings "conversion;no-sign-conversion") # too verbose
    # enum-int-mismatch needs >=GCC-13
    list(APPEND _just_warnings "disabled-optimization;enum-int-mismatch;literal-range;gnu;implicit-int;array-parameter")
    # TODO: reserved-identifier
      #list(APPEND _just_warnings "reserved-identifier")
      # list(APPEND _just_warnings "unsafe-buffer-usage"): This is just too verbose and doesn't have any understand. For c++ code
    list(APPEND _just_warnings "bool-operation;no-fixed-enum-extension")



    set (_error_warnings "trampolines;incompatible-pointer-types;restrict;shadow;alloc-zero;strict-prototypes")
    list(APPEND _error_warnings "return-type;switch;enum-conversion;implicit-function-declaration;init-self")
    list(APPEND _error_warnings "missing-prototypes;missing-declarations;type-limits;vla-larger-than=0")
    list(APPEND _error_warnings "int-to-pointer-cast;pointer-to-int-cast;format-security;unreachable-code")
    list(APPEND _error_warnings "inconsistent-dllimport;redundant-decls;undef;bad-function-cast;pedantic")
    list(APPEND _error_warnings "duplicated-branches;duplicated-cond;attribute-alias;nonnull;stringop-overread")

    # Increase the level of some warnings
    # format-truncation=2 does not work as we check whether truncation happens ...
    list(APPEND _error_warnings "implicit-fallthrough=5;attribute-alias=2;array-bounds=2;format-truncation=1")
    list(APPEND _error_warnings "shift-overflow=2;use-after-free=3;unused-const-variable=2")

    foreach (_ew ${_error_warnings})
      add_c_options("-Werror=${_ew}" ${_TARGETS})
    endforeach()

    foreach (_jw ${_just_warnings})
      add_c_options("-W${_jw}" ${_TARGETS})
    endforeach()

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" OR DISABLE_COMPILER_OPT)
      add_c_options("-fno-semantic-interpolation" ${_TARGETS})
    endif()

    if(NOT DISTRIB)
      add_c_options("-ftrivial-auto-var-init=pattern" ${_TARGETS})
    endif()

      # See https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html#-fstrict-flex-arrays
   add_c_options("-fstrict-flex-arrays=3" ${_TARGETS})
  endif()
endmacro()

#if (APPLE)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
#  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
#endif()
