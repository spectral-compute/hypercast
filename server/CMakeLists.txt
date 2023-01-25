# Options.
option(ENABLE_SERVER "Enable building the C++ server." On)

# The rest of this script is meaningful only if the server is turned on.
if (NOT ENABLE_SERVER)
    return()
endif()

# We're not only building Javascript stuff.
set(JS_ONLY 0 CACHE INTERNAL "" FORCE)

# Add the server's stuff.
add_subdirectory(src)
add_subdirectory(test)

# Documentation.
add_manual(server_docs_customer INSTALL_DESTINATION docs/live-video-streamer/server
                                MANUAL_SRC ${CMAKE_CURRENT_LIST_DIR}/docs)
