find_package(OpenGL REQUIRED)

set(OUTPUT_NAME Ex09_JunkShop)

set(SOURCES ${CMAKE_CURRENT_LIST_DIR}/JunkShop.cpp)

add_executable(${OUTPUT_NAME} ${COMMON_SOURCES} ${SOURCES})

target_link_libraries(${OUTPUT_NAME}
    ${LIBRARIES})

# set(OUTPUT_DIRECTORY ${ROOT_DIRECTORY}Bin/)
# set_target_properties(${OUTPUT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})

install(TARGETS ${OUTPUT_NAME}
    ARCHIVE DESTINATION Bin
    LIBRARY DESTINATION Bin
    RUNTIME DESTINATION Bin)
