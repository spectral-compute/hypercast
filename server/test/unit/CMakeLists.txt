# Add the gtest program.
include(GTest)
find_sources(SOURCES "${CMAKE_CURRENT_LIST_DIR}")
add_gtest_executable(live-video-streamer-server_test "${SOURCES}")
target_include_directories(live-video-streamer-server_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(live-video-streamer-server_test PRIVATE lvss-lib test_common)
