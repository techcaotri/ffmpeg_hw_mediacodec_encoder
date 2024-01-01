# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("ffmpeg_hw_encoder")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

include(CheckIncludeFile)
include(CheckIncludeFiles)

set(FFMPEG_PATH  ${CMAKE_CURRENT_LIST_DIR}/ffmpeg)
set(MY_LIB_SUFFIX armv8Android)
#set(CMAKE_SYSROOT "")
#set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS FALSE)
#find_path(
#        FFMPEG_avcodec_INCLUDE_DIR
#        NAMES "libavcodec/avformat.h" "libavcodec/version.h"
#        HINTS ENV FFMPEG_PATH ${FFMPEG_PATH} ${CMAKE_SOURCE_DIR}/${FFMPEG_PATH} ${PC_FFMPEG_avcodec_INCLUDE_DIRS}
#        PATH_SUFFIXES ffmpeg libav include)
#
#message("TriPham2: FFMPEG_avcodec_LIBRARY")
#set(CMAKE_LIBRARY_PATH   "${FFMPEG_PATH}/lib/${MY_LIB_SUFFIX}")
#find_library(
#        FFMPEG_avcodec_LIBRARY
#        NAMES "avcodec" "libavcodec"
#)

find_package(
        FFmpeg REQUIRED
        COMPONENTS avcodec
        avdevice
        avutil
        swscale
        avformat
        avfilter
        swresample
        postproc
)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        native-lib.cpp
        ffmpeg_encoder.cpp
        )

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
        FFmpeg::swscale
        FFmpeg::avutil
        FFmpeg::avdevice
        FFmpeg::avcodec
        FFmpeg::avformat
        FFmpeg::avfilter
        FFmpeg::swresample
        FFmpeg::postproc
        z
        # List libraries link to the target library
        android
        log)