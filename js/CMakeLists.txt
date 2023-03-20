# Something that's On for debug builds and Off otherwise.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(DEBUG_ON_OFF On)
else()
    set(DEBUG_ON_OFF Off)
endif()

# Options.
include(CMakeDependentOption)
option(ENABLE_JS "Enable Javascript stuff (e.g: the client library and demo client)." On)
cmake_dependent_option(JS_DEV "Build the Javascript stuff in development mode rather than production mode."
                       ${DEBUG_ON_OFF} ENABLE_JS Off)

# The rest of this script is meaningful only if the Javascript build is turned on.
if (NOT ENABLE_JS)
    return()
endif()

# Figure out the install prefix.
if (JS_ONLY)
    set(JS_INSTALL_PREFIX "")
else()
    set(JS_INSTALL_PREFIX "share/spectral-video-streamer/")
endif()

# Figure out what to exclude from the built Javascript stuff.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set(JS_INSTALL_EXCLUDE)
else()
    set(JS_INSTALL_EXCLUDE PATTERN "*.map" EXCLUDE PATTERN "asset-manifest.json" EXCLUDE)
endif()

# Copy everything to the build tree, since Jumblescript build systems have the mysterious property of not permitting
# out-of-tree builds.
file(GLOB_RECURSE JS_SRC RELATIVE "${CMAKE_CURRENT_LIST_DIR}" "*")
set(JS_COPY_DEPS)
foreach (F IN LISTS JS_SRC)
    # Exclude junk that might exist from an in-tree build.
    if (F MATCHES "(^|/)(.yarn|build|dist|node_modules)/")
        continue()
    endif()

    # Copy the file.
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${F}"
                       COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/${F}"
                                                        "${CMAKE_CURRENT_BINARY_DIR}/${F}"
                       MAIN_DEPENDENCY "${CMAKE_CURRENT_LIST_DIR}/${F}"
                       COMMENT "Copying ${F} for out-of-tree JS build.")

    # Add the copy to the list of dependencies, so if it changes, the JS targets below get re-run.
    list(APPEND JS_COPY_DEPS "${CMAKE_CURRENT_BINARY_DIR}/${F}")
endforeach()

# Install JS dependencies. This construction of add_custom_command and add_custom_target creates a target that is
# considered out of date when any of JS_COPY_DEPS changes, but still runs by default when not up to date.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_install"
                   COMMAND cmake -E touch "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_install"
                   COMMAND yarn install
                   DEPENDS "${JS_COPY_DEPS}"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                   COMMENT "Installing Javascript dependencies.")
add_custom_target(js_yarn_install ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_install")

# Build the projects.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_bundle"
                   COMMAND cmake -E touch "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_bundle"
                   COMMAND yarn "$<IF:$<BOOL:JS_DEV>,bundle-dev,bundle>"
                   DEPENDS js_yarn_install "${JS_COPY_DEPS}"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                   COMMENT "Building Javascript projects.")
add_custom_target(js_yarn_bundle ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_bundle")

# Install the things we just built.
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/client/dist/" DESTINATION ${JS_INSTALL_PREFIX}client
        ${JS_INSTALL_EXCLUDE})
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/demo-client/build/" DESTINATION ${JS_INSTALL_PREFIX}demo-client
        ${JS_INSTALL_EXCLUDE})
if (NOT XCMAKE_PACKAGING)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dev-client/dist/" DESTINATION ${JS_INSTALL_PREFIX}dev-client
            ${JS_INSTALL_EXCLUDE})
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/server/dist/" DESTINATION ${JS_INSTALL_PREFIX}js-server
            ${JS_INSTALL_EXCLUDE})
endif()

# Lint.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_foreach_lint"
                   COMMAND cmake -E touch "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_foreach_lint"
                   COMMAND yarn foreach lint
                   DEPENDS js_yarn_bundle "${JS_COPY_DEPS}"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                   COMMENT "Running Javascript linter.")
add_custom_target(js_yarn_foreach_lint ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_foreach_lint")

# Create the Javascript test script in the install directory. This really just serves to make it easy to find what to
# run. It depends on the build directory.
if (XCMAKE_ENABLE_TESTS)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_test"
         CONTENT "#!/bin/bash\ncd ${CMAKE_CURRENT_BINARY_DIR}\nyarn test")
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/js_test" DESTINATION test/bin)
endif()
