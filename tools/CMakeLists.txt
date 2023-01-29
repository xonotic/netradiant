set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

add_subdirectory(quake2)
add_subdirectory(heretic2)
add_subdirectory(quake3)

add_custom_target(tools)
add_dependencies(tools quake2 heretic2 quake3)
