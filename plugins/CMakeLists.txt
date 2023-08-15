set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/modules")

add_custom_target(modules)
macro(radiant_plugin name)
    message(STATUS "Found Module ${name}")
    add_library(${name} MODULE ${ARGN})
    add_dependencies(modules ${name})
    copy_dlls(${name})
    install(
            TARGETS ${name}
            LIBRARY DESTINATION ${RADIANT_ADDONS_DIR}/modules
    )
endmacro()

add_subdirectory(archivepak)
add_subdirectory(archivewad)
add_subdirectory(archivezip)
add_subdirectory(entity)
add_subdirectory(image)

if(BUILD_CRUNCH)
	add_subdirectory(imagecrn)
endif()

add_subdirectory(imagehl)
add_subdirectory(imagepng)
add_subdirectory(imageq2)
add_subdirectory(imagewebp)
add_subdirectory(iqmmodel)
add_subdirectory(mapq3)
add_subdirectory(mapxml)
add_subdirectory(md3model)
add_subdirectory(model)
add_subdirectory(shaders)
add_subdirectory(vfspk3)
