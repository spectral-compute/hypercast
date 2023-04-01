#pragma once

#include "Resource.hpp"

#include <string_view>
#include <vector>

namespace Server
{

/**
 * Like Resource, but with request handlers that are not coroutines.
 *
 * Nevertheless, does not block the event loop.
 */
class SynchronousResource : public Resource
{
public:
    ~SynchronousResource() override;
    using Resource::Resource;

protected:
    // The request body.
    std::vector<std::byte> requestData;

    virtual Awaitable<std::vector<std::byte>> extractData(Request& request);

public:
    Awaitable<void> operator()(Response &response, Request &request) override;

    /**
     * Service a request for the resource.
     *
     * @param response The response stream to write to.
     * @param request The request to service. The path in the request should be relative to this resource. The request
     *                is const because the asynchronous callbacks of the request are not available to subclasses of this
     *                class. Instead, the request data is provided as an argument to this method.
     */
    virtual void getSync(Response &response, const Request &request);
    virtual void postSync(Response &response, const Request &request);
    virtual void putSync(Response &response, const Request &request);
    virtual void optionsSync(Response &response, const Request &request);
};

/**
 * Like SynchronousResource, but checks that the request body data is empty.
 */
class SynchronousNullaryResource : public SynchronousResource
{
public:
    ~SynchronousNullaryResource() override;
    using SynchronousResource::SynchronousResource;

    Awaitable<std::vector<std::byte>> extractData(Request &request) override;
};

} //namespace Server
