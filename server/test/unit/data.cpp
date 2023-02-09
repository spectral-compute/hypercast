#include "data.hpp"

#include "util/util.hpp"

extern std::filesystem::path testDir;

std::vector<std::string> readFileAsLines(const std::filesystem::path &path, bool includeEmpty)
{
    /* Read the data as a string view. */
    std::vector<std::byte> data = Util::readFile(path);
    std::string_view dataString((const char *)data.data(), data.size());

    /* Split the string into lines. */
    std::vector<std::string> lines;
    while (!dataString.empty()) {
        // Find the next newline.
        size_t separator = dataString.find('\n');

        // Extract the single line.
        std::string_view line = dataString.substr(0, separator);

        // Add it to the list if it's non-empty or we're keeping empty lines.
        if (includeEmpty || !line.empty()) {
            lines.emplace_back(line);
        }

        // Move past the newline.
        dataString = dataString.substr((separator == std::string::npos) ? std::string::npos : (separator + 1));
    }
    return lines;
}

std::filesystem::path getTestDataPath(const std::filesystem::path &path)
{
    return testDir / "data" / path;
}

std::filesystem::path getSmpteDataPath(unsigned int width, unsigned int height,
                                       unsigned int frameRateNumerator, unsigned int frameRateDenominator,
                                       unsigned int sampleRate)
{
    return getTestDataPath(std::filesystem::path("smpte") / (
        // Video codec.
        "h264-" +

        // Resolution.
        std::to_string(width) + "x" + std::to_string(height) + "-" +

        // Frame rate.
        std::to_string(frameRateNumerator) +
        ((frameRateDenominator == 1) ? "" : ("_" + std::to_string(frameRateDenominator))) +

        // Audio codec.
        " aac-" +

        // Sample rate.
        std::to_string(sampleRate) +

        // Container.
        ".mkv"
    ));
}
