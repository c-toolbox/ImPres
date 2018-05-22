  #################################################################################
 #
 # ImPres - Immersive Presentation
 #
 # Copyright (c) 2016
 # Emil Axelsson, Erik Sundén
 # All rights reserved.
 # 
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions are met: 
 # 
 # 1. Redistributions of source code must retain the above copyright notice, this
 # list of conditions and the following disclaimer. 
 # 2. Redistributions in binary form must reproduce the above copyright notice,
 # this list of conditions and the following disclaimer in the documentation
 # and/or other materials provided with the distribution. 
 # 
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 # ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 # WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 # DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 # ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 # (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 # LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 # ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 # SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 # 
 #################################################################################
 
cmake_minimum_required(VERSION 2.8)

project (ImPres)
message(STATUS "Generating ImPres project")

set(IMPRES_BASE_DIR "${PROJECT_SOURCE_DIR}")
set(IMPRES_APPS_DIR "${IMPRES_BASE_DIR}/apps")
set(IMPRES_EXT_DIR "${IMPRES_BASE_DIR}/ext")

set(SGCT_BASE_DIRECTORY "${IMPRES_EXT_DIR}/sgct")
set(SGCT_INCLUDE_DIRECTORY "${SGCT_BASE_DIRECTORY}/include;${SGCT_BASE_DIRECTORY}/src/deps/glfw/include")
set(SGCT_TARGET_PATH ${PROJECT_BINARY_DIR}/ext/sgct) 
set(SGCT_BINARY_PATH ${PROJECT_BINARY_DIR}/ext/sgct)
set(SGCT_SOURCE_PATH ${PROJECT_SOURCE_DIR}/ext/sgct)

if( APPLE )
	if( SGCT_CPP11 )
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/mac_cpp11")
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/mac_cpp11")
	else()
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/mac")
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/mac")
	endif()
elseif( MINGW )
	set(SGCT_COMPILER "mingw")
	if( SGCT_MINGW64 )
		set(SGCT_ARCHITECTURE "x64")
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/${SGCT_COMPILER}_${SGCT_ARCHITECTURE}")
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/${SGCT_COMPILER}_${SGCT_ARCHITECTURE}")
	else()
		set(SGCT_ARCHITECTURE "x86")
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/${SGCT_COMPILER}")
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/${SGCT_COMPILER}")
	endif()
elseif( MSVC )
	if( CMAKE_GENERATOR STREQUAL "Visual Studio 9 2008" OR CMAKE_GENERATOR STREQUAL "Visual Studio 9 2008" )
		set(SGCT_COMPILER "msvc9")
		set(SGCT_ARCHITECTURE "x86")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 9 2008 Win64" OR CMAKE_GENERATOR STREQUAL "Visual Studio 9 2008 Win64" )
		set(SGCT_COMPILER "msvc9")
		set(SGCT_ARCHITECTURE "x64")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 10" OR CMAKE_GENERATOR STREQUAL "Visual Studio 10 2010" )
		set(SGCT_COMPILER "msvc10")
		set(SGCT_ARCHITECTURE "x86")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 10 Win64" OR CMAKE_GENERATOR STREQUAL "Visual Studio 10 2010 Win64" )
		set(SGCT_COMPILER "msvc10")
		set(SGCT_ARCHITECTURE "x64")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 11" OR CMAKE_GENERATOR STREQUAL "Visual Studio 11 2012" )
		set(SGCT_COMPILER "msvc11")
		set(SGCT_ARCHITECTURE "x86")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 11 Win64" OR CMAKE_GENERATOR STREQUAL "Visual Studio 11 2012 Win64" )
		set(SGCT_COMPILER "msvc11")
		set(SGCT_ARCHITECTURE "x64")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 12" OR CMAKE_GENERATOR STREQUAL "Visual Studio 12 2013")
		set(SGCT_COMPILER "msvc12")
		set(SGCT_ARCHITECTURE "x86")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 12 Win64" OR CMAKE_GENERATOR STREQUAL "Visual Studio 12 2013 Win64")
		set(SGCT_COMPILER "msvc12")
		set(SGCT_ARCHITECTURE "x64")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 14" OR CMAKE_GENERATOR STREQUAL "Visual Studio 14 2015")
		set(SGCT_COMPILER "msvc14")
		set(SGCT_ARCHITECTURE "x86")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 14 Win64" OR CMAKE_GENERATOR STREQUAL "Visual Studio 14 2015 Win64")
		set(SGCT_COMPILER "msvc14")
		set(SGCT_ARCHITECTURE "x64")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 15" OR CMAKE_GENERATOR STREQUAL "Visual Studio 15 2015")
		set(SGCT_COMPILER "msvc15")
		set(SGCT_ARCHITECTURE "x86")
	elseif( CMAKE_GENERATOR STREQUAL "Visual Studio 15 Win64" OR CMAKE_GENERATOR STREQUAL "Visual Studio 15 2017 Win64")
		set(SGCT_COMPILER "msvc15")
		set(SGCT_ARCHITECTURE "x64")
	else()
		#use mingw as template
		set(SGCT_COMPILER "mingw")
		set(SGCT_ARCHITECTURE "x86")
	endif()
	
	if(SGCT_ARCHITECTURE STREQUAL "x64")
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/${SGCT_COMPILER}_${SGCT_ARCHITECTURE}")
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/${SGCT_COMPILER}_${SGCT_ARCHITECTURE}")
	else()
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/${SGCT_COMPILER}")
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/${SGCT_COMPILER}")
	endif()
elseif( UNIX AND NOT APPLE )
	if( CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/linux_x64")
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/linux_x64")
	elseif( CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/linux")
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/linux")
	else()
		set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/unix")
		#use linux as template
		set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/linux")
	endif()
else()
	set(SGCT_LIB_PATH "${SGCT_TARGET_PATH}/lib/${CMAKE_SYSTEM_NAME}/${CMAKE_GENERATOR}")
	#use linux as template
	set(SGCT_LIB_SRC_PATH "${PROJECT_SOURCE_DIR}/lib/linux")
endif()

if( APPLE AND SGCT_CPP11 )
    set(SGCT_RELEASE_LIBRARY "${SGCT_LIB_PATH}/libsgct_cpp11.a")
    set(SGCT_DEBUG_LIBRARY "${SGCT_LIB_PATH}/libsgct_cpp11d.a")
elseif( MSVC )
    set(SGCT_RELEASE_LIBRARY "${SGCT_LIB_PATH}/sgct.lib")
    set(SGCT_DEBUG_LIBRARY "${SGCT_LIB_PATH}/sgctd.lib")
else() #mac, mingw and linux
    set(SGCT_RELEASE_LIBRARY "${SGCT_LIB_PATH}/libsgct.a")
    set(SGCT_DEBUG_LIBRARY "${SGCT_LIB_PATH}/libsgctd.a")
endif()

add_subdirectory(${SGCT_BASE_DIRECTORY})
add_subdirectory(apps)
