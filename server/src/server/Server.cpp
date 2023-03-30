#include "Server.hpp"

#include "Request.hpp"
#include "Response.hpp"

#include "log/Log.hpp"
#include "util/asio.hpp"
#include "util/debug.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>

/// @addtogroup server
/// @{
/// @defgroup server_implementation Implementation
/// @}

/// @addtogroup server_implementation
/// @{

namespace
{

/**
 * Get a human readable string for a response error kind.
 */
const char *getErrorKindString(Server::ErrorKind kind)
{
    switch (kind) {
        case Server::ErrorKind::BadRequest: return "Bad request";
        case Server::ErrorKind::Forbidden: return "Forbidden";
        case Server::ErrorKind::NotFound: return "Not found";
        case Server::ErrorKind::UnsupportedType: return "Unsupported request type";
        case Server::ErrorKind::Internal: return "Internal";
    }
    return "Unknown";
}

size_t getMaxRequestLength(const Server::Resource &resource, Server::Request::Type type)
{
    switch (type) {
        case Server::Request::Type::get: return resource.getMaxGetRequestLength();
        case Server::Request::Type::post: return resource.getMaxPostRequestLength();
        case Server::Request::Type::put: return resource.getMaxPutRequestLength();
    }
    unreachable();
}

/**
 * Checks that a resource is not excluded from handling a given request.
 *
 * @param resource The resource that would handle the request.
 * @param request The request that is to be handled.
 */
void checkResourceRestrictions(const Server::Resource &resource, Server::Request &request)
{
    /* Don't allow public access to non-public resources. */
    if (!resource.getIsPublic() && request.getIsPublic()) {
        throw Server::Error(Server::ErrorKind::Forbidden);
    }

    /* Make sure the resource type can handle the request path. */
    if (!resource.getAllowNonEmptyPath() && !request.getPath().empty()) {
        throw Server::Error(Server::ErrorKind::NotFound);
    }

    /* The only request type that is OK for public requesters is GET. */
    if (request.getType() != Server::Request::Type::get && request.getIsPublic()) {
        throw Server::Error(Server::ErrorKind::Forbidden);
    }

    /* Enforce the maximum request length. */
    request.setMaxLength(getMaxRequestLength(resource, request.getType()));
}

/**
 * A resource that has children.
 *
 * Resources of this type form a tree in which other resources can be looked up.
 */
class TreeResource final : public Server::Resource
{
public:
    ~TreeResource() override = default;
    TreeResource() : Server::Resource(true) {} // This needs to be public to allow sub-resources to be public.

    /**
     * Recursively traverse the request's path until we get to the resource to request. Then pass the request to that
     * resource.
     *
     * This throws the appropriate exception if .
     */
    Awaitable<void> operator()(Server::Response &response, Server::Request &request) override
    {
        /* Don't allow directory listing of resources. */
        if (request.getPath().empty()) {
            throw Server::Error(Server::ErrorKind::Forbidden);
        }

        /* Find the resource referred to. */
        auto it = children.find(request.getPath().front());
        if (it == children.end()) {
            throw Server::Error(Server::ErrorKind::NotFound);
        }

        // This can keep TreeResources after they're deleted from the tree, but that's OK. It also keeps non-tree
        // resources after they're deleted. The purpose of this is to make sure deletions don't break requests that are
        // in progress.
        std::shared_ptr<Resource> child = it->second; // Keep ownership.
        assert(child); // If the child doesn't exist, it should have been deleted from the tree entirely.
        Resource &resource = *child;

        /* Pop the outer-most path component and try to call the resource.  */
        request.popPathPart();
        checkResourceRestrictions(resource, request);
        co_await resource(response, request); // Wait for the resource to finish so the shared pointer keeps it alive.
    }

    /**
     * Find (and possibly create as null) a resource.
     *
     * @param child The name of the child to find.
     * @return A reference to the smart pointer for the resource.
     */
    std::shared_ptr<Server::Resource> &operator[](std::string_view child)
    {
        return children[std::string(child)];
    }

