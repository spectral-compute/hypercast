#include "DashResources.hpp"

#include "api/channel/SendDataResource.hpp"
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
 * Format the current time as a lexicographically sortable human readable timestamp.
 */
std::string formatPersistenceTimestamp()
{
    /* Get the current time as a UTC timezoned timestamp. */
    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const tm *utc = gmtime(&now);

    // If we couldn't convert the timestamp to a UTC timezone, return the integer representation.
    if (!utc) {
        return std::to_string(now);
    }

    /* Format the timezoned timestamp. */
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%04i-%02i-%02i %02i-%02i-%02i",
             utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday, utc->tm_hour, utc->tm_min, utc->tm_sec);
    return buffer;
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
     * The largest value that the steady clock can represent.
     */
    static constexpr auto maxSteadyTime =
        std::chrono::steady_clock::duration(std::numeric_limits<std::chrono::steady_clock::duration::rep>::max());

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
    std::chrono::steady_clock::time_point expiry{maxSteadyTime};
};

} // namespace

/**
 * Creates the resource for a single interleave, and keeps track of when it should expire.
 */
class Dash::DashResources::InterleaveExpiringResource final : public ExpiringResource
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
     * Get a reference to the interleave resource.
     */
    Dash::InterleaveResource &operator*() const
    {
        return *resource;
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

namespace
{

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

    /**
     * Get the index of the last segment, if any.
     */
    std::optional<unsigned int> getLastSegmentIndex()
    {
        if (segments.empty()) {
            return std::nullopt;
        }
        return segments.rbegin()->first;
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
 * @param uidPath The base path (with unique ID substituted) to the live streaming resources.
 */
std::string getLiveInfo(const Config::Channel &config, const Server::Path &uidPath);

} // namespace Dash

class Dash::DashResources::Interleave final : public StreamSegmentSet<InterleaveExpiringResource>
{
public:
    explicit Interleave(Server::Server &server, const Server::Path &uidPath, unsigned int interleaveIndex,
                        const Config::Channel &channelConfig, const Config::Http &httpConfig) :
        server(server), uidPath(uidPath), interleaveIndex(interleaveIndex),
        numEphemeralNotFoundSegments(httpConfig.cacheNonLiveTime * 1000 / channelConfig.dash.segmentDuration + 1)
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
            Server::Path path = uidPath / getInterleaveName(interleaveIndex, i);
            server.addResource<Server::ErrorResource>(path, Server::ErrorKind::NotFound, Server::CacheKind::ephemeral,
                                                      true);
        }
    }

private:
    Server::Server &server;
    const Server::Path &uidPath;
    const unsigned int interleaveIndex;
    const unsigned int numEphemeralNotFoundSegments; ///< The number of not-found to create after the last live segment.
    unsigned int nextEphemeralNotFoundSegment = 0; ///< The first not-found segment that's not been created yet.
};

class Dash::DashResources::Stream final : public StreamSegmentSet<SegmentExpiringResource> {};

Dash::DashResources::~DashResources()
{
    streams.clear();
    interleaves.clear();
    server.removeResourceTree(basePath);
}

