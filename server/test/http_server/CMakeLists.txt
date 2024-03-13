find_sources(SOURCES "${CMAKE_CURRENT_LIST_DIR}")
add_test_executable(test-http-server "${SOURCES}")
target_link_libraries(test-http-server PRIVATE lvss-lib test_common)
