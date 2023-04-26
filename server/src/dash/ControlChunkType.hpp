#pragma once

#include <cstdint>

namespace Dash
{

/**
 * The type of control chunk.
 *
 * Control chunks (which have a stream index of all ones) are used for non media-stream purposes, such as conveying
 * real-time information to the client via the CDN.
 */
enum class ControlChunkType : uint8_t
{
    /* User message control chunk types. */
    /**
     * Send a JSON object to the client library for use by the user of the library.
     */
    userJsonObject = 48,

    /**
     * Send binary data to the client library for use by the user of the library.
     */
    userBinaryData = 49,

    /**
     * Send a string to the client library for use by the user of the library.
     */
    userString = 50,

    /* Other control chunk types. */
    /**
     * The client should discard this chunk.
     *
     * This control chunk type is useful for padding interleave to increase its data rate to overcome CDN buffering
     * delays. This class automatically inserts chunks of this type for that purpose.
     */
    discard = 255
};

} // namespace Dash
