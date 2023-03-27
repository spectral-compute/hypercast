#pragma once

#include <string>

template <typename> class Awaitable;
class IOContext;

namespace Server
{

class Request;
class Response;

/**
 * A resource that can be registered with a server.
 *
 * Think of it like an HTTP resource. If the first line of a request is `GET /squiggle HTTP/1.0`, then this object is
 * the thing that handles requests to `/squiggle`.
 */
class Resource
{
protected:
    // Throw an error describing that the given HTTP verb is not supported.
    [[noreturn]] void unsupportedHttpVerb(const std::string& verb) const;

public:
    virtual ~Resource();

    /**
     * @param isPublic Whether or not the resource is to be accessible publicly. @see getIsPublic.
     */
    explicit Resource(bool isPublic = false) : isPublic(isPublic) {}

    /**
     * Service a request for the resource. Override the one(s) for the HTTP verbs you want to support.
     *
     * @param response The response stream to (set up to) write to.
     * @param request The request to service. The path in the request should be relative to this resource. This method
     *                should register at least one of the callbacks of the request if it needs its body.
     */
    virtual Awaitable<void> getAsync(Response &response, Request &request);
    virtual Awaitable<void> postAsync(Response &response, Request &request);
    virtual Awaitable<void> putAsync(Response &response, Request &request);

    /// Dispatches a request to one of the above.
    /// You probably don't want to override this one, but can if you want.
    virtual Awaitable<void> operator()(Response &response, Request &request);

    /// The maximum number of bytes in the request body. Default is zero.
    virtual size_t getMaxRequestLength() const noexcept;

    /**
     * Determine whether this resource can be serviced from a public location.
     *
     * If this returns false, then operator() will not be called unless the request originated from a location that is
     * considered private, such as localhost.
     *
     * The default is for resources to be private (so this returns false).
     */
    bool getIsPublic() const
    {
        return isPublic;
    }

    /**
     * Determine whether this resource can respond to requests with a non-empty path.
     *
     * If this returns false, then operator() will not be called unless the path in the request is empty.
     *
     * The default is false.
     */
    virtual bool getAllowNonEmptyPath() const noexcept;

private:
    /**
     * Whether the resource can be public.
     *
     * This is different from the capabilities getters above because this is about permissions rather than what the
     * resource type is fundamentally capable of doing.
     */
    const bool isPublic = false;
};

} // namespace Server
