#pragma once

#include "ControlChunkType.hpp"

#include "log/Log.hpp"
#include "server/Path.hpp"

#include <filesystem>
#include <memory>
#include <span>
#include <vector>

class IOContext;

namespace Config
{

struct Channel;
struct Dash;
struct Http;

} // namespace Config

namespace Ffmpeg
{

class Process;

} // namespace Ffmpeg

namespace Json
{

class Object;

} // namespace Json

namespace Server
{

class Path;
class Server;

} // namespace Server

/**
 * @defgroup dash DASH/RISE
 *
 * Stuff for accepting DASH input and converting it to RISE.
 */
/// @addtogroup dash
/// @{

/**
 * Stuff for accepting DASH input and converting it to RISE.
 */
namespace Dash
{

/**
 * Coordinates all the DASH/RISE resources.
 */
class DashResources final
{
public:
    ~DashResources();

    /**
     * Add the resources to the server for accepting DASH and converting to RISE, and prepare to manage that process
     * ongoing.
     */
    explicit DashResources(IOContext &ioc, Log::Log &log, const Config::Channel &config, const Config::Http &httpConfig,
                           Server::Path basePath, Server::Server &server, const Ffmpeg::Process &ffmpegProcess);

    DashResources(const DashResources &) = delete;
    DashResources & operator=(const DashResources &) = delete;

    /**
     * Notify that a given segment from a given stream has started to be received.
     *
     * This is needed so that the pre-availability of the next segment can be scheduled.
     *
     * @param streamIndex The stream index whose segment has started.
     * @param segmentIndex The index of the segment that's started.
     */
    void notifySegmentStart(unsigned int streamIndex, unsigned int segmentIndex);

    /**
     * Add a control chunk to all the interleaves' latest segments.
     *
     * This might create new interleave segments if the latest ones are all ended (or non-existent).
     *
     * @param chunkData The data for the control chunk.
     * @param type The control chunk type.
     */
    void addControlChunk(std::span<const std::byte> chunkData, ControlChunkType type);

    /**
     * @copydoc addControlChunk
     */
    void addControlChunk(std::string_view chunkData, ControlChunkType type)
    {
        addControlChunk(std::span((const std::byte *)chunkData.data(), chunkData.size()), type);
    }

    /**
     * Add a JSON object control chunk to all the interleaves' latest segments.
     *
     * @param j The value to give to the conent field.
     * @param type The value to give to the type field.
     */
    void addJsonObjectControlChunk(Json::Object j, std::string_view type);

    /**
     * Get the base path in the server for the resources managed by this object.
     */
    const Server::Path &getBasePath() const
    {
        return basePath;
    }

    /**
     * Get the base path in the server for the segments managed by this object.
     *
     * This exists so that the stream can be restarted without stale segments being served by the CDN.
     */
    const Server::Path &getUidPath() const
    {
        return uidPath;
    }

private:
    class Interleave;
    class Stream;
    class InterleaveExpiringResource;

    /**
     * Create the resources for the given segment.
     *
     * @param streamIndex The stream to which the segment belongs.
     * @param segmentIndex The index of the segment within the stream.
     */
    void createSegment(unsigned int streamIndex, unsigned int segmentIndex);

    /**
     * Get (and possibly create) the given segment of the given interleave.
     */
    InterleaveExpiringResource &getInterleaveSegment(unsigned int interleaveIndex, unsigned int segmentIndex);

    /**
     * Remove segments that should have expired and should no longer be accessible.
     */
    void gcSegments();

    /**
     * Get the full path to save the given DASH file to, if we're saving persistently.
     */
    std::filesystem::path getPersistencePath(std::string_view fileName) const;

    IOContext &ioc;
    Log::Log &log;
    Log::Context logContext;
    const Config::Channel &config;
    Server::Server &server;

    /**
     * The base path for all the resources this object manages.
     */
    const Server::Path basePath;

    /**
     * The UID for live resources that might need non-ephemeral caching but might also have name collisions.
     */
    const Server::Path uidPath;

    /**
     * The directory to store persistent DASH streams to.
     */
    const std::filesystem::path persistenceDirectory;

    /**
     * Tracks state for each non-interleave stream.
     */
    std::vector<Stream> streams;

    /**
     * Tracks state for each interleave stream.
     */
    std::vector<Interleave> interleaves;

    /**
     * Something to create weak pointers from to determine from a coroutine whether the object parent still exist.
     */
    std::shared_ptr<char> exists;
};

} // namespace Dash

/// @}
