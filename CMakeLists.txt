cmake_minimum_required(VERSION 3.25)
include(xcmake/scripts/Init.cmake)
project(SpectralLiveVideoStreamer
        VERSION 0.0.0
        DESCRIPTION "An ultra low-latency video streamer"
        HOMEPAGE_URL "https://spectralcompute.co.uk/hyperstream"
        LANGUAGES CXX)
include(XCMake)

# LTO.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# Sub-projects.
add_subdirectory(server)
add_subdirectory(js)

# Documentation.
set(DOCS_PREPROCESSOR_FLAG_NAMES)

add_manual(global_docs_customer INSTALL_DESTINATION docs/live-video-streamer MANUAL_SRC ${CMAKE_CURRENT_LIST_DIR}/docs
           ${DOCS_PREPROCESSOR_FLAG_NAMES})

if (XCMAKE_PRIVATE_DOCS)
    add_manual(global_docs_internal INSTALL_DESTINATION private_docs/live-video-streamer
               MANUAL_SRC ${CMAKE_CURRENT_LIST_DIR}/private_docs)
endif()

# Generate documentation.
if (XCMAKE_ENABLE_DOCS)
    # Create a symlink to xcmake so the documentation script can find it.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
    file(CREATE_LINK "${CMAKE_SOURCE_DIR}/xcmake" "${CMAKE_CURRENT_BINARY_DIR}/docs/xcmake" SYMBOLIC)

    # Actually generate the documentaiton.
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_docs"
        COMMAND cmake -E touch "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_docs"
        COMMAND yarn docs
        DEPENDS js_yarn_bundle "${JS_COPY_DEPS}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generate documentation for the Javascript projects."
    )
    add_custom_target(js_yarn_docs ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_docs")

    # Install the customer documentation. Note that because we're merging with the global documentation, we need to
    # rename index.html.
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/dist/docs/" DESTINATION "docs" PATTERN "*.html.d" EXCLUDE)

    # Install the private documentation.
    if (XCMAKE_PRIVATE_DOCS)
        install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/dist/private_docs/live-video-streamer/private/"
                DESTINATION "private_docs/live-video-streamer" PATTERN "*.html.d" EXCLUDE)
    endif()
endif()

# Configure the packaging.
set(CPACK_PACKAGE_NAME "spectral-live-video-streamer")
