cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(NetRadiant C CXX)

#-----------------------------------------------------------------------
# Builtins: configuration
#-----------------------------------------------------------------------

set(BUILTINS_INSTALL_DIR "${CMAKE_BINARY_DIR}/builtins")
set(BUILTINS_INCLUDE_DIR "${BUILTINS_INSTALL_DIR}/include")
set(BUILTINS_LIB_DIR "${BUILTINS_INSTALL_DIR}/lib")
set(BUILTINS_PKGCONFIG_DIR "${BUILTINS_INSTALL_DIR}/lib/pkgconfig")

option(BUILTIN_GTKGLEXT "Builtin GtkGLExt" OFF)
option(BUILTIN_GTKTHEME_MOJAVE "Builtin Mojave GTK theme" OFF)

if (APPLE)
    set(BUILTIN_GTKGLEXT ON)
    set(BUILTIN_GTKTHEME_MOJAVE ON)
endif ()

set(BUILTIN_GTKGLEXT_BUILT OFF CACHE INTERNAL "...")
if (EXISTS "${BUILTINS_PKGCONFIG_DIR}/gtkglext-1.0.pc")
    set(BUILTIN_GTKGLEXT_BUILT ON)
endif ()

if (EXISTS "${BUILTINS_INCLUDE_DIR}")
    include_directories("${BUILTINS_INCLUDE_DIR}")
endif ()

if (EXISTS "${BUILTINS_LIB_DIR}")
    set(ENV{LD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH}:${BUILTINS_LIB_DIR}")
    link_directories("${BUILTINS_LIB_DIR}")
endif ()

if (EXISTS "${BUILTINS_PKGCONFIG_DIR}")
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${BUILTINS_PKGCONFIG_DIR}")
endif ()

#-----------------------------------------------------------------------
# CMake modules
#-----------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

#-----------------------------------------------------------------------
# Submodules
#-----------------------------------------------------------------------

if (EXISTS "${PROJECT_SOURCE_DIR}/libs/crunch/inc/crn_decomp.h")
    set(DEFAULT_BUILD_CRUNCH ON CACHE INTERNAL "...")
else ()
    set(DEFAULT_BUILD_CRUNCH OFF CACHE INTERNAL "...")
endif ()

if (EXISTS "${PROJECT_SOURCE_DIR}/tools/unvanquished/daemonmap/tools/quake3/q3map2/main.c")
    set(DEFAULT_BUILD_DAEMONMAP ON CACHE INTERNAL "...")
else ()
    set(DEFAULT_BUILD_DAEMONMAP OFF CACHE INTERNAL "...")
endif ()

#-----------------------------------------------------------------------
# Build options
#-----------------------------------------------------------------------

option(BUILD_RADIANT "Build the GUI" ON)
option(BUILD_TOOLS "Build the tools" ON)
option(BUILD_CRUNCH "Build Crunch image support" ${DEFAULT_BUILD_CRUNCH})
option(BUILD_DAEMONMAP "Build daemonmap navigation mesh generator" ${DEFAULT_BUILD_DAEMONMAP})
option(DOWNLOAD_GAMEPACKS "Download game packs" ON)
option(USE_WERROR "Build with -Werror -pedantic-errors" OFF)
option(FHS_INSTALL "Install according to Filesystem Hierarchy Standard" OFF)

set(BUILD_BINARIES OFF CACHE INTERNAL "...")
if (BUILD_RADIANT OR BUILD_TOOLS)
    set(BUILD_BINARIES ON)
endif ()

if (BUILD_BINARIES)
    if (WIN32 OR APPLE
        OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux"
        OR "${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD" )
        set(BUNDLING_SUPPORTED ON)
    endif()

    if (BUNDLING_SUPPORTED)
        if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
            set(BUNDLE_LIBRARIES_DEFAULT ON)
        else ()
            set(BUNDLE_LIBRARIES_DEFAULT OFF)
        endif ()

        option(BUNDLE_LIBRARIES "Bundle libraries" ${BUNDLE_LIBRARIES_DEFAULT})
    endif ()

    if (BUNDLE_LIBRARIES AND BUNDLING_SUPPORTED)
        set(FHS_INSTALL OFF CACHE PATH "Disabled because of BUNDLE_LIBRARIES" FORCE)
    endif ()
endif ()

#-----------------------------------------------------------------------
# Paths
#-----------------------------------------------------------------------

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "..." FORCE)
endif ()

