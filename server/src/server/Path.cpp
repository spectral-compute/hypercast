#include "Path.hpp"

Server::Path::~Path() = default;
Server::Path::Path(const Path &) = default;
Server::Path &Server::Path::operator=(const Path &) = default;

Server::Path::Path(std::string_view path)
{
    /* Disallow characters that could pose vulnerabilities. */
    for (char c: path) {
        if (c < 0x20 || c > 0x7E) {
            // This restriction is a bit aggressive, but it does guard against non-canonical UTF-8 directory traversal
            // attacks.
            throw Exception("Path contains a character that is not printable ASCII.");
        }
        switch (c) {
            case '\\':
            case ':':
                throw Exception("Path contains bad character.");
        }
    }

    /* Parse the path from the end backwards, and build the parts buffer in the opposite direction. This ordering makes
       the pop_front() operation almost free. */
    while (!path.empty()) {
        // Find the next separation point.
        size_t sep = path.rfind('/');

        // Extract the part at the end after the separation point.
        std::string_view part = path.substr((sep == std::string::npos) ? 0 : (sep + 1));

        // Remove the last part of the path from the path.
        path = path.substr(0, (sep == std::string::npos) ? 0 : sep);

        // Filter out empty parts.
        if (part == "." || part.empty()) {
            continue; // The part is a no-op.
        }

        // Disallow parent directory accesses.
        if (part.find_first_not_of('.') == std::string::npos) {
            throw Exception("Path not allowed to contain parent dots.");
        }

        // Add the part to the list of parts.
        parts.emplace_back(part);
    }
}

std::strong_ordering Server::Path::operator<=>(const Path &) const noexcept = default;

Server::Path Server::Path::operator/(const Path &rhs) const
{
    Server::Path result;
    result.parts.reserve(parts.size() + rhs.parts.size());
    result.parts.insert(result.parts.end(), rhs.parts.begin(), rhs.parts.end());
    result.parts.insert(result.parts.end(), parts.begin(), parts.end());
    return result;
}

Server::Path::operator std::string() const
{
    std::string result;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        result += *it + "/";
    }
    if (!result.empty()) {
        result.resize(result.size() - 1); // Remove the trailing /.
    }
    return result;
}

Server::Path::operator std::filesystem::path() const
{
    std::filesystem::path result;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        result /= *it;
    }
    return result;
}
