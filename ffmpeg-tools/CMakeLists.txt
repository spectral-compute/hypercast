if (NOT ENABLE_SERVER)
    return()
endif()

install(PROGRAMS "${CMAKE_CURRENT_LIST_DIR}/build-ffmpeg" DESTINATION bin)
