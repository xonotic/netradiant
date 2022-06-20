set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

find_package(OpenGL REQUIRED)

string(SUBSTRING ${CMAKE_SHARED_MODULE_SUFFIX} 1 -1 _clibext)
add_definitions(-DCMAKE_SHARED_MODULE_SUFFIX="${_clibext}")
unset(_clibext)

set(RADIANTLIST
    autosave.cpp autosave.h
    brush.cpp brush.h
    brush_primit.cpp brush_primit.h
    brushmanip.cpp brushmanip.h
    brushmodule.cpp brushmodule.h
    brushnode.cpp brushnode.h
    brushtokens.cpp brushtokens.h
    brushxml.cpp brushxml.h
    build.cpp build.h
    camwindow.cpp camwindow.h
    clippertool.cpp clippertool.h
    commands.cpp commands.h
    console.cpp console.h
    csg.cpp csg.h
    dialog.cpp dialog.h
    eclass.cpp eclass.h
    eclass_def.cpp eclass_def.h
    eclass_doom3.cpp eclass_doom3.h
    eclass_fgd.cpp eclass_fgd.h
    eclass_xml.cpp eclass_xml.h
    entity.cpp entity.h
    entityinspector.cpp entityinspector.h
    entitylist.cpp entitylist.h
    environment.cpp environment.h
    error.cpp error.h
    feedback.cpp feedback.h
    filetypes.cpp filetypes.h
    filterbar.cpp filterbar.h
    filters.cpp filters.h
    findtexturedialog.cpp findtexturedialog.h
    grid.cpp grid.h
    groupdialog.cpp groupdialog.h
    gtkdlgs.cpp gtkdlgs.h
    gtkmisc.cpp gtkmisc.h
    help.cpp help.h
    image.cpp image.h
    main.cpp main.h
    mainframe.cpp mainframe.h
    map.cpp map.h
    mru.cpp mru.h
    nullmodel.cpp nullmodel.h
    parse.cpp parse.h
    patch.cpp patch.h
    patchdialog.cpp patchdialog.h
    patchmanip.cpp patchmanip.h
    patchmodule.cpp patchmodule.h
    plugin.cpp plugin.h
    pluginapi.cpp pluginapi.h
    pluginmanager.cpp pluginmanager.h
    pluginmenu.cpp pluginmenu.h
    plugintoolbar.cpp plugintoolbar.h
    points.cpp points.h
    preferencedictionary.cpp preferencedictionary.h
    preferences.cpp preferences.h
    qe3.cpp qe3.h
    qgl.cpp qgl.h
    referencecache.cpp referencecache.h
    renderer.cpp renderer.h
    renderstate.cpp renderstate.h
    resource.h
    scenegraph.cpp scenegraph.h
    select.cpp select.h
    selection.cpp selection.h
    server.cpp server.h
    shaders.cpp shaders.h
    sockets.cpp sockets.h
    stacktrace.cpp stacktrace.h
    surfacedialog.cpp surfacedialog.h
    texmanip.cpp texmanip.h
    textureentry.cpp textureentry.h
    textures.cpp textures.h
    texwindow.cpp texwindow.h
    timer.cpp timer.h
    treemodel.cpp treemodel.h
    undo.cpp undo.h
    url.cpp url.h
    view.cpp view.h
    watchbsp.cpp watchbsp.h
    winding.cpp winding.h
    windowobservers.cpp windowobservers.h
    xmlstuff.cpp xmlstuff.h
    xywindow.cpp xywindow.h
)

if (WIN32)
    list(APPEND RADIANTLIST multimon.cpp multimon.h)
endif ()

radiant_tool(${RADIANT_BASENAME} WIN32 radiant.rc ${RADIANTLIST})
add_dependencies(${RADIANT_BASENAME} modules)
target_link_libraries(${RADIANT_BASENAME}
    ${CMAKE_DL_LIBS}
    ${LIBXML2_LIBRARIES}
    ${OPENGL_gl_LIBRARY}
    ${GTK${GTK_TARGET}_LIBRARIES}
    ${GTKGL_LIBRARIES}
    includes
    cmdlib
    container
    ddslib
    debugging
    etclib
    filematch
    generic
    l_net
    math
    mathlib
    memory
    modulesystem
    os
    picomodel
    profile
    script
    signal
    splines
    stream
    string
    transformpath
    uilib
    xmllib
)

if (X11_LIBRARIES)
    target_link_libraries(${RADIANT_BASENAME} ${X11_LIBRARIES})
endif ()

# PIE executables were wrongly recognized as shared libraries on Linux
# because of a mistake in the mime-type library.
# Because of that it was not possible to launch such executables from
# the file browser.
#
# While the problem is fixed upstream, most distributions still ship
# with a faulty mime-type library and may do for a long time.
#
# See: https://gitlab.freedesktop.org/xdg/shared-mime-info/-/issues/11
#
# It may produce such warning though at strip time:
#   warning: allocated section `.dynstr' not in segment
# and produce an unusable binary, better not strip when using this hack.
#
# This hack is only wanted for release build and
# when NetRadiant is not installed system-wide.
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux"
    AND "${CMAKE_BUILD_TYPE}" STREQUAL "Release"
    AND NOT FHS_INSTALL)
    target_link_libraries(${RADIANT_BASENAME} -no-pie)
endif()

copy_dlls(${RADIANT_BASENAME})
