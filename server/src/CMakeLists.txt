# Find the sources for the server.
find_sources(SERVER_SOURCES "${CMAKE_CURRENT_LIST_DIR}")

# Create the targets. For testing, we need to create a library so that the
if (XCMAKE_ENABLE_TESTS)
    set(SERVER_TARGET live-video-streamer-server-lib)

    # Remove main.cpp from the list of sources, as it contains main() which the tests need not to contain.
    list(REMOVE_ITEM SERVER_SOURCES "${CMAKE_CURRENT_LIST_DIR}/main.cpp")

    # For the tests, we need a separate library against which the both the server and its unit tests can be linked.
    add_library(${SERVER_TARGET} STATIC NOINSTALL "${SERVER_SOURCES}")
    target_include_directories(${SERVER_TARGET} PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    target_compile_definitions(${SERVER_TARGET} PUBLIC WITH_TESTING)

    # Create the server executable, and link it against the library.
    add_executable(live-video-streamer-server "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
    target_link_libraries(live-video-streamer-server PRIVATE ${SERVER_TARGET})
else()
    set(SERVER_TARGET live-video-streamer-server)

    # Create an executable with the server's sources.
    add_executable(live-video-streamer-server "${SERVER_SOURCES}")
endif()
