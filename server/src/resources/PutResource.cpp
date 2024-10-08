#include "PutResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/debug.hpp"
#include "util/File.hpp"
#include "util/util.hpp"

Server::PutResource::~PutResource() = default;

Awaitable<void> Server::PutResource::getAsync(Response &response, Request &)
{
    if (!hasBeenPut) {
        response.setCacheKind(CacheKind::ephemeral); // Presumably, this is expected to appear in the next few seconds.
        throw Error(ErrorKind::NotFound, "PUT resource was GET'd before being PUT");
    }
    response.setCacheKind(cacheKind);
    response << data;
    co_return;
}

Awaitable<void> Server::PutResource::putAsync(Response &response, Request &request)
{
    assert(!request.getIsPublic());

    /* Read input from the request, and possibly write it to a file (that gets opened here). */
    std::vector<std::vector<std::byte>> dataParts;

    // The scope makes the file close as early as possible.
    {
        // Open the file to write to.
        Util::File file;
        if (!path.empty()) {
            file = Util::File(*ioc, path, true, false);
        }

        // Read input from the request.
        while (true) {
            // Read some data from the request body.
            std::vector<std::byte> dataPart = co_await request.readSome();
            if (dataPart.empty()) {
                break;
            }

            // Write the data to the file if it exists.
            if (file) {
                co_await file.write(dataPart);
            }

            // Save the data to what we're going to concatenate.
            dataParts.emplace_back(std::move(dataPart));
        }
    }

    /* Save the data we read. */
    data = Util::concatenate(dataParts);
    hasBeenPut = true;
}

size_t Server::PutResource::getMaxPutRequestLength() const noexcept
{
    return maxRequestLength;
}