    /**
     * Find a resource.
     *
     * @param child The name of the child to find.
     * @return A pointer to the smart pointer for the resource. Nullptr if the resource does not exist.
     */
    std::shared_ptr<Server::Resource> *at(std::string_view child)
    {
        auto it = children.find(std::string(child));
        return (it == children.end()) ? nullptr : &it->second;
    }

    bool getAllowNonEmptyPath() const noexcept override
    {
        return true;
    }

    /**
     * Determine if this resource has any children.
     */
    bool empty() const
    {
        return children.empty();
    }

    /**
     * Erase the given child.
     */
    void erase(std::string_view child)
    {
        children.erase(std::string(child));
    }

    /**
     * Remove null resources and empty trees.
     */
    void prune()
    {
        for (auto it = children.begin(); it != children.end();) {
            if (auto *tree = dynamic_cast<TreeResource *>(it->second.get())) {
                tree->prune();
                if (tree->empty()) {
                    children.erase(it++);
                    continue;
                }
            }
            ++it;
        }
    }

private:
    std::map<std::string, std::shared_ptr<Server::Resource>> children;
};

const char *getRequestTypeString(Server::Request::Type type)
{
    switch (type) {
        case Server::Request::Type::get: return "get";
        case Server::Request::Type::post: return "post";
        case Server::Request::Type::put: return "put";
    }
    unreachable();
}

} // namespace

/// @}

Server::Server::~Server() = default;

Awaitable<void> Server::Server::operator()(Response &response, Request &request) const
{
    Log::Context requestLog = log("request");
    requestLog << "what" << Log::Level::info << (std::string)request.getPath() << ", "
               << (request.getIsPublic() ? "public" : "private") << ", "
               << getRequestTypeString(request.getType());

    bool waitForResponse = false; // Call response.wait after the try/catch block, and then return.
    const char *what = nullptr; // Somewhere to put the internal error message if we can get one.

    /* Try to handle the request. */
    try {
        /* Grab the root resource (whatever it is, but it's probably a TreeResource), and make sure it exists. */
        std::shared_ptr<Resource> currentRoot = root; // Keep ownership.
        if (!currentRoot) {
            throw Error(ErrorKind::NotFound);
        }
        Resource &resource = *currentRoot;

        /* We kept a shared ownership to the root resource above, so now we can just call it. This will probably call
           TreeResource::operator(). The root gets the entire path. */
        checkResourceRestrictions(resource, request);
        co_await resource(response, request);

        /* Wait for the response to be written to the network, and return. */
        co_await response.flush(true);
        co_return;
    }

    /* Handle response exception objects. */
    catch (const Error &e) {
        if (response.getWriteStarted()) {
            // Print that something threw an Error that cannot be returned to the client. Really, this should be another
            // exception at this point.
            requestLog << Log::Level::error << getErrorKindString(e.kind) << "response error after writing started"
                       << (e.message.empty() ? "." : ": ") << e.message;
            co_return;
        }
        else {
            // Log the error.
            requestLog << "error" << Log::Level::info << getErrorKindString(e.kind)
                       << (e.message.empty() ? "" : ": ") << e.message;

            // Return the error to the client.
            response.setErrorAndMessage(e.kind, e.message);
            waitForResponse = true;
        }
    }

    /* Handle exceptions that are likely unexpected. These are always internal server errors. */
    catch (const std::exception &e) {
        what = e.what(); // We at least have *some* description of what the error is.
    }
    catch (...) {}

    /* Handle the case where we did actually handle the error, but need to co_await. */
    if (waitForResponse) {
        co_await response.flush(true); // Annoyingly, this can't be done in the catch block.
        co_return;
    }

    /* All but the first exception handler rely on not returning to actually handle the error with the response. */
    // Write an error somewhere, because we didn't actually handle it any other way than closing the stream and maybe
    // returning a generic error to the client.
    requestLog << Log::Level::error << (what ? "Error" : "Unknown error")
               << (response.getWriteStarted() ? " after writing started" : "")
               << (what ? ": " : "") << (what ? what : "");

    // Return an error code if possible.
    if (!response.getWriteStarted()) {
        response.setErrorAndMessage(ErrorKind::Internal);
        co_await response.flush(true);
    }
}

