#include "util.hpp"

#include <fstream>

std::vector<std::byte> Util::concatenate(std::vector<std::vector<std::byte>> dataParts)
{
    /* Optimization: Handle the empty case. */
    if (dataParts.empty()) {
        return {};
    }

    /* Optimization: Handle the case where there's only one part, and therefore it can just be moved. */
    if (dataParts.size() == 1) {
        return std::move(dataParts[0]);
    }

    /* Allocate space for a vector of the right size. */
    std::size_t totalSize = 0;
    for (const auto &part: dataParts) {
        totalSize += part.size();
    }

    std::vector<std::byte> allData;
    allData.reserve(totalSize);

    /* Concatenate the buffer. */
    for (auto &part: dataParts) {
        allData.insert(allData.end(), part.begin(), part.end());
        part.clear(); // Don't continue to waste memory :)
    }

    /* Done :) */
    return allData;
}

std::vector<std::byte> Util::readFile(const std::filesystem::path &path)
{
    std::ifstream f;
    f.exceptions(std::ios::badbit | std::ios::failbit);
    f.open(path, std::ios::ate | std::ios::binary);
    size_t size = f.tellg();

    std::vector<std::byte> result(size);
    f.seekg(0);
    f.read((char *)result.data(), size);

    return result;
}

std::string Util::replaceAll(std::string_view string, std::string_view token, std::string_view replacement)
{
    std::string result;
    while (true) {
        size_t pos = string.find(token);

        /* Token not found, so insert the entirety of the remainig string. */
        if (pos == std::string_view::npos) {
            result += string;
            return result;
        }

        /* Splice the replacement into the result along with the prefix. */
        result += string.substr(0, pos);
        result += replacement;
        string = string.substr(pos + token.size());
    }
}
