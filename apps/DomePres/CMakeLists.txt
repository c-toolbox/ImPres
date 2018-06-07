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
set(APP_NAME DomePres)

set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")
set(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/shared/user_cmake/Modules")

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

if(ZXING_ENABLE)
	find_package(ZXing REQUIRED)

	if(ZXING_FOUND)
		add_definitions(-DZXING_ENABLED)
	endif()
	set(IMPRES_ZXING_SRC 
	${CMAKE_SOURCE_DIR}/shared/BGR24LuminanceSource.h
	${CMAKE_SOURCE_DIR}/shared/BGR24LuminanceSource.cpp
	${CMAKE_SOURCE_DIR}/shared/QRCodeInterpreter.h
	${CMAKE_SOURCE_DIR}/shared/QRCodeInterpreter.cpp)
else()
	set(ZXING_INCLUDE_DIRECTORY "")
	set(ZXING_LIBRARY "")
	set(IMPRES_ZXING_SRC "")
endif()

if(RGBEASY_ENABLE)
	if(NOT RGBEASY_INCLUDE_DIRECTORY)
		if( WIN32 )
			find_path(RGBEASY_INCLUDE_DIRECTORY
				NAMES RGBAPI.h
				PATHS $ENV{SGCT_ROOT_DIR}/additional_includes/RGBEasy ${CMAKE_SOURCE_DIR}/ext/sgct/additional_includes/RGBEasy NO_DEFAULT_PATH
				REQUIRED)
		else()
			find_path(RGBEASY_INCLUDE_DIRECTORY
				NAMES RGBAPI.h
				PATH_SUFFIXES RGBEasy
				PATHS $ENV{SGCT_ROOT_DIR}/additional_includes/RGBEasy ${CMAKE_SOURCE_DIR}/ext/sgct/additional_includes/RGBEasy
				REQUIRED)
		endif()
	endif()
	if(NOT RGBEASY_LIBARIES_DIRECTORY)
		if( WIN32 )
			find_path(RGBEASY_LIBARIES_DIRECTORY
				NAMES RGBEasyLibs.txt
				PATHS $ENV{SGCT_ROOT_DIR}/additional_libs/RGBEasy ${CMAKE_SOURCE_DIR}/ext/sgct/additional_libs/RGBEasy NO_DEFAULT_PATH
				REQUIRED)
		else()
			find_path(RGBEASY_LIBARIES_DIRECTORY
				NAMES RGBEasyLibs.txt
				PATH_SUFFIXES RGBEasy
				PATHS $ENV{SGCT_ROOT_DIR}/additional_libs/RGBEasy ${CMAKE_SOURCE_DIR}/ext/sgct/additional_libs/RGBEasy
				REQUIRED)
		endif()
	endif()
	if(RGBEASY_INCLUDE_DIRECTORY AND RGBEASY_LIBARIES_DIRECTORY)
		add_definitions(-DRGBEASY_ENABLED)
	endif()
	set(IMPRES_RGBEASY_SRC
	${CMAKE_SOURCE_DIR}/shared/RGBEasyCaptureCPU.cpp
	${CMAKE_SOURCE_DIR}/shared/RGBEasyCaptureCPU.hpp	
	${CMAKE_SOURCE_DIR}/shared/RGBEasyCaptureGPU.cpp
	${CMAKE_SOURCE_DIR}/shared/RGBEasyCaptureGPU.hpp)
else()
	set(RGBEASY_INCLUDE_DIRECTORY "")
	set(RGBEASY_LIBARIES_DIRECTORY "")
	set(IMPRES_RGBEASY_SRC "")
endif()

if(OPENVR_SUPPORT)
	find_package(OpenVR REQUIRED)
	
	if(OPENVR_FOUND)
		add_definitions(-DOPENVR_SUPPORT)
	endif()

	if(NOT SGCT_OPENVR_INCLUDE_DIRECTORY)
		if( WIN32 )
			find_path(SGCT_OPENVR_INCLUDE_DIRECTORY
				NAMES SGCTOpenVR.h
				PATHS ${SGCT_BASE_DIRECTORY}/additional_includes/openvr ${CMAKE_SOURCE_DIR}/additional_includes/openvr NO_DEFAULT_PATH
				REQUIRED)
		else()
			find_path(SGCT_OPENVR_INCLUDE_DIRECTORY
				NAMES SGCTOpenVR.h
				PATH_SUFFIXES SGCTOpenVR
				PATHS ${SGCT_BASE_DIRECTORY}/additional_includes/openvr ${CMAKE_SOURCE_DIR}/additional_includes/openvr
				REQUIRED)
		endif()
	else()
		set(SGCT_OPENVR_FILES ${SGCT_OPENVR_INCLUDE_DIRECTORY}/SGCTOpenVR.h ${SGCT_OPENVR_INCLUDE_DIRECTORY}/SGCTOpenVR.cpp)
	endif()
else()
	set(OPENVR_INCLUDE_DIRS "")
	set(SGCT_OPENVR_INCLUDE_DIRECTORY "")
	set(OPENVR_LIBRARY "")
endif()
##OpenVR section end####################

add_executable(${APP_NAME}
	main.cpp
	${CMAKE_SOURCE_DIR}/shared/FFmpegCapture.cpp
	${CMAKE_SOURCE_DIR}/shared/FFmpegCapture.hpp
	${IMGUI_INCLUDE_DIRECTORY}/imconfig.h
    ${IMGUI_INCLUDE_DIRECTORY}/imgui.h
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_internal.h
    ${IMGUI_INCLUDE_DIRECTORY}/imgui.cpp
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_demo.cpp
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_draw.cpp
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_impl_glfw_gl3.h
    ${IMGUI_INCLUDE_DIRECTORY}/imgui_impl_glfw_gl3.cpp
	${IMPRES_RGBEASY_SRC}
	${IMPRES_ZXING_SRC}
	${SGCT_OPENVR_FILES}
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

if(RGBEASY_ENABLE)
	set(RGBEASY_LIBS ${RGBEASY_LIBARIES_DIRECTORY}/RGBEASY.LIB)
	if( WIN32 )
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			# 64 bits
			set(RGBEASY_LIBS
				debug ${RGBEASY_LIBARIES_DIRECTORY}/x64/Debug/RGBEASY.LIB
				optimized ${RGBEASY_LIBARIES_DIRECTORY}/x64/Release/RGBEASY.LIB
			)
		elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
			# 32 bits
			set(RGBEASY_LIBS
				debug ${RGBEASY_LIBARIES_DIRECTORY}/Win32/Debug/RGBEASY.LIB
				optimized ${RGBEASY_LIBARIES_DIRECTORY}/Win32/Release/RGBEASY.LIB
			)
		endif()
	endif()
else()
	set(RGBEASY_LIBS "")
endif()

find_package(OpenGL REQUIRED)
find_package(FFmpeg REQUIRED)

set(FFMPEG_INCLUDES
	${FFMPEG_LIBAVCODEC_INCLUDE_DIRS}
	${FFMPEG_LIBAVUTIL_INCLUDE_DIRS}
	${FFMPEG_LIBAVDEVICE_INCLUDE_DIRS}
	${FFMPEG_LIBAVFORMAT_INCLUDE_DIRS}
	${FFMPEG_LIBSWSCALE_INCLUDE_DIRS}
	${FFMPEG_LIBAVRESAMPLE_INCLUDE_DIRS})

include_directories(${SGCT_INCLUDE_DIRECTORY} ${FFMPEG_ROOT}/include ${FFMPEG_INCLUDES} ${ZXING_INCLUDE_DIRECTORY} ${IMGUI_INCLUDE_DIRECTORY} ${OPENVR_INCLUDE_DIRS} ${SGCT_OPENVR_INCLUDE_DIRECTORY} ${RGBEASY_ROOT}/include ${RGBEASY_INCLUDE_DIRECTORY} ${CMAKE_SOURCE_DIR}/shared)

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
		${OPENVR_LIBRARY}
		${RGBEASY_LIBS}
	)
elseif( WIN32 ) #MINGW or similar
	set(LIBS
		${SGCT_LIBS}
		${OPENGL_gl_LIBRARY}
		ws2_32
		${FFMPEG_LIBS}
		${ZXING_LIBRARY}
		${OPENVR_LIBRARY}
		${RGBEASY_LIBS}
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
		${OPENVR_LIBRARY}
		${RGBEASY_LIBS}
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
		${OPENVR_LIBRARY}
		${RGBEASY_LIBS}		
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
