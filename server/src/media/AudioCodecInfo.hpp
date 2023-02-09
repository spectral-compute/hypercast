#pragma once

#include "codec.hpp"

#include <span>

/// @addtogroup media
/// @{

namespace Codec
{

/**
 * Information about the capabilities of an audio codec.
 */
struct AudioCodecInfo final
{
    /**
     * Get information about a specific codec.
     */
    static const AudioCodecInfo &get(AudioCodec codec) noexcept;

    /**
     * The (sorted) set of supported sample rates.
     */
    std::span<const unsigned int> sampleRates;
};

} // namespace Codec

/// @}
