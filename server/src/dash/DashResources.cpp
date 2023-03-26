#include "DashResources.hpp"

#include "configuration/configuration.hpp"
#include "dash/SegmentResource.hpp"
#include "dash/InterleaveResource.hpp"
#include "dash/SegmentIndexDescriptorResource.hpp"
#include "log/Log.hpp"
#include "resources/ConstantResource.hpp"
#include "resources/ErrorResource.hpp"
#include "resources/PutResource.hpp"
#include "server/Server.hpp"
#include "util/asio.hpp"
#include "util/json.hpp"
#include "util/util.hpp"

#include <boost/asio/steady_timer.hpp>

#include <algorithm>
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

/**
 * Get the name of a segment file.
 *
 * @param streamIndex The interleave stream index.
 * @param segmentIndex The segment index within the interleave.
 * @return The name of the segment file (without directory).
 */
std::string getSegmentName(unsigned int streamIndex, unsigned int segmentIndex)
{
    char name[64];
    snprintf(name, sizeof(name), "chunk-stream%u-%0.09u.m4s", streamIndex, segmentIndex);
    return name;
}

/**
 * Get the name of a segment file.
 *
 * @param streamIndex The interleave stream index.
 * @return The name of the initializer segment file (without directory).
 */
std::string getInitializerName(unsigned int streamIndex)
{
    char name[64];
    snprintf(name, sizeof(name), "init-stream%u.m4s", streamIndex);
    return name;
}

/**
 * Get the name of a segment index descriptor file.
 *
 * @param streamIndex The interleave stream index.
 * @return The name of the segment index descriptor file (without directory).
 */
std::string getSegmentIndexDescriptorName(unsigned int streamIndex)
{
    char name[64];
    snprintf(name, sizeof(name), "chunk-stream%u-index.json", streamIndex);
    return name;
}

/**
 * Get the name of an interleave file.
 *
 * @param interleaveIndex The interleave stream index.
 * @param segmentIndex The segment index within the interleave.
 * @return The name of the interleave file (without directory).
 */
std::string getInterleaveName(unsigned int interleaveIndex, unsigned int segmentIndex)
{
    char name[64];
    snprintf(name, sizeof(name), "interleaved%u-%0.09u", interleaveIndex, segmentIndex);
    return name;
}

/**
 * A resource that expires after a given amount of time.
 */
class ExpiringResource
{
public:
    /**
     * Remove the resource from the server.
     */
    ~ExpiringResource()
    {
        if (path.empty()) {
            return; // Presumably, we moved out of this object.
        }
        server.removeResource(path);
    }

    // Only move construction.
    ExpiringResource(const ExpiringResource &) = delete;
    ExpiringResource &operator=(ExpiringResource &&) = delete;
    ExpiringResource &operator=(const ExpiringResource &) = delete;

    /**
     * Determine if the interleave has expired.
     */
    bool expired(std::chrono::steady_clock::time_point now) const
    {
        return now > expiry;
    }

protected:
    /**
     * Set up to remove a resource from the server after a given expiry.
     *
     * @param server The server to remove from upon destruction.
     * @param path The path where the resource is to be created. The subclass is expected to create this resource.
     * @param lifetimeMs The lifetime, in ms, of the resource.
     * @param delayExpiry Whether to delay the expiry until after updateExpiry is called.
     */
    explicit ExpiringResource(Server::Server &server, Server::Path path, unsigned int lifetimeMs,
                              bool delayExpiry = false) :
        server(server), path(std::move(path)), lifetimeMs(lifetimeMs)
    {
        if (!delayExpiry) {
            updateExpiry();
        }
    }

    ExpiringResource(ExpiringResource &&) = default;

    /**
     * The path to the resource.
     */
    const Server::Path &getPath() const
    {
        return path;
    }

    /**
     * Restart the expiry countdown from now.
     */
    void updateExpiry()
    {
        expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(lifetimeMs);
    }

private:
    /**
     * The server to remove the resource from in the destructor.
     */
    Server::Server &server;

    /**
     * The path to the segment.
     *
     * This is useful to be able to delete the resource.
     */
    Server::Path path;

    /**
     * The lifetime of the interleave once the last stream has started writing to it.
     */
    const unsigned int lifetimeMs;

