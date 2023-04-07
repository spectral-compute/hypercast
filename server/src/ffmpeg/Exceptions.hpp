#pragma once

#include <stdexcept>

namespace Ffmpeg
{

/**
 * Thrown when probing a URL that's in use with different parameters (either with a cached probe result, or with an
 * ffmpeg process).
 */
class InUseException final : public std::runtime_error
{
public:
    ~InUseException() override;
    using runtime_error::runtime_error;
};

} // namespace Ffmpeg
