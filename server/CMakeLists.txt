# The rest of this script is meaningful only if the server is turned on.
if (NOT ENABLE_SERVER)
    return()
endif()

# Find the sources for the server.
find_sources(SERVER_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src")

# For the tests, we need a separate library against which the both the server and its unit tests can be linked.
add_library(lvss-lib STATIC NOINSTALL "${SERVER_SOURCES}")
target_include_directories(lvss-lib PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")

if (XCMAKE_ENABLE_TESTS)
    target_compile_definitions(lvss-lib PUBLIC WITH_TESTING)
endif()

# Create the server executable, and link it against the library.
add_executable(live-video-streamer-server main.cpp)
target_link_libraries(live-video-streamer-server PRIVATE lvss-lib)

# Boost-filesystem is needed to have asyncio FS stuff.
find_package(Boost 1.81.0 REQUIRED COMPONENTS filesystem process)
target_link_libraries(lvss-lib PRIVATE Boost::boost Boost::filesystem Boost::process)

# Remove lots of Boost goo that we don't need or want. E.g: source location strings that probably aid reverse
# engineering.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    target_compile_definitions(lvss-lib PUBLIC BOOST_DISABLE_CURRENT_LOCATION)
endif()

# Work around https://github.com/boostorg/boost/issues/740. The bug this mentions is that the subprocess often causes
# the parent process to get stuck in the uring event loop. I suspect it's something to do with not closing/unregistering
# the file descriptors properly. Unfortunately, without uring we have to do synchronous file IO and either wait or use a
# thread and a notification system.
option(ENABLE_URING "Use liburing for IO. This is disabled by default due to a bug in boost::process::v2." Off)
if (ENABLE_URING)
    find_library(URING uring REQUIRED)
    add_library(uring UNKNOWN IMPORTED)
    set_property(TARGET uring PROPERTY IMPORTED_LOCATION "${URING}")
    target_link_libraries(lvss-lib PRIVATE uring)
    target_compile_definitions(lvss-lib PUBLIC BOOST_ASIO_HAS_IO_URING BOOST_ASIO_DISABLE_EPOLL)
endif()

# nlohmann::json.
find_package(nlohmann_json REQUIRED)
target_link_libraries(lvss-lib PRIVATE nlohmann_json::nlohmann_json)
target_compile_definitions(lvss-lib PRIVATE JSON_USE_IMPLICIT_CONVERSIONS=0)

# Documentation.
add_manual(server_docs_customer
    INSTALL_DESTINATION src/docs/live-video-streamer/server
    MANUAL_SRC ${CMAKE_CURRENT_LIST_DIR}/docs
)
if (XCMAKE_PRIVATE_DOCS)
    add_doxygen(server_doxygen
        SUBJECT Server
        PRIVATE INSTALL_DESTINATION private_docs/server
        TARGETS lvss-lib
    )
endif()

add_subdirectory(test)