if (APPLE)
    set(APPLE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/NetRadiant.app)
    set(FINAL_INSTALL_PREFIX ${APPLE_INSTALL_PREFIX}/Contents/Prefix)
else ()
    set(FINAL_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif ()

set(RADIANT_NAME "NetRadiant" CACHE INTERNAL "...")
add_definitions(-DRADIANT_NAME="${RADIANT_NAME}")
set(RADIANT_BASENAME "netradiant" CACHE INTERNAL "...")
add_definitions(-DRADIANT_BASENAME="${RADIANT_BASENAME}")

set(RADIANT_BIN_DIR ${FINAL_INSTALL_PREFIX} CACHE INTERNAL "...")
set(RADIANT_LIB_DIR ${FINAL_INSTALL_PREFIX}/lib CACHE INTERNAL "...")
set(RADIANT_ADDONS_DIR ${FINAL_INSTALL_PREFIX} CACHE INTERNAL "...")
set(RADIANT_ETC_DIR ${FINAL_INSTALL_PREFIX}/etc CACHE INTERNAL "...")
set(RADIANT_SHARE_DIR ${FINAL_INSTALL_PREFIX}/share CACHE INTERNAL "...")
set(RADIANT_DATA_DIR ${FINAL_INSTALL_PREFIX} CACHE INTERNAL "...")

if (FHS_INSTALL AND NOT BUNDLE_LIBRARIES)
    set(RADIANT_BIN_DIR ${FINAL_INSTALL_PREFIX}/bin)
    set(RADIANT_LIB_DIR ${FINAL_INSTALL_PREFIX}/lib)
    set(RADIANT_ADDONS_DIR ${RADIANT_LIB_DIR}/${RADIANT_BASENAME})
    set(RADIANT_ETC_DIR ${FINAL_INSTALL_PREFIX}/etc)
    set(RADIANT_SHARE_DIR ${FINAL_INSTALL_PREFIX}/share)
    set(RADIANT_DATA_DIR ${RADIANT_SHARE_DIR}/${RADIANT_BASENAME})
endif ()

set(GAMEPACKS_DOWNLOAD_DIR ${PROJECT_BINARY_DIR}/download CACHE PATH "Where to store downloaded game packs")

#-----------------------------------------------------------------------
# Version
#-----------------------------------------------------------------------

# CMake 3.0+ would allow this in project()
set(RADIANT_VERSION_MAJOR 1)
set(RADIANT_VERSION_MINOR 5)
set(RADIANT_VERSION_PATCH 0)
set(RADIANT_VERSION "${RADIANT_VERSION_MAJOR}.${RADIANT_VERSION_MINOR}.${RADIANT_VERSION_PATCH}")

set(RADIANT_ABOUTMSG "Custom build" CACHE STRING "About message")

set(RADIANT_VERSION_STRING "${RADIANT_VERSION}n")

if (NOT DEFINED GIT_VERSION)
     find_package(Git REQUIRED)

     execute_process(
          COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
          WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
          OUTPUT_VARIABLE GIT_VERSION
          OUTPUT_STRIP_TRAILING_WHITESPACE
     )

     if (GIT_VERSION)
         set(RADIANT_VERSION_STRING "${RADIANT_VERSION_STRING}-git-${GIT_VERSION}")
     endif ()
endif ()

message(STATUS "Building ${PROJECT_NAME} ${RADIANT_VERSION_STRING} ${RADIANT_ABOUTMSG}")

#-----------------------------------------------------------------------
# Language standard
#-----------------------------------------------------------------------

if (BUILD_BINARIES)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    if (CMAKE_VERSION VERSION_LESS "3.1")
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
            include(CheckCXXCompilerFlag)
            check_cxx_compiler_flag(--std=c++${CMAKE_CXX_STANDARD} STD_CXX)
            if (STD_CXX)
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++${CMAKE_CXX_STANDARD}")
            else ()
                message(SEND_ERROR "Requires C++${CMAKE_CXX_STANDARD} or better")
            endif ()
        else ()
            message(WARNING "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}, make sure it supports C++${CMAKE_CXX_STANDARD}")
        endif ()
    endif ()
endif ()

#-----------------------------------------------------------------------
# Flags
#-----------------------------------------------------------------------

if (BUILD_BINARIES)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")

    macro (addflags_c args)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${args}")
    endmacro ()

    macro (addflags_cxx args)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${args}")
    endmacro ()

    macro (addflags args)
        addflags_c("${args}")
        addflags_cxx("${args}")
    endmacro ()

    addflags("-fno-strict-aliasing")
    if (NOT WIN32)
        addflags("-fvisibility=hidden")
    endif ()

    if (USE_WERROR)
        addflags("-Werror")
        addflags("-pedantic-errors")
    endif ()

    addflags("-Wall")
    addflags("-Wextra")
    addflags("-pedantic")

    addflags_c("-Wno-deprecated-declarations") # vfs.c: g_strdown

    addflags("-Wno-unused-function")
    addflags("-Wno-unused-variable")
    addflags("-Wno-unused-parameter")

    if (APPLE)
        add_definitions(-DGL_SILENCE_DEPRECATION=1)
    endif()

    set(CMAKE_POSITION_INDEPENDENT_CODE 1)
endif ()

if (BUILD_RADIANT)
    set(GTK_TARGET 2 CACHE STRING "GTK target")
    add_definitions(-DGTK_TARGET=${GTK_TARGET})

    if (WIN32)
        add_definitions(-DWORKAROUND_WINDOWS_FLOATING_WINDOW=1)

        if (GTK_TARGET EQUAL 2)
            add_definitions(-DWORKAROUND_WINDOWS_GTK2_GLWIDGET=1)
        endif ()
    endif ()

    if (APPLE)
        if (GTK_TARGET EQUAL 2)
            add_definitions(-DWORKAROUND_MACOS_GTK2_DESTROY=1)
            add_definitions(-DWORKAROUND_MACOS_GTK2_GLWIDGET=1)
            add_definitions(-DWORKAROUND_MACOS_GTK2_LAGGYPOINTER=1)
        endif ()
    endif ()
endif ()

#-----------------------------------------------------------------------
# Defs
#-----------------------------------------------------------------------

if (BUILD_BINARIES)
    add_definitions(-DRADIANT_VERSION="${RADIANT_VERSION}")
    add_definitions(-DRADIANT_VERSION_STRING="${RADIANT_VERSION_STRING}")

    if (NOT CMAKE_BUILD_TYPE MATCHES Release)
        add_definitions(-D_DEBUG=1)
    endif ()
endif ()

if (BUILD_RADIANT)
    add_definitions(-DRADIANT_ABOUTMSG="${RADIANT_ABOUTMSG}")

    macro (disable_deprecated name gtk2only)
        add_definitions(-D${name}_DISABLE_SINGLE_INCLUDES)
        if ((${gtk2only} EQUAL 0) OR (GTK_TARGET EQUAL 2))
            add_definitions(-D${name}_DISABLE_DEPRECATED)
        endif ()
    endmacro ()

    disable_deprecated(ATK 0)
    disable_deprecated(G 0)
    disable_deprecated(GDK 0)
    disable_deprecated(GDK_PIXBUF 0)
    disable_deprecated(GTK 1)
    disable_deprecated(PANGO 0)
endif ()

if (BUILD_BINARIES)
    add_custom_target(binaries ALL)

    if(APPLE OR ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
        link_directories(
            /usr/local/lib
        )
    endif ()

    if (WIN32)
        add_definitions(
            -DWIN32=1
            -D_WIN32=1
        )
    else ()
        add_definitions(
            -DPOSIX=1
        )
    endif ()
endif ()

if (BUILD_RADIANT AND NOT APPLE AND NOT WIN32)
    set(XWINDOWS ON)
    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
    add_definitions(-DXWINDOWS=1)
endif ()

if (BUILD_BINARIES)
    include_directories("${PROJECT_SOURCE_DIR}/include")
    include_directories("${PROJECT_SOURCE_DIR}/libs")

    macro (copy_dlls target)
        if (BUNDLE_LIBRARIES AND BUNDLING_SUPPORTED)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${PROJECT_SOURCE_DIR}/library-bundler"
                "register" "${PROJECT_BINARY_DIR}" "$<TARGET_FILE:${target}>"
                VERBATIM
            )
            add_dependencies(bundle ${target})
        endif ()
    endmacro ()
endif ()

#-----------------------------------------------------------------------
# Bundle
#-----------------------------------------------------------------------

if (BUILD_BINARIES)
    if (BUNDLE_LIBRARIES AND BUNDLING_SUPPORTED)
        add_subdirectory(bundle)
    endif ()
endif ()

#-----------------------------------------------------------------------
# Builtins: targets
#-----------------------------------------------------------------------

add_custom_target(builtins)

if (BUILTIN_GTKGLEXT OR BUILTIN_GTKTHEME_MOJAVE)
    include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
endif ()

if (BUILTIN_GTKGLEXT)
    set(GTKGLEXT_PKGNAMES "gdk-2.0 gtk+-2.0 glib-2.0")

    if (NOT APPLE)
        set(GTKGLEXT_PKGNAMES "${GTKGLEXT_PKGNAMES} gl")
    endif ()

    execute_process(COMMAND pkg-config --libs ${GTKGLEXT_PKGNAMES}
        OUTPUT_VARIABLE GTKGLEXT_LDFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(GTKGLEXT_LDFLAGS "$ENV{LDFLAGS} -lm ${GTKGLEXT_LDFLAGS}")

    execute_process(COMMAND pkg-config --cflags ${GTKGLEXT_PKGNAMES}
        OUTPUT_VARIABLE GTKGLEXT_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(GTKGLEXT_CFLAGS "$ENV{CFLAGS} ${GTKGLEXT_CFLAGS}")


    set(CONFIGURE_OPTIONS --disable-gtk-doc --disable-gtk-doc-html --disable-gtk-doc-pdf --disable-dependency-tracking --without-x)

    if (CMAKE_BUILD_TYPE MATCHES Release)
        set(CONFIGURE_OPTIONS ${CONFIGURE_OPTIONS} --disable-debug)
    endif ()

    if (APPLE)
        set(GTKGLEXT_GIT_TAG macos)
    else ()
        set(GTKGLEXT_GIT_TAG fixes)
    endif ()

    if (APPLE)
        set(GTKGLEXT_CFLAGS "${GTKGLEXT_CFLAGS} -DGL_SILENCE_DEPRECATION=1 -Wno-deprecated-declarations")
    endif ()

    ExternalProject_Add(gtkglext
        GIT_REPOSITORY https://gitlab.gnome.org/illwieckz/gtkglext.git
        GIT_TAG ${GTKGLEXT_GIT_TAG}
        BUILD_IN_SOURCE ON
        CONFIGURE_COMMAND export CCFLAGS=${GTKGLEXT_CFLAGS}
        CONFIGURE_COMMAND && export LDFLAGS=${GTKGLEXT_LDFLAGS}
        CONFIGURE_COMMAND && export NOCONFIGURE=1
        CONFIGURE_COMMAND && ./autogen.sh
        CONFIGURE_COMMAND && ./configure --prefix "${BUILTINS_INSTALL_DIR}" ${CONFIGURE_OPTIONS}
        BUILD_COMMAND $(MAKE)
        BUILD_COMMAND CFLAGS=${GTKGLEXT_CFLAGS}
        BUILD_COMMAND LDFLAGS=${GTKGLEXT_LDFLAGS}
        INSTALL_COMMAND $(MAKE) install
    )

    include_directories("${BUILTINS_INCLUDE_DIR}/gtkglext-1.0")
    include_directories("${BUILTINS_LIB_DIR}/gtkglext-1.0/include")

    add_dependencies(builtins gtkglext)
    set_target_properties(gtkglext PROPERTIES EXCLUDE_FROM_ALL ON)
endif ()

if (BUNDLE_LIBRARIES)
    # It was required to tell cmake to not patch rpath on macos builtins.
    # It was also required to tell cmake to not patch rpath on freebsd binaries.
    # Patching rpath is done in library_bundler instead so we can skip this
    # step entirely when bundling.
    set(CMAKE_SKIP_RPATH ON)
endif()

if (BUILTIN_GTKTHEME_MOJAVE)
    ExternalProject_Add(gtktheme-mojave
        GIT_REPOSITORY https://github.com/vinceliuice/Mojave-gtk-theme.git
        GIT_TAG master
        BUILD_IN_SOURCE ON
        CONFIGURE_COMMAND echo
        BUILD_COMMAND echo
        INSTALL_COMMAND ./install.sh -o standard -a standard -s standard -i standard -d "${PROJECT_BINARY_DIR}/themes"
    )
    add_dependencies(builtins gtktheme-mojave)
    set_target_properties(gtktheme-mojave PROPERTIES EXCLUDE_FROM_ALL ON)
endif ()

#-----------------------------------------------------------------------
# Libraries
#-----------------------------------------------------------------------

if (BUILD_BINARIES)
    add_subdirectory(libs)
endif ()

if (BUILD_RADIANT)
    add_subdirectory(include)
endif ()

#-----------------------------------------------------------------------
# Plugins
#-----------------------------------------------------------------------

if (BUILD_RADIANT)
    add_subdirectory(contrib)
endif ()

#-----------------------------------------------------------------------
# Modules
#-----------------------------------------------------------------------

if (BUILD_RADIANT)
    add_subdirectory(plugins)
endif ()

#-----------------------------------------------------------------------
# Radiant
#-----------------------------------------------------------------------

if (BUILD_BINARIES)
    macro (radiant_tool name)
        add_dependencies(binaries ${name})

        if (WIN32)
            add_executable(${name} ${ARGN} ${PROJECT_SOURCE_DIR}/include/lib.rc)
        else ()
            add_executable(${name} ${ARGN})
        endif()

        copy_dlls(${name})

        install(
            TARGETS ${name}
            RUNTIME DESTINATION ${RADIANT_BIN_DIR}
        )
    endmacro ()

    if (BUILD_RADIANT)
        add_subdirectory(radiant)
    endif ()
endif ()

#-----------------------------------------------------------------------
# Tools
#-----------------------------------------------------------------------

if (BUILD_TOOLS)
    add_subdirectory(tools)

    file(GLOB DATA_FILES "${PROJECT_SOURCE_DIR}/setup/data/tools/*")

    if (NOT (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR))
        # Copy data files from sources to the build directory
        message(STATUS "Copying data files")
        file(COPY ${DATA_FILES} DESTINATION "${PROJECT_BINARY_DIR}")
    endif ()
endif ()

#-----------------------------------------------------------------------
# Game packs
#-----------------------------------------------------------------------

set(GAMEPACKS_LICENSE_LIST free CACHE STRING "Download game packs by license")
set(GAMEPACKS_NAME_LIST none CACHE STRING "Download game packs by name")

if (DOWNLOAD_GAMEPACKS)
    add_custom_target(gamepacks ALL
       COMMAND "${PROJECT_SOURCE_DIR}/gamepack-manager" --license ${GAMEPACKS_LICENSE_LIST} --name ${GAMEPACKS_NAME_LIST} --download-dir "${GAMEPACKS_DOWNLOAD_DIR}" --install-dir "${PROJECT_BINARY_DIR}/gamepacks" --download --install
       COMMENT "Downloading game packs"
    )
endif ()

#-----------------------------------------------------------------------
# Install
#-----------------------------------------------------------------------

if (BUILD_BINARIES)
    install(DIRECTORY
        setup/data/tools/base
        setup/data/tools/bitmaps
        setup/data/tools/gl
        setup/data/tools/plugins
        docs
        DESTINATION ${RADIANT_DATA_DIR}
    )

    install(FILES
        setup/data/tools/global.xlink
        DESTINATION ${RADIANT_DATA_DIR}
    )
endif ()

if (DOWNLOAD_GAMEPACKS)
    install(DIRECTORY
        ${PROJECT_BINARY_DIR}/gamepacks
        DESTINATION ${RADIANT_DATA_DIR}
    )
endif ()

if (BUILD_BINARIES AND FHS_INSTALL AND NOT WIN32 AND NOT APPLE)
    install(FILES
        icons/netradiant.png
        DESTINATION ${RADIANT_SHARE_DIR}/pixmaps
    )
    install(FILES
        icons/netradiant.desktop
        DESTINATION ${RADIANT_SHARE_DIR}/applications
    )
    install(FILES
        icons/x-netradiant-map.xml
        DESTINATION ${RADIANT_SHARE_DIR}/mime/application
    )
endif ()

if (APPLE)
    install(DIRECTORY
        setup/macos/Contents
        DESTINATION ${APPLE_INSTALL_PREFIX}
    )
    install(DIRECTORY
    ${PROJECT_BINARY_DIR}/themes
        DESTINATION ${APPLE_INSTALL_PREFIX}/Contents/Prefix/share
    )
    install(PROGRAMS
        setup/macos/NetRadiant
        DESTINATION ${APPLE_INSTALL_PREFIX}/Contents/MacOS
    )
endif ()

include(cmake/scripts/package.cmake)