    /**
     * When the segment is to expire.
     */
    std::chrono::steady_clock::time_point expiry{std::chrono::steady_clock::duration(0)};
};

/**
 * Creates the resource for a single interleave, and keeps track of when it should expire.
 */
class InterleaveExpiringResource final : public ExpiringResource
{
public:
    /**
     * Create the interleave segment.
     *
     * @param server The server to create the interleave in.
     * @param path The path to the interleave.
     * @param lifetimeMs The lifetime of the interleave, in seconds. The expiry timer starts once the last stream has
     *                   been given the interleave.
     * @param numStreams The number of streams that are expected to use this interleave.
     * @param args The arguments to give to Dash::InterleaveResource::InterleaveResource.
     */
    template <typename... Args>
    explicit InterleaveExpiringResource(Server::Server &server, Server::Path path, unsigned int lifetimeMs,
                                        unsigned int numStreams, Args &&...args) :
        ExpiringResource(server, std::move(path), lifetimeMs, true),
        remainingResources(numStreams),
        resource(server.addOrReplaceResource<Dash::InterleaveResource>(getPath(), std::forward<Args>(args)...))
    {
        assert(remainingResources > 0);
    }

    /**
     * Record that another segment has been given this interleave.
     */
    ExpiringResource &operator++()
    {
        assert(remainingResources > 0);
        remainingResources--;
        if (remainingResources == 0) {
            updateExpiry();
        }
        return *this;
    }

    /**
     * Get a shared pointer to the interleave resource.
     */
    operator std::shared_ptr<Dash::InterleaveResource>() const
    {
        return resource;
    }

    /**
     * Determine if the interleave has expired.
     */
    bool expired(std::chrono::steady_clock::time_point now) const
    {
        return remainingResources == 0 && ExpiringResource::expired(now);
    }

private:
    /**
     * The number of streams that use this resource that haven't yet claimed it.
     *
     * This prevents the perverse case of an interleave expiring before all its streams have gotten it. This should not
     * happen, but just in case, ...
     */
    unsigned int remainingResources;

    /**
     * A shared pointer to let the child resources keep hold of the interleave.
     */
    std::shared_ptr<Dash::InterleaveResource> resource;
};

/**
 * Creates the resource for a single DASH segment, and keeps track of when it should expire.
 */
class SegmentExpiringResource final : public ExpiringResource
{
public:
    /**
     * Create a DASH segment resource.
     *
     * @param server The server to create the resource in.
     * @param path The path at which to create the segment.
     * @param lifetimeMs The lifetime, in ms, of the segment.
     * @param args The arguments to give to Dash::SegmentResource::SegmentResource.
     */
    template <typename... Args>
    explicit SegmentExpiringResource(Server::Server &server, Server::Path path, unsigned int lifetimeMs,
                                     Args &&...args) :
        ExpiringResource(server, std::move(path), lifetimeMs)
    {
        server.addOrReplaceResource<Dash::SegmentResource>(getPath(), std::forward<Args>(args)...);
        updateExpiry();
    }
};

/**
 * Maintain a garbage collectable map of segment descriptors.
 *
 * @tparam T A type with an expired method that takes the current time and returns whether the segment has expired or
 *           not. When destroyed, the T object should remove its segment from the server.
 */
template <typename T>
class StreamSegmentSet
{
public:
    /**
     * Get or create a T for a given index.
     *
     * @param index The index in the stream of the T to get.
     * @param args The arguments to give to T::T if the object for the given index does not exist.
     * @return The InterleaveSegment object corresponding to the given index.
     */
    template <typename... Args>
    T &get(unsigned int index, Args &&...args)
    {
        auto it = segments.find(index);
        if (it == segments.end()) {
            it = segments.emplace(index, T(std::forward<Args>(args)...)).first;
        }
        return it->second;
    }

    /**
     * Garbage collect the segments.
     *
     * @param now The time to consider to be now for the purposes of garbage collection.
     */
    void gc(std::chrono::steady_clock::time_point now)
    {
        std::erase_if(segments, [now](const auto &v) {
            return v.second.expired(now);
        });
    }

private:
    std::map<unsigned int, T> segments;
};

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

