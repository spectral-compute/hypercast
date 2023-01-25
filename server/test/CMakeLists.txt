if (NOT XCMAKE_ENABLE_TESTS)
    return()
endif()

include(GTest)
find_sources(TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}")
add_gtest_executable(live-video-streamer-server_test "${TEST_SOURCES}")
target_include_directories(live-video-streamer-server_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(live-video-streamer-server_test PRIVATE live-video-streamer-server-lib)
