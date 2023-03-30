#pragma once

#include "HttpServer.hpp"
#include "configuration/configuration.hpp"

#include <map>

namespace Server {


/// A place to keep the server's per-instance state.
struct State final
{
public:
    ~State();

    IOContext& ioc;

private:
    // The configuration currently active on the server. This configuration object is complete.
    Config::Root config;

public:
    // The configuration object that was loaded. This contains only the keys in the config file.
    Config::Root requestedConfig;

private:
    struct Channel;

    std::unique_ptr<Log::Log> log;

    HttpServer server;

    /**
     * The state for the channel that's streaming.
     *
     * TODO: Currently, this is a single channel. Eventually, we'll want to make this a map from configuration channel
     *       to channel state.
     */
    std::map<std::string, Channel> channels;

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
