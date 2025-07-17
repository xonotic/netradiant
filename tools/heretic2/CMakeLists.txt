include_directories(BEFORE h2data common qcommon)

find_package(OpenGL REQUIRED)

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

radiant_tool(h2data
    h2data/h2data.rc

    common/bspfile.c common/bspfile.h
    common/cmdlib.c common/cmdlib.h
    common/inout.c common/inout.h
    common/l3dslib.c common/l3dslib.h
    common/lbmlib.c common/lbmlib.h
    common/md4.c common/md4.h
    common/path_init.c
    common/polylib.c common/polylib.h
    common/qfiles.c common/qfiles.h
    common/scriplib.c common/scriplib.h
    common/threads.c common/her2_threads.h
    common/token.c common/token.h
    common/trilib.c common/trilib.h

    qcommon/angles.h
    qcommon/arrayedlist.h
    qcommon/flex.h
    qcommon/fmodel.h
    qcommon/h2common.h
    qcommon/placement.h
    qcommon/qfiles.h
    qcommon/q_typedef.h
    qcommon/reference.c qcommon/reference.h
    qcommon/resourcemanager.c qcommon/resourcemanager.h
    qcommon/skeletons.c qcommon/skeletons.h

    h2data/adpcm.h
    h2data/animcomp.c h2data/animcomp.h 
    h2data/anorms.h
    h2data/book.c
    h2data/fmodels.c h2data/qd_fmodel.h
    h2data/images.c
    h2data/jointed.c
    h2data/jointed.h
    h2data/joints.h
    h2data/models.c
    h2data/pics.c
    h2data/qdata.c h2data/qdata.h
    h2data/qd_skeletons.c h2data/qd_skeletons.h
    h2data/resource.h
    h2data/sprites.c
    h2data/svdcmp.c
    h2data/tables.c
    h2data/tmix.c
    h2data/video.c
)

target_compile_definitions(h2data
    PRIVATE
)

target_link_libraries(h2data
    ${LIBXML2_LIBRARIES}
    l_net
    mathlib
)

add_custom_target(heretic2)
add_dependencies(heretic2 h2data q2map)

if (UNIX)
    target_link_libraries(h2data pthread m)
endif ()
