#pragma once

#include "Path.hpp"

#include <vector>

template <typename> class Awaitable;

namespace Server
{

/**
 * A server request.
 *
 * The request stores information from headers, such as the path and request type. The request body data can be read
 * (or checked for emptiness) with the various read methods.
 *
 * This is subclassed by the implementation of the HTTP server to provide an implementation that's suitable for the
 * asynchronous networking stuff the HTTP server does.
 */
class Request
{
public:
    /**
     * Represents a request type.
     *
     * These types have the same meaning as their counterparts in HTTP.
     */
    enum class Type
    {
        /**
         * Equivalent to the HTTP GET request.
         *
         * This is also used for HEAD requests.
         */
        get,

        /**
         * Equivalent to the HTTP POST request.
         */
        post,

        /**
         * Equivalent to the HTTP PUT request.
         */
        put
    };

    virtual ~Request();
    explicit Request(Path path, Type type, bool isPublic) : path(std::move(path)), type(type), isPublic(isPublic) {}

    /**
     * Transform this request into a request from within its outer-most path part.
     *
     * I.e: if the request is for a/b/c, it'll be for b/c after this method has been called.
     */
    void popPathPart()
    {
        path.pop_front();
    }

    /**
     * Get the path for this request.
     */
    const Path &getPath() const
    {
        return path;
    }

    /**
     * Get the type of this request.
     *
     * This is useful to distinguish between, e.g: GET and PUT requests.
     */
    Type getType() const
    {
        return type;
    }

    /**
     * Determine if the request came from somewhere that's considered public.
     */
    bool getIsPublic() const
    {
        return isPublic;
    }

    /**
     * Read some data from the request body.
     *
     * @return The data that was read. This returns an empty result when the request body is finished.
     */
    virtual Awaitable<std::vector<std::byte>> readSome() = 0;

    /**
     * Read all the (remaining, if readSome() has already been called) body data.
     *
     * This keeps calling readSome() until it returns empty, as described above.
     *
     * @return The complete body data.
     */
    Awaitable<std::vector<std::byte>> readAll();

    /**
     * Like readAll, but returns a string.
     */
    Awaitable<std::string> readAllAsString();

    /**
     * Calls readAll() to ensure no more body data exists.
     *
     * @throws Error if any data remains.
     */
    Awaitable<void> readEmpty();

private:
    Path path;
    const Type type;
    const bool isPublic;
};

} // namespace server
