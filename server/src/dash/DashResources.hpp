#pragma once

#include "log/Log.hpp"
#include "server/Path.hpp"

#include <vector>

class IOContext;

namespace Config
{

struct Dash;
class Root;

} // namespace Config

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
    explicit DashResources(IOContext &ioc, Log::Log &log, const Config::Root &config, Server::Server &server);

    /**
     * Notify that a given segment from a given stream has started to be received.
     *
     * This is needed so that the pre-availability of the next segment can be scheduled.
     *
     * @param streamIndex The stream index whose segment has started.
     * @param segmentIndex The index of the segment that's started.
     */
    void notifySegmentStart(unsigned int streamIndex, unsigned int segmentIndex);

private:
    class Interleave;
    class Stream;

    /**
     * Create the resources for the given segment.
     *
     * @param streamIndex The stream to which the segment belongs.
     * @param segmentIndex The index of the segment within the stream.
     */
    void createSegment(unsigned int streamIndex, unsigned int segmentIndex);

    /**
     * Remove segments that should have expired and should no longer be accessible.
     */
    void gcSegments();

    IOContext &ioc;
    Log::Log &log;
    Log::Context logContext;
    const Config::Root &config;
    Server::Server &server;

    /**
     * The base path for all the resources this object manages.
     */
    const Server::Path basePath;

    /**
     * Tracks state for each non-interleave stream.
     */
    std::vector<Stream> streams;

    /**
     * Tracks state for each interleave stream.
     */
    std::vector<Interleave> interleaves;
};

} // namespace Dash

/// @}