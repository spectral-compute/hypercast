#pragma once

#include <filesystem>
#include <string>
#include <vector>

/**
 * Read a file from the filesystem as a vector of lines.
 *
 * @param path The path of the file to read.
 * @return The file's contents as a vector of lines.
 */
std::vector<std::string> readFileAsLines(const std::filesystem::path &path, bool includeEmpty = true);

/**
 * Get an absolute path to the test data file with the given relative path.
 *
 * @param path The path relative to the installed test data directory. The layout of the installed test data directory
 *             is the same as that of the server test directory in the repository, except that the data directories are
 *             removed from the paths.
 * @return An absolute path to the data file.
 */
std::filesystem::path getTestDataPath(const std::filesystem::path &path = {});

/**
 * Get a path to the SMPTE test video with the given properties.
 *
 * @param width Video width.
 * @param height Video height
 * @param frameRateNumerator Frame rate numerator.
 * @param frameRateDenominator Frame rate denominator.
 * @param sampleRate Sample rate.
 * @return
 */
std::filesystem::path getSmpteDataPath(unsigned int width, unsigned int height,
                                       unsigned int frameRateNumerator, unsigned int frameRateDenominator,
                                       unsigned int sampleRate);
