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
set(APP_NAME ImPresDomeViewer)

set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")
set(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/user_cmake/Modules)

project(${APP_NAME})

macro (set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
set_property (TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY}
	    ${XCODE_VALUE})
endmacro (set_xcode_property)

if(NOT IMGUI_INCLUDE_DIRECTORY)
    if( WIN32 )
        find_path(IMGUI_INCLUDE_DIRECTORY
            NAMES imgui.h
            PATHS ${SGCT_BASE_DIRECTORY}/additional_includes/imgui NO_DEFAULT_PATH
            REQUIRED)
    else()
        find_path(IMGUI_INCLUDE_DIRECTORY
            NAMES imgui.h
            PATH_SUFFIXES imgui
            PATHS ${SGCT_BASE_DIRECTORY}/additional_includes/imgui
            REQUIRED)
    endif()
endif()

option(USE_ZXING "Use ZXING to enable QR decoding and more" OFF)
if(USE_ZXING)
	find_package(ZXing REQUIRED)

	if(ZXING_FOUND)
		add_definitions(-DZXING_ENABLED)
	endif()
	set(IMPRES_ZXING_SRC BGR24LuminanceSource.h BGR24LuminanceSource.cpp)
else()
	set(ZXING_INCLUDE_DIRECTORY "")
	set(ZXING_LIBRARY "")
	set(IMPRES_ZXING_SRC "")
endif()

add_executable(${APP_NAME}
	main.cpp
	Capture.cpp
	Capture.hpp
	${IMGUI_INCLUDE_DIRECTORY}/imconfig.h
    ${IMGUI_INCLUDE_DIRECTORY}/imgui.h
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_internal.h
    ${IMGUI_INCLUDE_DIRECTORY}/imgui.cpp
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_demo.cpp
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_draw.cpp
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_impl_glfw_gl3.h
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_impl_glfw_gl3.cpp
	${IMPRES_ZXING_SRC}	
)

#copy data
file(GLOB files
	"${PROJECT_SOURCE_DIR}/*.png"
	"${PROJECT_SOURCE_DIR}/*.jpg"
	"${PROJECT_SOURCE_DIR}/*.xml"
	"${PROJECT_SOURCE_DIR}/*.vert*"
	"${PROJECT_SOURCE_DIR}/*.frag*")
