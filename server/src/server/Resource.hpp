#pragma once

template <typename> class Awaitable;
class IOContext;

namespace Server
{

class Request;
class Response;

/**
 * An HTTP resource that can be registered with a server.
 */
class Resource
{
public:
    virtual ~Resource();

    /**
     * Constructor :)
     *
     * @param isPublic Whether or not the resource is to be accessible publicly. @see getIsPublic.
     */
    explicit Resource(bool isPublic = false) : isPublic(isPublic) {}

    /**
     * Service a request for the resource.
     *
     * @param response The response stream to (set up to) write to.
     * @param request The request to service. The path in the request should be relative to this resource. This method
     *                should register at least one of the callbacks of the request if it needs its body.
     */
    virtual Awaitable<void> operator()(Response &response, Request &request) = 0;

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

    /**
     * Determine whether this resource can service a request of type Request::Type::get (e.g: an HTTP GET request).
     *
     * If this returns false, then operator() will not be called for requests of type Request::Type::get.
     *
     * The default is false.
     */
    virtual bool getAllowGet() const noexcept;

    /**
     * Like getAllowGet(), for Request::Type::post.
     */
    virtual bool getAllowPost() const noexcept;

    /**
     * Like getAllowPut(), for Request::Type::put.
     */
    virtual bool getAllowPut() const noexcept;

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
