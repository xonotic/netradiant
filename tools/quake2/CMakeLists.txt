include_directories(BEFORE common)

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

radiant_tool(q2map
    q2map/q2map.rc

    common/bspfile.c common/bspfile.h
    common/cmdlib.c common/cmdlib.h
    common/inout.c common/inout.h
    common/l3dslib.c common/l3dslib.h
    common/lbmlib.c common/lbmlib.h
    common/md4.c common/md4.h
    common/path_init.c
    common/qfiles.h
    common/polylib.c common/polylib.h
    common/scriplib.c common/scriplib.h
    common/threads.c common/q2_threads.h
    common/trilib.c common/trilib.h
    
    q2map/brushbsp.c
    q2map/csg.c
    q2map/faces.c
    q2map/flow.c
#   q2map/gldraw.c
    q2map/glfile.c
    q2map/leakfile.c
    q2map/lightmap.c
    q2map/main.c q2map/q2map.h
    q2map/map.c
    q2map/nodraw.c
    q2map/patches.c
    q2map/portals.c
    q2map/prtfile.c
    q2map/qbsp.c
    q2map/qbsp.h
    q2map/qrad.c
    q2map/qrad.h
    q2map/qvis.c
    q2map/qvis.h
    q2map/textures.c
    q2map/trace.c
    q2map/tree.c
    q2map/writebsp.c
)

target_compile_definitions(q2map
    PRIVATE
)

target_link_libraries(q2map
    ${LIBXML2_LIBRARIES}
    l_net
    mathlib
)

radiant_tool(qdata3
    qdata/qdata.rc

    common/bspfile.c common/bspfile.h
    common/cmdlib.c common/cmdlib.h
    common/inout.c common/inout.h
    common/l3dslib.c common/l3dslib.h
    common/lbmlib.c common/lbmlib.h
    common/md4.c common/md4.h
    common/path_init.c
    common/qfiles.h
    common/polylib.c common/polylib.h
    common/scriplib.c common/scriplib.h
    common/threads.c common/q2_threads.h
    common/trilib.c common/trilib.h

    qdata/anorms.h
    qdata/images.c
    qdata/models.c
    qdata/qdata.c qdata/qdata.h
    qdata/sprites.c
    qdata/tables.c
    qdata/video.c
)

target_compile_definitions(qdata3
    PRIVATE
)

target_link_libraries(qdata3
    ${LIBXML2_LIBRARIES}
    l_net
    mathlib
)

add_custom_target(quake2)
add_dependencies(quake2 q2map qdata3)

if (UNIX)
    target_link_libraries(q2map pthread m)
    target_link_libraries(qdata3 pthread m)
endif ()
