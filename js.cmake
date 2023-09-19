# The rest of this script is meaningful only if the Javascript build is turned on.
if (NOT ENABLE_JS)
    return()
endif()

# Figure out the install prefix.
set(JS_INSTALL_PREFIX "share/spectral-video-streamer/")

# Figure out what to exclude from the built Javascript stuff.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set(JS_INSTALL_EXCLUDE)
else()
    set(JS_INSTALL_EXCLUDE PATTERN "*.map" EXCLUDE PATTERN "asset-manifest.json" EXCLUDE)
endif()

# Copy everything to the build tree, since Jumblescript build systems have the mysterious property of not permitting
# out-of-tree builds. Annoyingly, some tools also expects things to be in the root directory, so we have to pick out the
# bits we want rather than isolating it all in its own directory.
file(GLOB_RECURSE JS_SRC RELATIVE "${CMAKE_CURRENT_LIST_DIR}" CONFIGURE_DEPENDS
     ".yarn/*" "client/*" "common/*" "demo-client/*" "dev-client/*" "settings-ui/*"
     ".yarnrc.yml" "cra-build-helper.sh" "eslintrc-template.js" "package.json" "tsconfig.json" "yarn.lock"
     "docs/build.sh" "docs/typedoc.js")
set(JS_COPY_DEPS)
foreach (F IN LISTS JS_SRC)
    # Exclude junk that might exist from an in-tree build, or is not JS-related.
    if (F MATCHES "(^|/)(build|dist|node_modules)/")
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
add_custom_target(js_copy ALL DEPENDS "${JS_COPY_DEPS}")

# Install JS dependencies. This construction of add_custom_command and add_custom_target creates a target that is
# considered out of date when any of JS_COPY_DEPS changes, but still runs by default when not up to date.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_install_file"
                   COMMAND yarn install
                   COMMAND cmake -E touch "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_install_file"
                   DEPENDS js_copy "${JS_COPY_DEPS}"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                   COMMENT "Installing Javascript dependencies.")
add_custom_target(js_yarn_install ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_install_file")

# Figure out the arguments to give to yarn for building the projects.
set(YARN_ARGS)
if (CLIENT_INFO_URL)
    list(APPEND YARN_ARGS --env "INFO_URL=${CLIENT_INFO_URL}")
endif()

# Build the projects.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_bundle_file"
                   COMMAND yarn "$<IF:$<BOOL:${JS_DEV}>,bundle-dev,bundle>" ${YARN_ARGS}
                   COMMAND cmake -E touch "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_bundle_file"
                   DEPENDS js_copy js_yarn_install "${JS_COPY_DEPS}"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                   COMMENT "Building Javascript projects.")
add_custom_target(js_yarn_bundle ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_bundle_file")

# Install the things we just built.
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/client/dist/" DESTINATION ${JS_INSTALL_PREFIX}client
        ${JS_INSTALL_EXCLUDE})
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/demo-client/build/" DESTINATION ${JS_INSTALL_PREFIX}demo-client
        ${JS_INSTALL_EXCLUDE})
if (ENABLE_SERVER)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/settings-ui/build/" DESTINATION ${JS_INSTALL_PREFIX}settings-ui
            ${JS_INSTALL_EXCLUDE})
endif()
if (NOT XCMAKE_PACKAGING)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dev-client/dist/" DESTINATION ${JS_INSTALL_PREFIX}dev-client
            ${JS_INSTALL_EXCLUDE})
endif()

# Lint.
#add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_foreach_lint"
#                   COMMAND cmake -E touch "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_foreach_lint"
#                   COMMAND yarn lint
#                   DEPENDS js_copy js_yarn_bundle "${JS_COPY_DEPS}"
#                   WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
#                   COMMENT "Running Javascript linter.")
#add_custom_target(js_yarn_foreach_lint ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_foreach_lint")

# Create the Javascript test script in the install directory. This really just serves to make it easy to find what to
# run. It depends on the build directory.
if (XCMAKE_ENABLE_TESTS)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_test"
         CONTENT "#!/bin/bash\ncd ${CMAKE_CURRENT_BINARY_DIR}\nyarn test")
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/js_test" DESTINATION test/bin)
endif()

# Generate documentation.
if (XCMAKE_ENABLE_DOCS)
    # Create a symlink to xcmake so the documentation script can find it.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
    file(CREATE_LINK "${CMAKE_SOURCE_DIR}/xcmake" "${CMAKE_CURRENT_BINARY_DIR}/docs/xcmake" SYMBOLIC)

    # Actually generate the documentaiton.
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_docs_file"
        COMMAND yarn docs
        COMMAND cmake -E touch "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_docs_file"
        DEPENDS js_copy js_yarn_bundle "${JS_COPY_DEPS}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generate documentation for the Javascript projects."
    )
    add_custom_target(js_yarn_docs ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/js_yarn_docs_file")

    # Install the customer documentation. Note that because we're merging with the global documentation, we need to
    # rename index.html.
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/dist/docs/" DESTINATION "docs" PATTERN "*.html.d" EXCLUDE)

    # Install the private documentation.
    #if (XCMAKE_PRIVATE_DOCS)
    #    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/dist/private_docs/live-video-streamer/private/"
    #            DESTINATION "private_docs/live-video-streamer" PATTERN "*.html.d" EXCLUDE)
    #endif()
endif()