class Dash::DashResources::Interleave final : public StreamSegmentSet<InterleaveExpiringResource>
{
public:
    explicit Interleave(Server::Server &server, const Server::Path &basePath, unsigned int interleaveIndex,
                        const Config::Root &config) :
        server(server), basePath(basePath), interleaveIndex(interleaveIndex),
        numEphemeralNotFoundSegments(config.http.cacheNonLiveTime * 1000 / config.dash.segmentDuration + 1)
    {
    }

    /**
     * Add ephemeral not-found interleave segments, that haven't already been added, to cover the period after the given
     * segment index.
     *
     * These not-found error segments have ephemeral caching so that the not-found is not still cached by the time they
     * become pre-available.
     */
    void addEphemeralNotFoundSegments(unsigned int segmentIndex)
    {
        /* Figure out the start and end range of the not-found segments to create. */
        unsigned int firstEphemeralNotFoundSegment = std::max(segmentIndex + 1, nextEphemeralNotFoundSegment);
        nextEphemeralNotFoundSegment = std::max(segmentIndex + numEphemeralNotFoundSegments + 1,
                                                nextEphemeralNotFoundSegment);

        /* Create the resources for the not-found segments. */
        for (unsigned int i = firstEphemeralNotFoundSegment; i < nextEphemeralNotFoundSegment; i++) {
            Server::Path path = basePath / getInterleaveName(interleaveIndex, i);
            server.addResource<Server::ErrorResource>(path, Server::ErrorKind::NotFound, Server::CacheKind::ephemeral,
                                                      true);
        }
    }

private:
    Server::Server &server;
    const Server::Path &basePath;
    const unsigned int interleaveIndex;
    const unsigned int numEphemeralNotFoundSegments; ///< The number of not-found to create after the last live segment.
    unsigned int nextEphemeralNotFoundSegment = 0; ///< The first not-found segment that's not been created yet.
};

class Dash::DashResources::Stream final : public StreamSegmentSet<SegmentExpiringResource> {};

Dash::DashResources::~DashResources() = default;

Dash::DashResources::DashResources(IOContext &ioc, Log::Log &log, const Config::Root &config, Server::Server &server) :
    ioc(ioc), log(log), logContext(log("dash")), config(config), server(server),
    basePath(Util::replaceAll(config.paths.liveStream, "{uid}", getUid()))
{
    logContext << "basePath" << Log::Level::info << (std::string)getBasePath();

    /* Create an object to represent each stream and interleave. */
    {
        // Figure out how many audio streams there are.
        unsigned int numAudioStreams = 0;
        for (const Config::Quality &q: config.qualities) {
            if (q.audio) {
                numAudioStreams++;
            }
        }

        // Create DASH stream tracking for each audio and video stream.
        streams.resize(config.qualities.size() + numAudioStreams);

        // Create RISE interleave tracking. There are currently as many interleaves as video streams.
        interleaves.reserve(config.qualities.size());
        for (unsigned int i = 0; i < (unsigned int)config.qualities.size(); i++) {
            interleaves.emplace_back(server, getBasePath(), i, config);
        }
    }

    /* Add resources. */
    // The info.json.
    server.addResource<Server::ConstantResource>(config.paths.liveInfo, getLiveInfo(config, basePath),
                                                 "application/json", Server::CacheKind::ephemeral, true);

    // The manifest.mpd file.
    server.addResource<Server::PutResource>(basePath / "manifest.mpd", Server::CacheKind::fixed, true);

    // For now, each video quality has a single corresponding audio quality.
    {
        unsigned int videoIndex = 0;
        unsigned int audioIndex = (unsigned int)config.qualities.size();
        for (const Config::Quality &q: config.qualities) {
            // Add the first video segment and corresponding interleave.
            createSegment(videoIndex, 1);

            // Add the initializer segment.
            server.addResource<Server::PutResource>(basePath / getInitializerName(videoIndex), Server::CacheKind::fixed,
                                                    true);

            // Next video stream.
            videoIndex++;

            // Likewise for audio.
            if (!q.audio) {
                continue;
            }
            createSegment(audioIndex, 1);
            server.addResource<Server::PutResource>(basePath / getInitializerName(audioIndex), Server::CacheKind::fixed,
                                                    true);
            audioIndex++;
        }
    }
}

