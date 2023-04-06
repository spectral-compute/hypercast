#pragma once

#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Server
{

/**
 * Represents the path to a resource.
 */
class Path final
{
public:
    /**
     * An exception that's thrown if the path is bad.
     */
    class Exception final : public std::runtime_error
    {
    public:
        explicit Exception(const char *msg) : std::runtime_error(msg) {}
    };

    ~Path();
    Path(const Path &);
    Path(Path &&) = default;
    Path &operator=(const Path &);
    Path &operator=(Path &&) = default;

    /**
     * Construct a path with this path first and rhs as the subpath.
     */
    Path operator/(const Path &rhs) const;

    /**
     * Constructor :)
     *
     * @param path A path whose parts are separated by the '/' character. The outer-most part appears first. The path is
     *             canonicalized so that empty parts and parts whose value is "." are removed. The path is not allowed
     *             to contain characters that might enable directory traversal or other path manipulation attacks.
     * @throws Exception If the path is invalid as described above.
     */
    Path(std::string_view path);
    template <typename T> Path(const T &path) : Path(std::string_view(path)) {}

    /**
     * Imposes an arbitrary total ordering.
     */
    std::strong_ordering operator<=>(const Path &) const noexcept;

    /**
     * Convert the path to a string.
     */
    operator std::string() const;

    /**
     * Convert the path to a filesystem path object.
     */
    operator std::filesystem::path() const;

    /**
     * Get the single part of the request.
     *
     * This call is valid only if there is exactly one part. There is a debug assertion to enforce this.
     */
    const std::string &operator*() const
    {
        assert(parts.size() == 1);
        return parts[0];
    }

    /**
     * Get the path part with the given index.
     *
     * @param index The index to get the path part for. The outer-most part has index 0 and the inner-most part has
     *              index size() - 1.
     */
    const std::string &operator[](size_t index) const
    {
        assert(index < parts.size());
        return parts[parts.size() - index - 1]; // The path is stored backwards to aid pop_front().
    }

    /**
     * Determine if the path is the empty path (i.e: no parts).
     */
    bool empty() const
    {
        return parts.empty();
    }

    /**
     * Get the number of parts in the path.
     */
    size_t size() const
    {
        return parts.size();
    }

    /**
     * Get the outer-most part of the path.
     */
    const std::string &front() const
    {
        return parts.back(); // The path is stored backwards to aid pop_front().
    }

    /**
     * Get the inner-most part of the path.
     */
    const std::string &back() const
    {
        return parts.front(); // The path is stored backwards to aid pop_front().
    }

    /**
     * Remove the outermost part.
     */
    void pop_front()
    {
        parts.pop_back(); // The path is stored backwards to aid this method.
    }

private:
    Path() = default;

    std::vector<std::string> parts;
};

} // namespace Server
