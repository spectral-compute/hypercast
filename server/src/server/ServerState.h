#pragma once

#include "HttpServer.hpp"
#include "configuration/configuration.hpp"


namespace Server {


/// A place to keep the server's per-instance state.
struct State {
public:
    IOContext& ioc;

private:
    // The configuration currently active on the server. This configuration object is complete.
    Config::Root config;

public:
    // The configuration object that was loaded. This contains only the keys in the config file.
    Config::Root requestedConfig;

private:
    std::unique_ptr<Log::Log> log;

    HttpServer server;

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
