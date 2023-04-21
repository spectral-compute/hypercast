if (NOT ENABLE_SERVER)
    return()
endif()

install(PROGRAMS "${CMAKE_CURRENT_LIST_DIR}/encode-interjection"
                 "${CMAKE_CURRENT_LIST_DIR}/set-interjection-metadata"
                 "${CMAKE_CURRENT_LIST_DIR}/generate-interjection-set-metadata"
        DESTINATION bin)
