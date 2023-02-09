#include "AudioCodecInfo.hpp"

#include "util/debug.hpp"

namespace
{

constexpr Codec::AudioCodecInfo none = {};

constexpr unsigned int aacSampleRates[] = { 7350, 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000,
                                            88200, 96000 };
constexpr Codec::AudioCodecInfo aac = {
    .sampleRates = aacSampleRates
};

constexpr unsigned int opusSampleRates[] = { 8000, 12000, 16000, 24000, 48000 };
constexpr Codec::AudioCodecInfo opus = {
    .sampleRates = opusSampleRates
};

} // namespace

const Codec::AudioCodecInfo &Codec::AudioCodecInfo::get(AudioCodec codec) noexcept
{
    switch (codec) {
        case AudioCodec::none: return none;
        case AudioCodec::aac: return aac;
        case AudioCodec::opus: return opus;
    }
    unreachable();
}
