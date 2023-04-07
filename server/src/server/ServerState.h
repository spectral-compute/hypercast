#pragma once

#include "HttpServer.hpp"
#include "configuration/configuration.hpp"
#include "util/Mutex.hpp"

#include <map>
#include <stdexcept>

namespace MediaInfo
{

struct SourceInfo;

} // namespace MediaInfo

namespace Server {

/**
 * An exception that's thrown by State::applyConfiguration if the configuration modification can't be applied.
 */
class BadConfigurationReplacementException final : public std::runtime_error
{
public:
    BadConfigurationReplacementException(const std::string &message) : runtime_error(message) {}
};

/// A place to keep the server's per-instance state.
struct State final
{
public:
    ~State();

    /**
     * Get the server that this object is the associated state for.
     */
    Server &getServer()
    {
        return server;
    }

    /**
     * Get the (fully filled-in) configuration.
     */
    const Config::Root &getConfiguration() const
    {
        return config;
    }

    /**
     * Get the set of URs that are in use by the current configuration.
     */
    const std::set<std::string> &getInUseUrls() const
    {
        return inUseUrls;
    }

    IOContext& ioc;

private:
    // The configuration currently active on the server. This configuration object is complete.
    Config::Root config;

public:
    // The configuration object that was loaded. This contains only the keys in the config file.
    Config::Root requestedConfig;

private:
    struct Channel;

    /**
     * Prevent concurrent calls to applyConfiguration.
     */
    Mutex mutex;

    std::unique_ptr<Log::Log> log;

    HttpServer server;

    /**
     * The state for the channel that's streaming.
     */
    std::map<std::string, Channel> channels;

    /**
     * The set of URLs that are in use by the current configuration.
     *
     * This is an optimization over analyzing the configuration each time.
     */
    std::set<std::string> inUseUrls;

    // Flag to suppress "you can't change that" for the first run of `applyConfiguration`, allowing us to use
    // `applyConfiguration` for initial configuration.
    bool performingStartup = true;

    /// Used to throw exceptions if you try to change a setting that isn't allowed to change except on startup.
    void configCannotChange(bool itChanged, const std::string& name) const;

public:
    /// Perform initial setup/configuration.
    State(
        const Config::Root& initialCfg,
        IOContext& ioc
    );

    inline IOContext& getIoc() const {
        return ioc;
    }
    inline Log::Log& getLog() const {
        return *log;
    }

    /// Change the settings. Add as much clever incremental reconfiguration logic here as you like.
    /// Various options are re-read every time they're used and don't require explicit reconfiguration,
    /// so they don't appear specifically within this function.
    Awaitable<void> applyConfiguration(Config::Root newCfg);
};


} // namespace Server
