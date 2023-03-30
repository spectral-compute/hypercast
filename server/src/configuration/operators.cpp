#include "configuration/configuration.hpp"

/**
 * @file
 *
 * These are expensive and used repeatedly (in the tests, and elsewhere), so don't keep code-generating them repeatedly.
 */

#ifndef WITH_TESTING
Config::Root::~Root() = default;

Config::Root::Root(const Root &) = default;
Config::Root::Root(Root &&) noexcept = default;
Config::Root &Config::Root::operator=(const Root &) = default;
Config::Root &Config::Root::operator=(Root &&) noexcept = default;
#endif // WITH_TESTING

bool Config::Source::operator==(const Source &) const = default;
bool Config::FrameRate::operator==(const FrameRate &) const = default;
bool Config::VideoQuality::operator==(const VideoQuality &) const = default;
bool Config::AudioQuality::operator==(const AudioQuality &) const = default;
bool Config::ClientBufferControl::operator==(const ClientBufferControl &) const = default;
bool Config::Quality::operator==(const Quality &) const = default;
bool Config::Dash::operator==(const Dash &) const = default;
bool Config::History::operator==(const History &) const = default;
bool Config::Channel::operator==(const Channel &) const = default;
bool Config::Directory::operator==(const Directory &) const = default;
bool Config::Network::operator==(const Network &) const = default;
bool Config::Http::operator==(const Http &) const = default;
bool Config::Log::operator==(const Log &) const = default;
bool Config::Root::operator==(const Root &) const = default;
