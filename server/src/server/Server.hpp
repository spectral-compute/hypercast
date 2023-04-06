#pragma once

#include "Resource.hpp"

#include "log/Log.hpp"

#include <memory>
#include <set>
#include <string>

/**
 * @defgroup server Server
 *
 * Stuff for implementing a generic server.
 */
/// @addtogroup server
/// @{

/**
 * Contains the stuff for implementing a generic server.
 *
 * Stuff for non-generic resources, such as the DASH-like stuff or APIs, exist elsewhere but use the stuff in this
 * namespace.
 */
namespace Server
{

class Path;
class Request;
class Response;

/**
 * A server that manages resources, requests for those resources, and responses to those requests.
 *
 * This is subclassed by the implementation of the HTTP server to provide an implementation that's suitable for the
 * asynchronous networking stuff the HTTP server does. The idea is to separate out the generic stuff like finding the
 * right resource and enforcing resource restrictions from the HTTP/networking specific stuff.
 */
class Server
{
public:
    virtual ~Server();

    /**
     * Add a resource to the server.
     *
     * @tparam ResourceType The type of resource to add. Must be a subclass of Resource.
     * @param path The path to add the resource to. This must not point to a resource that already exists and must not
     *             point to a child of a resource.
     * @param args The arguments to forward to the resource's constructor.
     * @return A shared pointer to the newly created resource. The server also keeps ownership of the resource until
     *         it's removed or replaced.
     * @throws std::runtime_error if the conditions on path are not met.
     */
    template <typename ResourceType, typename... Args>
    std::shared_ptr<ResourceType> addResource(const Path &path, Args &&...args)
    {
        return addOrReplaceResource<ResourceType>(false, path, std::forward<Args>(args)...);
    }

    /**
     * Add a resource to the server, replacing any that already exists.
     *
     * @tparam ResourceType The type of resource to add. Must be a subclass of Resource.
     * @param path The path to add the resource to. This must not point to an intermediate path in the resource tree and
     *             must not point to a child of a resource.
     * @param args The arguments to forward to the resource's constructor.
     * @return A shared pointer to the newly created resource.
     * @throws std::runtime_error if the conditions on path are not met.
     */
    template <typename ResourceType, typename... Args>
    std::shared_ptr<ResourceType> addOrReplaceResource(const Path &path, Args &&...args)
    {
        return addOrReplaceResource<ResourceType>(true, path, std::forward<Args>(args)...);
    }

    /**
     * Remove a resource.
     *
     * @param path The path of the resource to remove. This must point to a resource (not an intermediate node in the
     *             tree) that exists.
     * @throws std::runtime_error if the conditions on path are not met.
     */
    void removeResource(const Path &path)
    {
        removeResourceOrTree(path, false);
    }

    /**
     * Remove a tree of resources.
     *
     * This is useful where a resource creates a tree of resources, but either doesn't track their paths, or it would be
     * unnecessarily verbose to write the code to generate those paths just for their deletion.
     *
     * @param path The base path of the resources to remove. Every resource that starts with this path will be removed.
     */
    void removeResourceTree(const Path &path)
    {
        removeResourceOrTree(path, true);
    }

    /**
     * Add a path to use ephemeral caching for the Not Found responses.
     *
     * This is useful for things like info.json, which might be cretaed at short notice
     */
    void addEphemeralWhenNotFound(Path path);

protected:
    explicit Server(Log::Log &log);

    /**
     * Handle a request.
     *
     * If an exception is thrown during the request, this method handles it. Exceptions from Resource objects therefore
     * do not propagate past this method.
     *
     * @param response The response object for the request.
     * @param request The request object. The path for this is manipulated so that when it's passed to the Resource, the
     *                path is relative to that resource.
     */
    Awaitable<void> operator()(Response &response, Request &request) const;

    /**
     * The log to write to.
     */
    Log::Log &log;

    /**
     * Where to log things that come out of the generic server.
     */
    Log::Context logContext;

private:
    /**
     * Add a resource to the server, replacing any that already exists.
     *
     * @tparam ResourceType The type of resource to add. Must be a subclass of Resource.
     * @param replace Whether replacement is permitted.
     * @param path The path to add the resource to. This must not point to an intermediate path in the resource tree and
     *             must not point to a child of a resource.
     * @param args The arguments to forward to the resource's constructor.
     * @return A reference to the newly created resource.
     */
    template <typename ResourceType, typename... Args>
    std::shared_ptr<ResourceType> addOrReplaceResource(bool replace, const Path &path, Args &&...args)
    {
        /* Get the node that we're going to insert into, and check it's not a tree. */
        std::shared_ptr<Resource> &node = getOrCreateLeafNode(path, replace);

        /* Create the resource. */
        auto resource = std::make_shared<ResourceType>(std::forward<Args>(args)...); // Create the resource.

        /* Log. */
        logResourceChange(path, true, (bool)node);

        /* Insert it into the tree and return a reference to it. */
        node = resource;
        return resource;
    }

    /**
     * Create a leaf node (i.e: smart pointer to a resource) in the resources tree.
     *
     * @param path The path to the node to find. This must not point to an intermediate node in the tree and
     *             must not point to a child of a resource (other than a tree resource, which implements an intermediate
     *             node).
     * @param existing Permit returning an existing node.
     * @return A pointer to the node. Or nullptr if it doesn't exist and isn't allowed to be created.
     */
    std::shared_ptr<Resource> &getOrCreateLeafNode(const Path &path, bool existing);

    /**
     * Remove a resource or resource tree.
     *
     * @param path The path to remove.
     * @param allowTreeRemoval Whether to allow the removal of a whole resource tree, i.e intermediate node (true), or
     *                         to only allow the removal of a single resource (leaf node).
     */
    void removeResourceOrTree(const Path &path, bool allowTreeRemoval);

    /**
     * Log that a resource was changed.
     *
     * @param path The path to which the operation applies.
     * @param added Whether a resource was added.
     * @param removed Whether a resource was removed.
     */
    void logResourceChange(const Path &path, bool added, bool removed);

    /**
     * The root node in the resource tree.
     *
     * Resources are shared pointers because a resource might be removed from the tree while it's handling a request. In
     * order that such a request can complete successfully, operator() keeps a shared pointer to the Resource it's
     * using.
     */
    std::shared_ptr<Resource> root;

    /**
     * Paths that return ephemeral rather than fixed Not Found errors if non-existent.
     */
    std::set<Path> ephemeralWhenNotFound;
};

} // namespace Server

/// @}
