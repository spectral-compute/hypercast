# Only if tests are enabled.
if (NOT XCMAKE_ENABLE_TESTS)
    return()
endif()

# Shared test stuff.
add_subdirectory(common)

# Build the test programs.
add_subdirectory(http_server)
add_subdirectory(unit)

# A function to generate test data from commands like sed that read/write to/from stdin and stdout.
function (makeTestInputFromStdio OUT IN)
    # Figure out the build path and make a directory for it.
    set(BUILD_DEST "${CMAKE_CURRENT_BINARY_DIR}/data/${OUT}")
    get_filename_component(BUILD_DEST_DIR "${BUILD_DEST}" DIRECTORY)
    file(MAKE_DIRECTORY "${BUILD_DEST_DIR}")

    # Add a command and target to create the test file.
    string(MAKE_C_IDENTIFIER "StdioTest_${OUT}" TARGET_NAME)
    add_custom_command(OUTPUT "${BUILD_DEST}"
                       COMMAND ${ARGN} < "${IN}" > "${BUILD_DEST}"
                       MAIN_DEPENDENCY "${IN}"
                       COMMENT "Creating test file ${OUT}")
    add_custom_target("${TARGET_NAME}" DEPENDS "${BUILD_DEST}")

    # Make sure the target runs and install it.
    get_filename_component(INSTALL_DIR "test/data/${OUT}" DIRECTORY)
    add_dependencies(live-video-streamer-server_test "${TARGET_NAME}")
    install(FILES "${BUILD_DEST}" DESTINATION "${INSTALL_DIR}")
endfunction()

# Generate some data about the configuration system.
makeTestInputFromStdio("srcinfo/optional_config.txt" "${CMAKE_SOURCE_DIR}/server/src/configuration/configuration.hpp"
                       sed -nE "s|^ *std::optional<.*> ([A-Za-z0-9_]+)\\\;( *//.*)?$|\\1|\\\;T\\\;p")

# Generate SMPTE test videos.
function (makeSmpteTest WIDTH HEIGHT FRAMERATE SAMPLERATE)
    # Figure out the name of the file.
    string(MAKE_C_IDENTIFIER "${FRAMERATE}" C_FRAMERATE)
    string(REGEX REPLACE "^_" "" C_FRAMERATE "${C_FRAMERATE}")
    set(NAME "h264-${WIDTH}x${HEIGHT}-${C_FRAMERATE} aac-${SAMPLERATE}.mkv")

    # Figure out the build path and make a directory for it.
    set(BUILD_DEST "${CMAKE_CURRENT_BINARY_DIR}/data/smpte/${NAME}")
    get_filename_component(BUILD_DEST_DIR "${BUILD_DEST}" DIRECTORY)
    file(MAKE_DIRECTORY "${BUILD_DEST_DIR}")

    # Add a command and target to create the test file.
    string(MAKE_C_IDENTIFIER "SmpteTest_${NAME}" TARGET_NAME)
    add_custom_command(OUTPUT "${BUILD_DEST}"
                       COMMAND ffmpeg -loglevel warning
                                      -f lavfi -i "smptehdbars=rate=${FRAMERATE}:size=${WIDTH}x${HEIGHT}"
                                      -f lavfi -i "sine=frequency=1000:sample_rate=${SAMPLERATE}" -t 1
                                      -c:v h264 -preset placebo -c:a aac -b:v 256k -b:a 64k
                                      -y "${BUILD_DEST}"
                       COMMENT "Creating test file ${NAME}")
    add_custom_target("${TARGET_NAME}" DEPENDS "${BUILD_DEST}")

    # Make sure the target runs and install it.
    add_dependencies(live-video-streamer-server_test "${TARGET_NAME}")
    install(FILES "${BUILD_DEST}" DESTINATION "test/data/smpte")
endfunction()

makeSmpteTest(1920 1080 25 48000)
makeSmpteTest(1920 1080 30000/1001 48000)
makeSmpteTest(1920 1080 50 48000)
