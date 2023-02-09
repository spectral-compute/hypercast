#pragma once

#include "Resource.hpp"

#include <string_view>
#include <vector>

namespace Server
{

/**
 * Like Resource, but with an operator() that is not a coroutine.
 */
class SynchronousResource : public Resource
{
public:
    ~SynchronousResource() override;
    using Resource::Resource;

    Awaitable<void> operator()(Response &response, Request &request) final;

    /**
     * Service a request for the resource.
     *
     * @param response The response stream to write to.
     * @param request The request to service. The path in the request should be relative to this resource. The request
     *                is const because the asynchronous callbacks of the request are not available to subclasses of this
     *                class. Instead, the request data is provided as an argument to this method.
     * @param requestData The request's body data.
     */
    virtual void operator()(Response &response, const Request &request, std::vector<std::byte> requestData) = 0;
};

/**
 * Like SynchronousResource, but the body is converted to a string.
 */
class SynchronousStringResource : public Resource
{
public:
    ~SynchronousStringResource() override;
    using Resource::Resource;

    Awaitable<void> operator()(Response &response, Request &request) final;

    /**
     * Service a request for the resource.
     *
     * @param response The response stream to write to.
     * @param request The request to service. The path in the request should be relative to this resource. The request
     *                is const because the asynchronous callbacks of the request are not available to subclasses of this
     *                class. Instead, the request data is provided as an argument to this method.
     * @param requestData The request's body data.
     */
    virtual void operator()(Response &response, const Request &request, std::string_view requestData) = 0;
};

/**
 * Like SynchronousResource, but checks that the request body data is empty.
 */
class SynchronousNullaryResource : public Resource
{
public:
    ~SynchronousNullaryResource() override;
    using Resource::Resource;

    Awaitable<void> operator()(Response &response, Request &request) final;

    /**
     * Service a request for the resource.
     *
     * @param response The response stream to write to.
     * @param request The request to service. The path in the request should be relative to this resource. The request
     *                is const because the asynchronous callbacks of the request are not available to subclasses of this
     *                class. Instead, resources of this type do not accept any request body data.
     */
    virtual void operator()(Response &response, const Request &request) = 0;
};

} //namespace Server
