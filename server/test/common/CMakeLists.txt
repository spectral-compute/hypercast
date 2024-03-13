find_sources(SOURCES "${CMAKE_CURRENT_LIST_DIR}")
add_library(test_common STATIC NOINSTALL "${SOURCES}")
target_link_libraries(test_common PRIVATE lvss-lib)
target_include_directories(test_common PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