std::shared_ptr<Server::Resource> &Server::Server::getOrCreateLeafNode(const Path &path, bool existing)
{
    /* Tree traversal. */
    std::shared_ptr<Resource> *node = &root;
    for (size_t i = 0; i < path.size(); ++i) {
        assert(node); // The node variable is always created from a reference.

        // Create a tree node if the node is null.
        if (!*node) {
            *node = std::make_shared<TreeResource>();
        }

        // Make sure we're not trying to create the child of a leaf node.
        auto *tree = dynamic_cast<TreeResource *>(node->get());
        if (!tree) {
            if (auto *rootTree = dynamic_cast<TreeResource *>(root.get())) {
                // This is expensive in the exceptional case, but this means we don't need to track the history of our
                // traversal in the non-exceptional case to be able to find where we might have created tree nodes.
                rootTree->prune();
            }
            throw std::runtime_error("Cannot get/create child \"" + (std::string)path + "\" of server resource.");
        }

        // Get/create the next node.
        node = &(*tree)[path[i]];
    }
    assert(node);

    /* Check that the node is what we expect. */
    // None of the nodes we return from this should be a TreeResource.
    if (dynamic_cast<TreeResource *>(node->get())) {
        // This cannot happen by the node creation operation above.
        throw std::runtime_error("Path \"" + (std::string)path + "\" points to intermediate server tree node.");
    }

    // Check that the node is not an existing node if we're not allowed it.
    if (!existing && *node) {
        // This cannot happen by the node creation operation above.
        throw std::runtime_error("Path \"" + (std::string)path + "\" points to existing server resource.");
    }

    /* Done :) */
    return *node;
}

void Server::Server::logResourceChange(const Path &path, bool added, bool removed)
{
    assert(added || removed);
    const char *type = "nop";
    if (added && removed) {
        type = "replaced";
    }
    else if (added) {
        type = "added";
    }
    else if (removed) {
        type = "removed";
    }
    logContext << type << Log::Level::info << (std::string)path;
}

void Server::Server::removeResource(const Path &path)
{
    /* Tree traversal to find the tree node path to the leaf to remove. */
    std::vector<TreeResource *> intermediateNodes;
    intermediateNodes.reserve(path.size());

    std::shared_ptr<Resource> *node = &root;
    for (size_t i = 0; i < path.size(); ++i) {
        assert(node); // The node variable is always created from a reference, except where it's checked for nullness.

        // Make sure we're not erasing a non-existent node.
        if (!*node) {
            throw std::runtime_error("Cannot erase non-existent server resource \"" + (std::string)path + "\".");
        }

        // Make sure we're not trying to erase the child of a leaf node.
        auto *tree = dynamic_cast<TreeResource *>(node->get());
        if (!tree) {
            throw std::runtime_error("Cannot erase child \"" + (std::string)path + "\" of leaf server tree node.");
        }

        // Save this node in the list of intermediates and get the next one.
        intermediateNodes.push_back(tree);

        node = (*tree).at(path[i]);
        if (!node) {
            throw std::runtime_error("Cannot remove non-existing server tree node \"" + (std::string)path + "\".");
        }
    }
    assert(node);
    assert(intermediateNodes.size() == path.size()); // Number of lookups, and therefore number of intermediate nodes.

    /* Check that we're not trying to remove an intermediate node. */
    if (dynamic_cast<TreeResource *>(node->get())) {
        throw std::runtime_error("Cannot remove intermediate server tree node \"" + (std::string)path + "\".");
    }

    /* Once we get here, we know we're doing the removal. Log it. */
    logResourceChange(path, false, true);

    /* Iterate backwards, removing the resource and any consequently empty trees. */
    for (size_t j = 0; j < path.size(); ++j) {
        size_t i = path.size() - j - 1;

        assert(intermediateNodes[i]);
        assert(j == 0 || intermediateNodes[i + 1]->empty());

        intermediateNodes[i]->erase(path[i]);
        if (!intermediateNodes[i]->empty()) {
            return; // This node is not empty, so neither will any of its parents be.
        }
    }

    // The root node is now empty.
    assert(intermediateNodes[0] == root.get());
    assert(intermediateNodes[0]->empty());
    root.reset();
}
