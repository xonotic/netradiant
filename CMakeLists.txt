cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(NetRadiant C CXX)

#CMake 3.0+ would allow this in project()
set(NetRadiant_VERSION_MAJOR 1)
set(NetRadiant_VERSION_MINOR 5)
set(NetRadiant_VERSION_PATCH 0)
set(NetRadiant_VERSION ${NetRadiant_VERSION_MAJOR}.${NetRadiant_VERSION_MINOR}.${NetRadiant_VERSION_PATCH})
set(Q3MAP_VERSION 2.5.17n)
set(Q3MAP_STREAM_VERSION 1)

set(RADIANT_VERSION_STRING "${NetRadiant_VERSION}n${GIT_VERSION}")
execute_process(COMMAND git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

if(GIT_VERSION)
    set(RADIANT_VERSION_STRING "${RADIANT_VERSION_STRING}-git-${GIT_VERSION}")
    set(Q3MAP_VERSION "${Q3MAP_VERSION}-git-${GIT_VERSION}")
    add_custom_target(version_git ALL
        COMMAND [ "`git rev-parse --short HEAD`" != "${GIT_VERSION}" ] && ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}
        VERBATIM
    )
endif()

if(CMAKE_EXECUTABLE_SUFFIX)
    set(RADIANT_EXECUTABLE ${CMAKE_EXECUTABLE_SUFFIX})
else()
    execute_process(COMMAND uname -m
        OUTPUT_VARIABLE RADIANT_EXECUTABLE OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

set(RADIANT_ABOUTMSG "Custom build" CACHE STRING "About message")
message(STATUS "Building ${PROJECT_NAME} ${RADIANT_VERSION_STRING} ${RADIANT_ABOUTMSG}")


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# For some reason the above flags don't really work...
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(--std=c++${CMAKE_CXX_STANDARD} STD_CXX)
    if(STD_CXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++${CMAKE_CXX_STANDARD}")
    else()
        message(SEND_ERROR "Requires C++${CMAKE_CXX_STANDARD} or better")
    endif()
else()
    message(WARNING "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}, make sure it supports C++${CMAKE_CXX_STANDARD}")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Enable Debug by default, can be changed with -D CMAKE_BUILD_TYPE=Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-D_DEBUG=1)
endif()

find_package(OpenGL REQUIRED)

add_definitions(-DRADIANT_EXECUTABLE="${RADIANT_EXECUTABLE}")

add_definitions(-DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE)

if(NOT WIN32)
    add_definitions(
        -DPOSIX=1
        -DXWINDOWS=1
    )
    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
else()
    add_definitions(
        -D_WIN32
    )
endif()

configure_file("${CMAKE_SOURCE_DIR}/include/version.cpp.in" "${CMAKE_BINARY_DIR}/version.cpp")
set(INCLUDELIST
    include/aboutmsg.h
    include/cullable.cpp
    include/cullable.h
    include/editable.cpp
    include/editable.h
    include/iarchive.cpp
    include/iarchive.h
    include/ibrush.cpp
    include/ibrush.h
    include/icamera.cpp
    include/icamera.h
    include/idatastream.cpp
    include/idatastream.h
    include/ieclass.cpp
    include/ieclass.h
    include/ientity.cpp
    include/ientity.h
    include/ifilesystem.cpp
    include/ifilesystem.h
    include/ifiletypes.cpp
    include/ifiletypes.h
    include/ifilter.cpp
    include/ifilter.h
    include/igl.cpp
    include/igl.h
    include/iglrender.cpp
    include/iglrender.h
    include/igtkgl.cpp
    include/igtkgl.h
    include/iimage.cpp
    include/iimage.h
    include/imap.cpp
    include/imap.h
    include/imodel.cpp
    include/imodel.h
    include/ipatch.cpp
    include/ipatch.h
    include/iplugin.cpp
    include/iplugin.h
    include/ireference.cpp
    include/ireference.h
    include/irender.cpp
    include/irender.h
    include/iscenegraph.cpp
    include/iscenegraph.h
    include/iscriplib.cpp
    include/iscriplib.h
    include/iselection.cpp
    include/iselection.h
    include/ishaders.cpp
    include/ishaders.h
    include/itexdef.cpp
    include/itexdef.h
    include/itextstream.cpp
    include/itextstream.h
    include/itextures.cpp
    include/itextures.h
    include/itoolbar.cpp
    include/itoolbar.h
    include/iundo.cpp
    include/iundo.h
    include/mapfile.cpp
    include/mapfile.h
    include/modelskin.cpp
    include/modelskin.h
    include/moduleobserver.cpp
    include/moduleobserver.h
    include/modulesystem.cpp
    include/modulesystem.h
    include/nameable.cpp
    include/nameable.h
    include/namespace.cpp
    include/namespace.h
    include/preferencesystem.cpp
    include/preferencesystem.h
    include/qerplugin.cpp
    include/qerplugin.h
    include/renderable.cpp
    include/renderable.h
    include/selectable.cpp
    include/selectable.h
    include/stream_version.h
    include/version.h
    include/warnings.h
    include/windowobserver.cpp
    include/windowobserver.h
)

add_subdirectory(libs)

add_subdirectory(contrib)
add_subdirectory(plugins)

add_subdirectory(radiant build)

add_subdirectory(tools)

if(NOT (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR))
    # Copy data files from sources to the build directory
    message(STATUS "Copying data files")
    file(GLOB DATA_FILES "${CMAKE_SOURCE_DIR}/setup/data/tools/*")
    file(COPY ${DATA_FILES} DESTINATION "${CMAKE_BINARY_DIR}")
    file(GLOB DATA_FILES "${CMAKE_SOURCE_DIR}/docs/*")
    file(COPY ${DATA_FILES} DESTINATION "${CMAKE_BINARY_DIR}/docs")
endif()

option(DOWNLOAD_GAMEPACKS "Download game packs" ON)
add_custom_target(game_packs
    COMMAND ${CMAKE_COMMAND} -E make_directory games
    COMMAND DOWNLOAD_GAMEPACKS=yes SOURCE_DIR="${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/install-gamepacks.sh" "${CMAKE_BINARY_DIR}"
    COMMENT "Downloading game packs"
)
add_custom_command(OUTPUT games
	COMMAND make game_packs
	DEPENDS download-gamepacks.sh install-gamepacks.sh install-gamepack.sh
)
if(DOWNLOAD_GAMEPACKS)
    add_custom_target(game_packs_all ALL DEPENDS games)
endif()