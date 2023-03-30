#include "configuration/defaults.hpp"
#include "configuration/configuration.hpp"

#include <cassert>
#include <cmath>

namespace Config
{

void allocateLatency(Config::Quality &q, const Root &config, const Channel &channel);

/**
 * Get latency sources that are explicit or intrinsic to the source in seconds.
 */
double getExplicitLatencySources(const Root &config, const Channel &channel)
{
    assert(channel.source.latency);
    return (*channel.source.latency + config.network.transitLatency + config.network.transitJitter) / 1000.0;
}

/**
 * Get the audio rate in bytes per second for a quality, accounting for the fact that there may be no audio at all.
 */
double getAudioRate(const AudioQuality &aq)
{
    return aq ? (aq.bitrate * 125) : 0.0;
}

/**
 * Figure out the contribution of the video bit rate (when combined with audio bit rate) to the latency.
 */
double getVideoRateLatencyContribution(double videoRate, const Quality &q, const Root &config)
{
    return config.network.transitBufferSize / (videoRate + getAudioRate(q.audio));
}

/**
 * Fill in a quality.
 */
void fillInQuality(Config::Quality &q, const Root &config, const Channel &channel)
{
    assert(q.video.frameRate.type == Config::FrameRate::fps);

    /* Set the GOP size. */
    if (!q.video.gop) {
        q.video.gop = (q.video.frameRate.numerator * channel.dash.segmentDuration + 500) /
                      (q.video.frameRate.denominator * 1000);
    }

    /* Allocate the latency budget between the various things that use it. This also sets the maximum video bitrate. */
    allocateLatency(q, config, channel);
    assert(q.video.bitrate);
    assert(q.video.minBitrate);
    assert(q.video.rateControlBufferLength);
    assert(q.clientBufferControl.extraBuffer);

    /* Calculate the interleave window so we know some statistical properties of the minimum interleave rate. */
    if (!q.minInterleaveWindow) {
        q.minInterleaveWindow = std::min(*q.video.rateControlBufferLength / 2, 250u);
    }

    /* Set the client buffer control parameters based on the jitter they have to deal with (and target latency). */
    // Set the time to wait before seeking based on the timestamp rate and interleave window.
    if (!q.clientBufferControl.minimumInitTime) {
        q.clientBufferControl.minimumInitTime = std::max(q.interleaveTimestampInterval * 16,
                                                         *q.minInterleaveWindow * 4);
    }

    // Set the initial buffer based on the expected jitter.
    if (!q.clientBufferControl.initialBuffer) {
        // Get the latencies for the minimum bitrate and maximum bitrate.
        double minVideoRateLatency = getVideoRateLatencyContribution(*q.video.minBitrate * 125, q, config);
        double maxVideoRateLatency = getVideoRateLatencyContribution(*q.video.bitrate * 125, q, config);
        assert(maxVideoRateLatency < minVideoRateLatency);

        // Set the seek buffer based on this implied jitter.
        q.clientBufferControl.initialBuffer =
            config.network.transitJitter + std::round((minVideoRateLatency - maxVideoRateLatency) * 1000.0);
    }

    // By default, set the seek buffer to the extra buffer. This is the one part that doesn't try to fully account for
    // jitter.
    // TODO: For that, I guess we probably want to make the client keep an estimate of where the leading edge actually
    //       is (ignoring minimum latency).
    if (!q.clientBufferControl.seekBuffer) {
        q.clientBufferControl.seekBuffer = *q.clientBufferControl.extraBuffer;
    }

    /* Calculate a minimum interleave rate. */
    if (!q.minInterleaveRate) {
        double interleaveRateLatency =
            (q.targetLatency - *q.minInterleaveWindow - *q.clientBufferControl.extraBuffer) / 1000.0 -
            getExplicitLatencySources(config, channel);
        assert(interleaveRateLatency > 0.0);

        double interleaveRate = config.network.transitBufferSize / interleaveRateLatency;
        assert(interleaveRate < (*q.video.minBitrate * 125) + getAudioRate(q.audio));

        q.minInterleaveRate = (unsigned int)std::round(interleaveRate * 8.0 / 1000.0);
    }
}

} // namespace Config
