# Find the sources for the server.
find_sources(SERVER_SOURCES "${CMAKE_CURRENT_LIST_DIR}")

# Create the targets. For testing, we need to create a library so that the tests can link to it.
if (XCMAKE_ENABLE_TESTS)
    set(TARGET live-video-streamer-server-lib)
    set(TARGETS ${TARGET} live-video-streamer-server)

    # Remove main.cpp from the list of sources, as it contains main() which the tests need not to contain.
    list(REMOVE_ITEM SERVER_SOURCES "${CMAKE_CURRENT_LIST_DIR}/main.cpp")

    # For the tests, we need a separate library against which the both the server and its unit tests can be linked.
    add_library(${TARGET} STATIC NOINSTALL "${SERVER_SOURCES}")
    target_include_directories(${TARGET} PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
    target_compile_definitions(${TARGET} PUBLIC WITH_TESTING)

    # Create the server executable, and link it against the library.
    add_executable(live-video-streamer-server "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
    target_link_libraries(live-video-streamer-server PRIVATE ${TARGET})
else()
    set(TARGET live-video-streamer-server)
    set(TARGETS ${TARGET})

    # Create an executable with the server's sources.
    add_executable(${TARGET} "${SERVER_SOURCES}")
    target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

    # Remove extra junk.
    target_link_options(${TARGET} PRIVATE -Wl,--gc-sections)
endif()

# Boost-filesystem is needed to have asyncio FS stuff.
find_package(Boost 1.81.0 REQUIRED COMPONENTS filesystem)
target_link_libraries(${TARGET} PRIVATE Boost::boost Boost::filesystem)

# Remove lots of Boost goo that we don't need or want. E.g: source location strings that probably aid reverse
# engineering.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    target_compile_definitions("${TARGET}" PUBLIC BOOST_DISABLE_CURRENT_LOCATION)
endif()

# Work around https://github.com/boostorg/boost/issues/740.
find_library(URING uring REQUIRED)
add_library(uring UNKNOWN IMPORTED)
set_property(TARGET uring PROPERTY IMPORTED_LOCATION "${URING}")
target_link_libraries(${TARGET} PRIVATE uring)
target_compile_definitions(${TARGET} PUBLIC BOOST_ASIO_HAS_IO_URING BOOST_ASIO_DISABLE_EPOLL)

# nlohmann::json.
find_package(nlohmann_json REQUIRED)
target_link_libraries(${TARGET} PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(${TARGET} PRIVATE JSON_USE_IMPLICIT_CONVERSIONS=0)

# Documentation.
if (XCMAKE_PRIVATE_DOCS)
    add_doxygen(server_doxygen SUBJECT Server PRIVATE INSTALL_DESTINATION private_docs/server TARGETS ${TARGETS})
endif()