Dash::DashResources::DashResources(IOContext &ioc, Log::Log &log, const Config::Channel &channelConfig,
                                   const Config::Http &httpConfig, Server::Path basePath, Server::Server &server) :
    ioc(ioc), log(log), logContext(log("dash")), config(channelConfig), server(server),
    basePath(std::move(basePath)), uidPath(this->basePath / channelConfig.uid),
    persistenceDirectory(config.history.persistentStorage.empty() ? std::filesystem::path{} :
                         std::filesystem::path(config.history.persistentStorage) / formatPersistenceTimestamp()),
    exists(std::make_shared<char>(0))
{
    logContext << "base path" << Log::Level::info << (std::string)getBasePath();
    logContext << "uid path" << Log::Level::info << (std::string)getUidPath();

    /* Create the API resources. */
    {
        Server::Path apiBasePath = Server::Path("api/channels") / this->basePath;
        server.addResource<Api::Channel::SendDataResource>(apiBasePath / "send_user_json", *this);
    }

    /* Create the persistence directory. */
    if (!persistenceDirectory.empty()) {
        logContext << "persistence" << Log::Level::info << persistenceDirectory;
        if (std::filesystem::exists(persistenceDirectory)) {
            logContext << Log::Level::error << "Persistence directory exists.";
            throw std::runtime_error("Persistence directory exists.");
        }
        std::filesystem::create_directory(persistenceDirectory);
    }

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
            interleaves.emplace_back(server, uidPath, i, config, httpConfig);
        }
    }

    /* Add resources. */
    // The info.json.
    server.addResource<Server::ConstantResource>(this->basePath / "info.json", getLiveInfo(config, uidPath),
                                                 "application/json", Server::CacheKind::ephemeral, true);

    // The manifest.mpd file.
    server.addResource<Server::PutResource>(uidPath / "manifest.mpd", ioc, getPersistencePath("manifest.mpd"),
                                            Server::CacheKind::fixed, 1 << 16, true);

    // For now, each video quality has a single corresponding audio quality.
    {
        unsigned int videoIndex = 0;
        unsigned int audioIndex = (unsigned int)config.qualities.size();
        for (const Config::Quality &q: config.qualities) {
            // Add the initializer segment.
            {
                std::string initializerSegmentName = getInitializerName(videoIndex);
                server.addResource<Server::PutResource>(uidPath / initializerSegmentName, ioc,
                                                        getPersistencePath(initializerSegmentName),
                                                        Server::CacheKind::fixed,
                                                        1 << 14, true);
            }
            // Add the first video segment and corresponding interleave.
            createSegment(videoIndex, 1);

            // Next video stream.
            videoIndex++;

            // Likewise for audio.
            if (!q.audio) {
                continue;
            }
            {
                std::string initializerSegmentName = getInitializerName(audioIndex);
                server.addResource<Server::PutResource>(uidPath / initializerSegmentName, ioc,
                                                        getPersistencePath(initializerSegmentName),
                                                        Server::CacheKind::fixed,
                                                        1 << 14, true);
            }
            createSegment(audioIndex, 1);
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

    spawnDetached(ioc, [=, this, stillExists = std::weak_ptr(exists)]() -> Awaitable<void> {
        /* Don't play with a dead object. */
        if (stillExists.expired()) {
            co_return;
        }

        /* Update the segment index descriptor. */
        try {
            server.addOrReplaceResource<SegmentIndexResource>(uidPath / getSegmentIndexDescriptorName(streamIndex),
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

            // The this object might have been deleted while waiting.
            if (stillExists.expired()) {
                co_return;
            }

            // Create the segment.
            createSegment(streamIndex, segmentIndex + 1);
        }
        catch (const std::exception &e) {
            if (stillExists.expired()) {
                co_return;
            }
            logContext << Log::Level::error
                       << "Exception while creating pre-available segment " << segmentIndex
                       << " for stream " << (streamIndex + 1) << ": " << e.what() << ".";
        }
        catch (...) {
            if (stillExists.expired()) {
                co_return;
            }
            logContext << Log::Level::error
                       << "Unknown exception while creating pre-available segment " << segmentIndex
                       << " for stream " << (streamIndex + 1) << ".";
        }
    });
}

void Dash::DashResources::addControlChunk(std::span<const std::byte> chunkData, ControlChunkType type)
{
    /* Add the control chunk to every interleave. */
    for (unsigned int i = 0; i < interleaves.size(); i++) {
        // Find the last interleave segment.
        std::optional<unsigned int> lastSegmentIndex = interleaves[i].getLastSegmentIndex();
        if (!lastSegmentIndex) {
            lastSegmentIndex = 1; // This is the index of the first segment, and we haven't had any yet.
        }
        Dash::InterleaveResource *segment = &*getInterleaveSegment(i, *lastSegmentIndex);

        // If this interleave hasn't started yet, try the previous one. This accounts for pre-available interleaves. If
        // we're at the point of having ended segment N but not yet started segment N+1, we'll get bumped back.
        if (!segment->hasStarted() && (*lastSegmentIndex) > 1) {
            (*lastSegmentIndex)--;
            segment = &*getInterleaveSegment(i, *lastSegmentIndex);
        }

        // Create the next interleave segment if this one's ended. It's possible this'll bump us back to one that's not
        // started, but that's what we want if the previous one ended and the next one hasn't started.
        else if (segment->hasEnded()) {
            (*lastSegmentIndex)++;
            segment = &*getInterleaveSegment(i, *lastSegmentIndex);
            assert(!segment->hasEnded());
        }

        // Add the control chunk.
        segment->addControlChunk(chunkData, type);
    }
}

void Dash::DashResources::createSegment(unsigned int streamIndex, unsigned int segmentIndex)
{
    logContext << "segmentPreavailable" << Log::Level::info << Json::dump({
        { "streamIndex", streamIndex },
        { "segmentIndex", segmentIndex }
    });

    assert(streamIndex < streams.size());
    bool isAudio = streamIndex >= config.qualities.size();

    /* Garbage collect existing segments. */
    gcSegments();

    /* Create a new interleave segment if the one we need doesn't exist already. */
    // Figure out the interleave index.
    unsigned int interleaveIndex = (streamIndex >= config.qualities.size()) ?
                                   streamIndex - (unsigned int)config.qualities.size() : streamIndex;

    // Create the corresponding interleave segment if it doesn't already exist.
    InterleaveExpiringResource &interleave = getInterleaveSegment(interleaveIndex, segmentIndex);

    /* Add the new segment. */
    {
        std::string segmentName = getSegmentName(streamIndex, segmentIndex);
        streams[streamIndex].get(segmentIndex, server, uidPath / segmentName, config.history.historyLength * 1000,
                                 ioc, log, config.dash, *this, streamIndex, segmentIndex, interleave, interleaveIndex,
                                 isAudio ? 1 : 0, getPersistencePath(segmentName));
    }
    ++interleave; // The stream now has been given the interleave.

    /* Set the caching for the following interleave segments (up to however many could be reached with fixed caching) to
       ephemeral. */
    interleaves[interleaveIndex].addEphemeralNotFoundSegments(segmentIndex);
}

Dash::DashResources::InterleaveExpiringResource &
Dash::DashResources::getInterleaveSegment(unsigned int interleaveIndex, unsigned int segmentIndex)
{
    assert(interleaveIndex < interleaves.size());

    /* Figure out if this interleave has audio. */
    assert(interleaves.size() == config.qualities.size());
    unsigned int interleaveNumStreams = config.qualities[interleaveIndex].audio ? 2 : 1;

    /* Create the interleave and the descriptor we keep track of it with. */
    const Config::Quality &q = config.qualities[interleaveIndex];
    return interleaves[interleaveIndex].get(segmentIndex, server,
                                            uidPath / getInterleaveName(interleaveIndex, segmentIndex),
                                            config.history.historyLength * 1000, interleaveNumStreams, ioc, log,
                                            interleaveNumStreams,
                                            (*q.minInterleaveRate * *q.minInterleaveWindow + 7) / 8,
                                            *q.minInterleaveWindow, q.interleaveTimestampInterval);
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

std::filesystem::path Dash::DashResources::getPersistencePath(std::string_view fileName) const
{
    if (persistenceDirectory.empty()) {
        return {};
    }
    return persistenceDirectory / fileName;
}
