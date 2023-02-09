#include "DashResources.hpp"

#include "configuration/configuration.hpp"
#include "log/Log.hpp"
#include "resources/ConstantResource.hpp"
#include "server/Path.hpp"
#include "server/Server.hpp"
#include "util/asio.hpp"
#include "util/util.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>

/// @addtogroup dash
/// @{
/// @defgroup dash_implementation Implementation
/// @}

/// @addtogroup dash_implementation
/// @{

namespace
{

/**
 * Generate a unique ID.
 *
 * This is useful for URLs that might otherwise conflict with stale versions in a cache.
 */
std::string getUid()
{
    constexpr const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    constexpr int alphabetSize = sizeof(alphabet) - 1; // The -1 removes the null terminator.

    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>
                  (std::chrono::system_clock::now().time_since_epoch()).count();

    std::string result;
    while (ms > 0) {
        result += alphabet[ms % alphabetSize];
        ms /= alphabetSize;
    }
    return result;
}

} // namespace

/// @}

namespace Dash
{

/**
 * Generate an info.json.
 *
 * @param config The configuration object.
 * @param basePath The base path (with unique ID substituted) to the live streaming resources.
 */
std::string getLiveInfo(const Config::Root &config, const Server::Path &basePath);

} // namespace Dash

Dash::DashResources::~DashResources() = default;

Dash::DashResources::DashResources(IOContext &ioc, Log::Log &log, const Config::Root &config, Server::Server &server) :
    ioc(ioc), log(log), logContext(log("dash")), dashConfig(config.dash), server(server)
{
    /* Generate a base path. */
    Server::Path basePath = Util::replaceAll(config.paths.liveStream, "{uid}", getUid());

    /* Add resources. */
    server.addResource<Server::ConstantResource>(config.paths.liveInfo, getLiveInfo(config, basePath),
                                                 "application/json", Server::CacheKind::ephemeral, true);

    // For now, each video quality has a single corresponding audio quality.
    for (unsigned int i = 0; i < config.qualities.size(); i++) {
        createSegment(i, 0);
        if (config.qualities[i].audio) {
            createSegment(i + (unsigned int)config.qualities.size(), 0);
        }
    }
}

void Dash::DashResources::notifySegmentStart(unsigned int streamIndex, unsigned int segmentIndex)
{
    boost::asio::co_spawn((boost::asio::io_context &)ioc, [=, this]() -> boost::asio::awaitable<void> {
        try {
            /* Wait for the segment to become pre-available. */
            boost::asio::steady_timer timer(ioc);
            timer.expires_from_now(std::chrono::milliseconds(dashConfig.segmentDuration -
                                                             dashConfig.preAvailabilityTime));
            co_await timer.async_wait(boost::asio::use_awaitable);

            /* Create the segment. */
            createSegment(streamIndex, segmentIndex);
        }
        catch (const std::exception &e) {
            logContext << Log::Level::error
                       << "Exception while creating pre-available segment " << segmentIndex
                       << " for stream " << streamIndex << ": " << e.what() << ".";
        }
        catch (...) {
            logContext << Log::Level::error
                       << "Unknown exception while creating pre-available segment " << segmentIndex
                       << " for stream " << streamIndex << ".";
        }
    }, boost::asio::detached);
}

void Dash::DashResources::createSegment([[maybe_unused]] unsigned int streamIndex,
                                        [[maybe_unused]] unsigned int segmentIndex)
{
    /* Create a new interleave segment if the one we need doesn't exist already. */

    /* Add the new segment. */

    /* Set the caching for the following segments (up to however many could be reached with fixed caching) to
       ephemeral. */
}
