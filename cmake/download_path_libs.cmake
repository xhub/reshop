function(download_path_libs)
   set(PATH_BASEURL "https://pages.cs.wisc.edu/~ferris/path/julia")

   if (WINDOWS AND (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64"))
      set (PATH_LIBS "path50.dll;lusol.dll")
      set (PATH_FNAME "path50.dll")
   elseif(APPLE)
      set (PATH_LIBS "libpath50.dylib;liblusol.dylib")
      set (PATH_FNAME "libpath50.dylib")
      if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
         set (URL_SUBDIR "/path_libraries")
      endif()

   elseif (LINUX AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
       set (PATH_LIBS "libpath50.so;liblusol.so")
       set (PATH_FNAME "libpath50.so")

   else()
      message(STATUS "No PATH binaries are available for OS ${CMAKE_SYSTEM_NAME} and arch ${CMAKE_SYSTEM_PROCESSOR}")
      message(STATUS "Either make a GAMS installation available (either by putting it in PATH or via the GAMSDIR option)")
      message(STATUS "Tests requiring the PATH solver won't be build/run.")
      return()
   endif()

   file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

   foreach(_F ${PATH_LIBS})
      MESSAGE(STATUS "Downloading file ${_F} to ${CMAKE_CURRENT_BINARY_DIR}/${_F}")
      file(DOWNLOAD ${PATH_BASEURL}${URL_SUBDIR}/${_F} ${CMAKE_CURRENT_BINARY_DIR}/${_F}
           TIMEOUT 10
           STATUS DL_STATUS)

      if(NOT DL_STATUS MATCHES "0;")
         MESSAGE(STATUS "ERROR: Couldn't download file ${_F} from URL ${PATH_BASEURL}${URL_SUBDIR}/${_F} got status ${DL_STATUS}")
         return ()
      endif()

   endforeach()

   #This is necessary for the testing
   set(RHP_PATH_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/${PATH_FNAME} CACHE FILEPATH "PATH solver file" FORCE)
endfunction()