void Dash::DashResources::notifySegmentStart(unsigned int streamIndex, unsigned int segmentIndex)
{
    logContext << "segmentStart" << Log::Level::info << Json::dump({
        { "streamIndex", streamIndex },
        { "segmentIndex", segmentIndex }
    });

    spawnDetached(ioc, [=, this]() -> Awaitable<void> {
        /* Update the segment index descriptor. */
        try {
            server.addOrReplaceResource<SegmentIndexResource>(basePath / getSegmentIndexDescriptorName(streamIndex),
                                                              segmentIndex);
        }
        catch (const std::exception &e) {
            logContext << Log::Level::error
                       << "Exception while creating segment index descriptor " << segmentIndex
                       << " for stream " << (streamIndex + 1) << ": " << e.what() << ".";
        }
        catch (...) {
            logContext << Log::Level::error
                       << "Unknown exception while creating segment index descriptor " << segmentIndex
                       << " for stream " << (streamIndex + 1) << ".";
        }

        /* Create the next segment's resource once it's time for it to become pre-available. */
        try {
            // Wait for the segment to become pre-available.
            boost::asio::steady_timer timer(ioc);
            timer.expires_from_now(std::chrono::milliseconds(config.dash.segmentDuration -
                                                             config.dash.preAvailabilityTime));
            co_await timer.async_wait(boost::asio::use_awaitable);

            // Create the segment.
            createSegment(streamIndex, segmentIndex + 1);
        }
        catch (const std::exception &e) {
            logContext << Log::Level::error
                       << "Exception while creating pre-available segment " << segmentIndex
                       << " for stream " << (streamIndex + 1) << ": " << e.what() << ".";
        }
        catch (...) {
            logContext << Log::Level::error
                       << "Unknown exception while creating pre-available segment " << segmentIndex
                       << " for stream " << (streamIndex + 1) << ".";
        }
    });
}

void Dash::DashResources::createSegment(unsigned int streamIndex, unsigned int segmentIndex)
{
    logContext << "segmentPreavailable" << Log::Level::info << Json::dump({
        { "streamIndex", streamIndex },
        { "segmentIndex", segmentIndex }
    });

    assert(streamIndex < streams.size());
    bool isAudio = streamIndex >= config.qualities.size();
    unsigned int lifetimeMs = config.history.historyLength;

    /* Garbage collect existing segments. */
    gcSegments();

    /* Create a new interleave segment if the one we need doesn't exist already. */
    // Figure out the interleave index.
    unsigned int interleaveIndex = (streamIndex >= config.qualities.size()) ?
                                   streamIndex - (unsigned int)config.qualities.size() : streamIndex;
    assert(interleaveIndex < interleaves.size());

    // Figure out if this interleave has audio.
    assert(interleaves.size() == config.qualities.size());
    unsigned int interleaveNumStreams = config.qualities[interleaveIndex].audio ? 2 : 1;

    // Create the interleave and the descriptor we keep track of it with.
    InterleaveExpiringResource &interleave =
        interleaves[interleaveIndex].get(segmentIndex, server,
                                         basePath / getInterleaveName(interleaveIndex, segmentIndex), lifetimeMs,
                                         interleaveNumStreams, ioc, log, interleaveNumStreams,
                                         config.qualities[interleaveIndex].interleaveTimestampInterval);

    /* Add the new segment. */
    streams[streamIndex].get(segmentIndex, server,
                             basePath / getSegmentName(streamIndex, segmentIndex), lifetimeMs,
                             ioc, log, config.dash, *this, streamIndex, segmentIndex, interleave, interleaveIndex,
                             isAudio ? 1 : 0);
    ++interleave; // The stream now has been given the interleave.

    /* Set the caching for the following interleave segments (up to however many could be reached with fixed caching) to
       ephemeral. */
    interleaves[interleaveIndex].addEphemeralNotFoundSegments(segmentIndex);
}

void Dash::DashResources::gcSegments()
{
    auto now = std::chrono::steady_clock::now();
    for (Stream &s: streams) {
        s.gc(now);
    }
    for (Interleave &i: interleaves) {
        i.gc(now);
    }
}
