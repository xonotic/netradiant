if("${RADIANT_IQM_PLUGIN}" STREQUAL "picomodel")
        set(PICO_IQM_FILE "pm_iqm.c")
endif()

add_library(picomodel STATIC
        lwo/clip.c
        lwo/envelope.c
        lwo/list.c
        lwo/lwio.c
        lwo/lwo2.c lwo/lwo2.h
        lwo/lwob.c
        lwo/pntspols.c
        lwo/surface.c
        lwo/vecmath.c
        lwo/vmap.c

        picointernal.c picointernal.h
        picomodel.c ../picomodel.h
        picomodules.c
        pm_3ds.c
        pm_ase.c
        pm_fm.c pm_fm.h
        ${PICO_IQM_FILE}
        pm_lwo.c
        pm_md2.c
        pm_md3.c
        pm_mdc.c
        pm_ms3d.c
        pm_obj.c
        pm_terrain.c
        )