file(COPY ${files} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
if(WIN32)
	file(GLOB scripts "${PROJECT_SOURCE_DIR}/*.bat")
	file(COPY ${scripts} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ${scripts} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
	file(COPY ${scripts} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
	file(COPY ${files} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
	file(COPY ${files} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
elseif(UNIX)
	file(GLOB scripts "${PROJECT_SOURCE_DIR}/*.command")
	file(COPY ${scripts} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()
	
if(APPLE)
	option(SGCT_CPP11 "Use libc++ instead of libstdc++" ON)
	set(CMAKE_OSX_ARCHITECTURES "x86_64")
	if(CMAKE_GENERATOR STREQUAL Xcode)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")
	endif()
endif()
	
if(NOT DEFINED SGCT_RELEASE_LIBRARY)
	if(SGCT_CPP11)	
		find_library(SGCT_RELEASE_LIBRARY
			NAMES sgct_cpp11 libsgct_cpp11 REQUIRED)
	else()
		find_library(SGCT_RELEASE_LIBRARY
			NAMES sgct libsgct REQUIRED)
	endif()
endif()
		
if(NOT DEFINED SGCT_DEBUG_LIBRARY)		
	if(SGCT_CPP11)	
		find_library(SGCT_DEBUG_LIBRARY 
			NAMES sgct_cpp11d libsgct_cpp11d REQUIRED)
	else()
		find_library(SGCT_DEBUG_LIBRARY 
			NAMES sgctd libsgctd REQUIRED)
	endif()
endif()
	
set(SGCT_LIBS
	debug ${SGCT_DEBUG_LIBRARY}
	optimized ${SGCT_RELEASE_LIBRARY}
)

find_package(OpenGL REQUIRED)
find_package(FFmpeg REQUIRED)

set(FFMPEG_INCLUDES
	${FFMPEG_LIBAVCODEC_INCLUDE_DIRS}
	${FFMPEG_LIBAVUTIL_INCLUDE_DIRS}
	${FFMPEG_LIBAVDEVICE_INCLUDE_DIRS}
	${FFMPEG_LIBAVFORMAT_INCLUDE_DIRS}
	${FFMPEG_LIBSWSCALE_INCLUDE_DIRS}
	${FFMPEG_LIBAVRESAMPLE_INCLUDE_DIRS})

include_directories(${SGCT_INCLUDE_DIRECTORY} ${FFMPEG_ROOT}/include ${FFMPEG_INCLUDES} ${ZXING_INCLUDE_DIRECTORY} ${IMGUI_INCLUDE_DIRECTORY})

set(FFMPEG_LIBS
	${FFMPEG_LIBAVCODEC_LIBRARIES}
	${FFMPEG_LIBAVUTIL_LIBRARIES}
	${FFMPEG_LIBAVDEVICE_LIBRARIES}
	${FFMPEG_LIBAVFORMAT_LIBRARIES}
	${FFMPEG_LIBSWSCALE_LIBRARIES}
	${FFMPEG_LIBAVRESAMPLE_LIBRARIES})

if( MSVC )
	set(LIBS
		${SGCT_LIBS}
		${OPENGL_gl_LIBRARY}
		ws2_32
		${FFMPEG_LIBS}
		${ZXING_LIBRARY}
	)
elseif( WIN32 ) #MINGW or similar
	set(LIBS
		${SGCT_LIBS}
		${OPENGL_gl_LIBRARY}
		ws2_32
		${FFMPEG_LIBS}
		${ZXING_LIBRARY}
	)
elseif( APPLE )
	find_library(COCOA_LIBRARY Cocoa REQUIRED)
	find_library(IOKIT_LIBRARY IOKit REQUIRED)
	find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)

	set(LIBS
		${SGCT_LIBS}
		${OPENGL_gl_LIBRARY}
		${COCOA_LIBRARY}
		${IOKIT_LIBRARY}
		${COREVIDEO_LIBRARY}
		${FFMPEG_LIBS}
		${ZXING_LIBRARY}
	)
else() #linux
	find_package(X11 REQUIRED)
	find_package(Threads REQUIRED)

	set(LIBS
		${SGCT_LIBS}
		${OPENGL_gl_LIBRARY}
		${X11_X11_LIB}
		${X11_Xrandr_LIB}
		${X11_Xinerama_LIB}
		${X11_Xinput_LIB}
		${X11_Xxf86vm_LIB}
		${X11_Xcursor_LIB}
		${CMAKE_THREAD_LIBS_INIT}
		${FFMPEG_LIBS}
		${ZXING_LIBRARY}		
	)
endif()

if( MSVC )
		if( "${MSVC_VERSION}" LESS 1600 ) #less than visual studio 2010
		STRING(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replacementFlags
			${CMAKE_EXE_LINKER_FLAGS_DEBUG})
		SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${replacementFlags}" )
		
		STRING(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replacementFlags
			${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
		SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${replacementFlags}" )
	else()
		STRING(REPLACE "INCREMENTAL" "INCREMENTAL:NO" replacementFlags
			${CMAKE_EXE_LINKER_FLAGS_DEBUG})
		SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${replacementFlags}" )
		
		STRING(REPLACE "INCREMENTAL" "INCREMENTAL:NO" replacementFlags
			${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
		SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${replacementFlags}" )
	endif()
endif()
	
if(MSVC AND NOT "${MSVC_VERSION}" LESS 1400)
	add_definitions( "/MP" )
endif()

if( WIN32 )
	add_definitions(-D__WIN32__)
	if( MINGW )
		set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
	endif()
elseif( APPLE  )
	add_definitions(-D__APPLE__)
	if(SGCT_CPP11)
		set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ ${CMAKE_CXX_FLAGS}")
		set_xcode_property(${APP_NAME} CLANG_CXX_LANGUAGE_STANDARD "c++11")
		set_xcode_property(${APP_NAME} CLANG_CXX_LIBRARY "libc++")
	else()
		set(CMAKE_CXX_FLAGS "-std=c++0x -stdlib=libstdc++ ${CMAKE_CXX_FLAGS}")
		set_xcode_property(${APP_NAME} CLANG_CXX_LANGUAGE_STANDARD "c++0x")
		set_xcode_property(${APP_NAME} CLANG_CXX_LIBRARY "libstdc++")
	endif()
else()
	add_definitions(-D__LINUX__)
	set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif()

target_link_libraries(${APP_NAME} ${LIBS})
