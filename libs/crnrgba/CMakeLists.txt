add_library(crnrgba STATIC
        crn_rgba.h
        crn_rgba.cpp
        ../crunch/crn_decomp.h
        ../crunch/crn_defs.h
        ../crunch/crnlib.h
        )

set_target_properties(crnrgba PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(crnrgba PRIVATE ../crunch)
target_link_libraries(crnrgba PRIVATE ddslib)
target_compile_options(crnrgba PRIVATE -fexceptions)
