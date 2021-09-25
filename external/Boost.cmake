# -----------------------------------------------------------------------------
# HTM Community Edition of NuPIC
# Copyright (C) 2016, Numenta, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Affero Public License for more details.
#
# You should have received a copy of the GNU Affero Public License
# along with this program.  If not, see http://www.gnu.org/licenses.
# -----------------------------------------------------------------------------

# Creates ExternalProject for building the boost system and filesystem static libraries
#  Documentation: https://boostorg.github.io/build/manual/develop/index.html
#                 https://boostorg.github.io/build/tutorial.html
#
#  info about "--layout versioned" https://stackoverflow.com/questions/32991736/boost-lib-naming-are-missing/52208574#52208574
#               filenames created by boost: https://www.boost.org/doc/libs/1_68_0/more/getting_started/unix-variants.html#library-naming
#
#  NOTE: We are looking for a static library containing just filesystem and system modules.
#        This library must be merged into a shared library so it must be compiled with -fPIC.
#        This is the reason we cannot use an externally installed version.
#
#        We may not need Boost at all.  If using C++17 standard and the compiler version supports
#        std::filesystem then we will skip this module entirely. See logic in CommonCompilerConfig.cmake.
#
#######################################

if(EXISTS "${REPOSITORY_DIR}/build/ThirdParty/share/boost.tar.gz")
    set(BOOST_URL "${REPOSITORY_DIR}/build/ThirdParty/share/boost.tar.gz")
else()
    set(BOOST_URL "https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.gz")
endif()

# Download the boost distribution (at configure time).
message(STATUS "obtaining Boost")
if(IS_DIRECTORY ${EP_BASE}/boost)
else()
  message(STATUS "   Boost download & install takes a while, go get a coffee :) ...")
endif()
include(DownloadProject/DownloadProject.cmake)
#  note: this will not download if it already exists.
download_project(PROJ Boost_download
	PREFIX ${EP_BASE}/boost
	URL ${BOOST_URL}
	UPDATE_DISCONNECTED 1
	QUIET
	)
	

# Set some parameters
set(BOOST_ROOT ${Boost_download_SOURCE_DIR})
set(Boost_INCLUDE_DIRS ${BOOST_ROOT})

file(GLOB Boost_LIBRARIES ${BOOST_ROOT}/stage/lib/libboost*)
list(LENGTH Boost_LIBRARIES qty_libs)
if(${qty_libs} LESS 2)
  #message(STATUS "Boost being installed at BOOST_ROOT = ${BOOST_ROOT}")
  if (MSVC OR MSYS OR MINGW)
    if (MSYS OR MINGW)
	  set(bootstrap "bootstrap.bat gcc")
	  set(toolset toolset=gcc architecture=x86)
    elseif(MSVC)
	  set(bootstrap "bootstrap.bat vc141")
	  set(toolset toolset=msvc-15.0 architecture=x86)
    endif()
  else()
    set(bootstrap "./bootstrap.sh")
    set(toolset) # b2 will figure out the toolset
  endif()
  # On Windows this will build 4 libraries per module.  32/64bit and release/debug variants
  # All will be Static, multithreaded, shared runtime link, compiled with -fPIC
  
  execute_process(COMMAND "${bootstrap}" 
        WORKING_DIRECTORY ${BOOST_ROOT}
	OUTPUT_QUIET
	RESULT_VARIABLE error_result
	)
  if(error_result)
    message(FATAL_ERROR "Boost bootstrap has errors.   ${error_result}")
  else()
    execute_process(
        COMMAND "./b2"
  		--prefix=${BOOST_ROOT}
  		--with-filesystem 
		--with-system 
		--layout=system
		variant=release
		threading=multi 
		runtime-link=shared 
		link=static 
		cxxflags="-fPIC"
		stage
  	WORKING_DIRECTORY ${BOOST_ROOT} 
	OUTPUT_QUIET
	RESULT_VARIABLE error_result
  	)
    if(error_result)
      message(FATAL_ERROR "Boost build has errors. ${error_result}")
    else()
      file(GLOB Boost_LIBRARIES ${BOOST_ROOT}/stage/lib/libboost*)
      set(Boost_INCLUDE_DIRS ${BOOST_ROOT})
    endif()
  endif()
endif()

#message(STATUS "  Boost_INCLUDE_DIRS = ${Boost_INCLUDE_DIRS}")
#message(STATUS "  Boost_LIBRARIES = ${Boost_LIBRARIES}")

STRING(REGEX REPLACE ";" "@@@" Boost_LIBRARIES "${Boost_LIBRARIES}")
FILE(APPEND "${EXPORT_FILE_NAME}" "BOOST_ROOT@@@${BOOST_ROOT}\n")
FILE(APPEND "${EXPORT_FILE_NAME}" "Boost_INCLUDE_DIRS@@@${Boost_INCLUDE_DIRS}\n")
FILE(APPEND "${EXPORT_FILE_NAME}" "Boost_LIBRARIES@@@${Boost_LIBRARIES}\n")


