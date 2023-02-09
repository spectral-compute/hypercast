#pragma once

/// @addtogroup media
/// @{

/**
 * Information about codecs.
 */
namespace Codec
{

/**
 * A video codec.
 */
enum class VideoCodec
{
    h264, h265, vp8, vp9, av1
};

/**
 * An audio codec.
 */
enum class AudioCodec
{
    none, aac, opus
};

} // namespace Codec

/// @}
