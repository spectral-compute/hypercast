#include "configuration/configuration.hpp"

#include <cassert>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace Config
{

double getExplicitLatencySources(const Root &config);
double getAudioRate(const AudioQuality &aq);
double getVideoRateLatencyContribution(double videoRate, const Quality &q, const Root &config);

} // namespace Config

/// @addtogroup configuration_implementation
/// @{

namespace
{

/**
 * The maximum ratio between the video encoder's minimum and maximum bit rates.
 *
 * It's probably a good idea to make the minimum bit rate at least a bit smaller than the maximum rate, otherwise the
 * encoder's rate-control algorithm might become over-constrained.
 */
constexpr double maxVideoEncoderRateRangeRatio = 0.75;

/**
 * Fudge factor for rounding errors.
 *
 * This is 10 ms to account for the rounding error of converting to integer values.
 */
[[maybe_unused]] constexpr double epsilon = 1e-2;

/*
 * Get an exception for being unable to achieve the target latency.
 *
 * It's hard to come up with error messages that are succinct, but also meaningful without having very detailed
 * knowledge of the algorithm used to compute the defaults.
 */
[[nodiscard]] auto LatencyUnachievableException(std::string_view reason)
{
    return std::runtime_error("Target latency is unachievable because " + std::string(reason) + ".");
}

/**
 * Describes the actual value of a parameter and its associated latency.
 */
struct ParameterCandidate final
{
    /**
     * The value for this parameter.
     */
    double value = 0.0;

    /**
     * The additive contribution to latency corresponding to the chosen value.
     */
    double latency = 0.0;
};

/**
 * Get the video bit-rate that would be needed to target a given latency.
 */
ParameterCandidate getMinVideoRateForLatency(double latency, const Config::Quality &q, const Config::Root &config)
{
    double rate = config.network.transitBufferSize / latency - Config::getAudioRate(q.audio);
    double rateLatency = latency;
    if (rate < 0.0) {
        rate = 0.0;
        rateLatency = Config::getVideoRateLatencyContribution(rate, q, config);
    }
    else {
        assert(std::abs(Config::getVideoRateLatencyContribution(rate, q, config) - rateLatency) < epsilon);
    }
    return { .value = rate, .latency = rateLatency };
}

/**
 * Represents constraints on a parameter.
 */
struct ParameterConstraints final
{
    static ParameterConstraints getFromLatency(double minLatency, double maxLatency)
    {
        return ParameterConstraints{ .minValue = minLatency, .maxValue = maxLatency,
                                     .minLatency = minLatency, .maxLatency = maxLatency };
    }

    static ParameterConstraints getFromFixed(double value, double latency = 0.0)
    {
        return ParameterConstraints{ .minValue = value, .maxValue = value, .minLatency = latency, .maxLatency = latency,
                                     .fixed = true };
    }

    static ParameterConstraints getFromFixedLatency(double latency)
    {
        return getFromFixed(latency, latency);
    }

    /**
     * The minimum value this parameter can take.
     */
    double minValue = 0.0;

    /**
     * The maximum value this parameter can take.
     */
    double maxValue = 0.0;

    /**
     * The target value to use if we can.
     *
     * This is not set for every parameter type.
     */
    double targetValue = 0.0;

    /**
     * The minimum additive contribution to latency.
     *
     * In some cases, this could come from using maxValue rather than minValue.
     */
    double minLatency = 0.0;

    /**
     * The maximum additive contribution to latency.
     *
     * In some cases, this could come from using minValue rather than maxValue.
     */
    double maxLatency = 0.0;

    /**
     * Whether or not the parameter has a fixed value (either from the configuration or the default setting algorithm).
     */
    bool fixed = false;
};

/**
 * Get constraints on the maximum video rate can be.
 */
ParameterConstraints getMaxVideoRateConstraints(const Config::Quality &q)
{
    assert(q.video.width);
    assert(q.video.height);
    assert(q.video.frameRate.type == Config::FrameRate::fps);

    /* Handle the case where this parameter is set in the configuration. */
    if (q.video.bitrate) {
        return ParameterConstraints::getFromFixed(*q.video.bitrate);
    }

    /* A reference rate (in bytes per second) that we would choose as a target given set of reference video
       parameters. */
    // TODO: Choose these parameters empirically.
    constexpr double refRate = 3e6 / 8; // In bytes per second.
    constexpr unsigned int refWidth = 1920;
    constexpr unsigned int refHeight = 1080;
    constexpr double refFrameRate = (25 + 30) / 2.0;
    unsigned int refCrf = 25;

    // TODO: Provide a scale factor based on the preset.
    // TODO: Provide a scale factor based on the chosen codec.

    /* Scale the reference rate according to the *actual* parameters. */
    // A scaling for resolution somewhere between linear in side-length (or square root in number of pixels) and
    // linear in number of pixels is probably appropriate.
    // TODO: Choose this scaling function empirically.
    double resolutionScale = std::pow((*q.video.width * *q.video.height) / (double)(refWidth * refHeight), 0.75);

    // A sub-linear scaling function for frame rate is probably appropriate because human vision is less sensitive to
    // high frequencies.
    // TODO: Choose this scaling function empirically.
    double frameRateScale = std::pow(q.video.frameRate.numerator /
                                     (q.video.frameRate.denominator * (double)refFrameRate), 0.5);

    // CRF is defined to approximately double the bitrate when 6 is subtracted from it.
    // https://trac.ffmpeg.org/wiki/Encode/H.264#a1.ChooseaCRFvalue
    // TODO: Test that ffmpeg behaves in a way we consider nice when given all of a minimum bitrate, maximum bitrate,
    //       and CRF. That is: encode to the CRF subject to the other constraints.
    //       https://trac.ffmpeg.org/wiki/Encode/H.264#ConstrainedencodingVBVmaximumbitrate
    double crfScale = std::pow(2.0, ((int)refCrf - (int)q.video.crf) / 6.0);

    /* Produce upper and lower bounds based on a target rate. */
    double targetRate = refRate * resolutionScale * frameRateScale * crfScale;
    return { .minValue = targetRate / 2.0, .maxValue = targetRate * 2.0, .targetValue = targetRate };
}

/**
 * Get constraints on what the minimum video rate can be.
 *
 * @param maxRate The maximum value of the maximum video bit rate.
 */
ParameterConstraints getMinVideoRateConstraints(const Config::Quality &q, const Config::Root &config,
                                                double latencyBudget, double maxRate)
{
    /* Handle the case where this parameter is set in the configuration. */
    if (q.video.minBitrate) {
        double rate = *q.video.minBitrate * 125;
        return ParameterConstraints::getFromFixed(rate, Config::getVideoRateLatencyContribution(rate, q, config));
    }

    /* Calculate the maximum value for the minimum rate based on the maximum value for the maximum rate. */
    maxRate *= maxVideoEncoderRateRangeRatio;
    double minLatency = Config::getVideoRateLatencyContribution(maxRate, q, config);

    // If even the maximum rate is not high enough to fit within the entire rate budget, then the latency target is
    // obviously unachievable.
    if (latencyBudget - minLatency < 0.0) {
        throw LatencyUnachievableException("the minimum bitrate would be unreasonable");
    }

    /* Calculate the minimum rate based on the CDN buffer. */
    auto [minRate, maxLatency] = getMinVideoRateForLatency(latencyBudget, q, config);

    // Check the results for sanity.
    assert(minRate <= maxRate + epsilon); // Otherwise, the above throwing check must be broken.
    assert(minLatency <= maxLatency + epsilon);

    // Cope with floating point rounding errors.
    minRate = std::min(minRate, maxRate);
    minLatency = std::min(minLatency, maxLatency);

    /* Done :) */
    return { .minValue = minRate, .maxValue = maxRate, .minLatency = minLatency, .maxLatency = maxLatency };
}

/**
 * Relative latency allocation of sources of latency that are not already set.
 *
 * The initial state of this object is to give each source of latency some allocation of the budget. As we discover
 * either that we can't affect it because of an existing setting, or we allocate that budget by setting values, the
 * allocation to those parts is set to zero (since they're not part of the remaining budget any more).
 */
class LatencyBudget final
{
public:
    /**
     * Parameters we allocate for.
     */
    enum class Parameter
    {
        minBitRate,
        rateControlBufferLength,
        clientExtraBuffer
    };

    /**
     * The condition under which fixParameter should fix a parameter.
     */
    enum class FixParameterCondition
    {
        /**
         * Fix the parameter if it's already fixed.
         */
        ifFixed,

        /**
         * Fix the parameter if the default allocation would have a latency less than (or equal to) the constraint's
         * minimum latency.
         */
        lowLatency,

        /**
         * Fix the parameter if the default allocation would have a latency greater than than (or equal to) the
         * constraint's maximum latency.
         */
        highLatency,

        /**
         * Always fix the parameter unless it's already fixed.
         */
        ifNotFixed
    };

    /**
     * Create a latency budget with a given remaining latency.
     */
    LatencyBudget(double latency) : budget(latency) {}

    /**
     * Get the latency budget that has to be shared amongst the non-fixed (i.e: non-zero relative allocation)
     * parameters.
     */
    operator double() const
    {
        return budget;
    }

    /**
     * Sets a parameter if the default allocation lies outside that parameter's constraints.
     *
     * @param higherValueLowerLatency True if higher values lead to lower latency. False by default.
     * @param condition The condition under which to fix the parameter.
     * @param param The parameter we're considering.
     * @param constraints The parameter's constraints.
     * @param configValue The value in the configuration that corresponds to the parameter.
     * @param scaleFactor A multiplicative scale factor to apply to convert from SI+byte units to units used in the
     *                    configuration.
     * @param fn A function that takes a latency and returns a value for the parameter. If not set, then the identity
     *           function is used.
     */
    void fixParameter(bool higherValueLowerLatency, FixParameterCondition condition, Parameter param,
                      ParameterConstraints &constraints, std::optional<unsigned int> &configValue,
                      double scaleFactor = 1.0, const std::function<double (double)> &fn = [](double x) { return x; })
    {
        assert(constraints.minValue <= constraints.maxValue);
        assert(constraints.minLatency <= constraints.maxLatency);

        /* Handle already fixed parameters. */
        if (constraints.fixed) {
            assert(constraints.minValue == constraints.maxValue);
            assert(configValue);
            if (condition == FixParameterCondition::ifFixed) {
                assert(constraints.minLatency == constraints.maxLatency);
                removeParameter(param, constraints.maxLatency);
            }
            return;
        }
        assert(!configValue);

        /* Figure out what to compare to. */
        bool compareHigh = false;
        bool compareLow = false;
        switch (condition) {
            case FixParameterCondition::ifFixed:
                return; // Don't do any comparison. Never set any value.
            case FixParameterCondition::lowLatency:
                compareLow = !higherValueLowerLatency;
                compareHigh = higherValueLowerLatency;
                break;
            case FixParameterCondition::highLatency:
                compareLow = higherValueLowerLatency;
                compareHigh = !higherValueLowerLatency;
                break;
            case FixParameterCondition::ifNotFixed:
                break;
        }

        /* Figure out the latency for the default allocation and its corresponding value. */
        double defaultLatency = getAbsoluteBudget(param);
        double defaultValue = fn(defaultLatency);

        /* Don't set a value if the condition is not met. */
        if (compareHigh && defaultValue < constraints.maxValue) {
            return;
        }
        if (compareLow && defaultValue > constraints.maxValue) {
            return;
        }

        /* If we got here, we set the parameter, and thus should remove it from the budget. */
        constraints.minValue = defaultValue * scaleFactor;
        constraints.maxValue = constraints.minValue;
        constraints.fixed = true;
        configValue = (unsigned int)std::round(constraints.minValue);
        removeParameter(param, defaultLatency);
    }

    /**
     * Sets a parameter if the default allocation lies outside that parameter's constraints.
     *
     * @param condition The condition under which to fix the parameter.
     * @param param The parameter we're considering.
     * @param constraints The parameter's constraints.
     * @param configValue The value in the configuration that corresponds to the parameter.
     * @param scaleFactor A multiplicative scale factor to apply to convert from SI+byte units to units used in the
     *                    configuration.
     * @param fn A function that takes a latency and returns a value for the parameter. If not set, then the identity
     *           function is used.
     */
    void fixParameter(FixParameterCondition condition, Parameter param, ParameterConstraints &constraints,
                      std::optional<unsigned int> &configValue, double scaleFactor = 1.0,
                      const std::function<double (double)> &fn = [](double x) { return x; })
    {
        fixParameter(false, condition, param, constraints, configValue, scaleFactor, fn);
    }

private:
    /**
     * Get the budget for a given parameter according to the default relative budgets and remaining budget.
     */
    double getAbsoluteBudget(Parameter param) const
    {
        double sum = 0.0;
        for (double r: relative) {
            sum += r;
        }
        return relative[(int)param] * budget / sum;
    }

    /**
     * Remove a parameter from the latency budget.
     *
     * @param latency The now known latency of the parameter.
     */
    void removeParameter(Parameter param, double latency)
    {
        relative[(int)param] = 0.0;
        budget -= latency;
        assert(budget >= -epsilon);
    }

    /**
     * Absolute remaining budget.
     */
    double budget;

    /**
     * Relative budgets for each parameter. These are in the order of Parameter.
     */
    double relative[3] = { 1.0, 1.0, 0.25 };
};

} // namespace

/// @}

namespace Config
{

/**
 * Fill in the missing bitrate and buffer control settings for a quality that require latency allocation.
 *
 * This includes:
 *  - Maximum video bitrate (upon which the minimum video bitrate depends).
 *  - Minimum video bitrate.
 *  - The video encoder's rate control buffer length.
 *  - The client's extra buffer parameter.
 */
void allocateLatency(Quality &q, const Root &config)
{
    /* Figure out what the latency budget is in seconds. */
    // TODO: Empirically measure the defaults for the network sources of latency.
    LatencyBudget latencyBudget = q.targetLatency / 1000.0 - getExplicitLatencySources(config);
    if (latencyBudget < 0.0) {
        throw LatencyUnachievableException("the explicit latency sources exceed it");
    }

    /* Calculate the minimum and maximum values for each of the parameters. */
    // Maximum/average rate in bytes per second.
    ParameterConstraints maxVideoRateConstraints = getMaxVideoRateConstraints(q);

    // Minimum video rate in bytes per second.
    ParameterConstraints minVideoRateConstraints = getMinVideoRateConstraints(q, config, latencyBudget,
                                                                              maxVideoRateConstraints.maxValue);

    // Encoder rate control buffer length.
    ParameterConstraints rateControlBufferLengthConstraints =
        q.video.rateControlBufferLength ?
        ParameterConstraints::getFromFixedLatency(*q.video.rateControlBufferLength / 1000.0) :
        ParameterConstraints::getFromLatency(0.25, 2.0);

    // Client extra buffer.
    // Note that although the client has extra buffering due to network jitter, that's accounted for in what we already
    // removed from the latency budget. Jitter from the CDN buffer's response to the difference between the minimum and
    // maximum bitrates is absorbed by the latency of the CDN buffer's response to the minimum bitrate.
    ParameterConstraints clientExtraBufferConstraints =
        q.clientBufferControl.extraBuffer ?
        ParameterConstraints::getFromFixedLatency(*q.clientBufferControl.extraBuffer / 1000.0) :
        ParameterConstraints::getFromLatency(0.1, 10.0); // High value unused except with a very high target latency.

    /* Check that the sum of the minimum latencies doesn't exceed the latency budget. */
    if ((minVideoRateConstraints.minLatency + rateControlBufferLengthConstraints.minLatency +
         clientExtraBufferConstraints.minLatency) > latencyBudget)
    {
        throw LatencyUnachievableException("the sum of the set and minimum reasonable default latencies exceed it");
    }

    /* Allocate the latency budget. */
    for (LatencyBudget::FixParameterCondition condition: {
        // Parameters that are already fixed by the configuration.
        LatencyBudget::FixParameterCondition::ifFixed,

        // Set parameters whose maximum latency is less than (or equal to) what their default allocation would be. This
        // increases the remaining budget for everything else.
        LatencyBudget::FixParameterCondition::lowLatency,

        // Set parameters whose minimum latency is more than (or equal to) what their default allocation would be. This
        // decreases the remaining budget for everything else, but the fact that it doesn't cause the budget to be
        // exceeded is enforced by checking the sum of the minimum latencies earlier.
        LatencyBudget::FixParameterCondition::highLatency,

        // Everything else should get the default value, which will be in range.
        LatencyBudget::FixParameterCondition::ifNotFixed
    }) {
        latencyBudget.fixParameter(true, condition, LatencyBudget::Parameter::minBitRate, minVideoRateConstraints,
                                   q.video.minBitrate, 1.0 / 125.0, [&](double latency) {
            return getMinVideoRateForLatency(latency, q, config).value;
        });
        latencyBudget.fixParameter(condition, LatencyBudget::Parameter::rateControlBufferLength,
                                   rateControlBufferLengthConstraints, q.video.rateControlBufferLength, 1000.0);
        latencyBudget.fixParameter(condition, LatencyBudget::Parameter::clientExtraBuffer, clientExtraBufferConstraints,
                                   q.clientBufferControl.extraBuffer, 1000.0);
    }
    assert(latencyBudget >= -epsilon); // Implied by not having done the preceding throw.

    /* Choose a maximum video bitrate. */
    assert(minVideoRateConstraints.minValue == minVideoRateConstraints.maxValue); // I.e: Fixed.
    assert(maxVideoRateConstraints.minValue <= maxVideoRateConstraints.targetValue); // Target in range.
    assert(maxVideoRateConstraints.targetValue <= maxVideoRateConstraints.maxValue); // Target in range.
    assert(minVideoRateConstraints.maxValue <=
           maxVideoRateConstraints.maxValue * maxVideoEncoderRateRangeRatio); // Not over-constrained.

    // Calculate the maximum bitrate to be the target rate, subject to that not over-constraining the encoder.
    double maxVideoRate = std::max(maxVideoRateConstraints.targetValue,
                                   minVideoRateConstraints.maxValue / maxVideoEncoderRateRangeRatio);
    assert(maxVideoRate >= maxVideoRateConstraints.minValue);
    assert(maxVideoRate <= maxVideoRateConstraints.maxValue);

    // Set the maximum bitrate.
    // TODO: Account for maximum vs average bitrate.
    q.video.bitrate = maxVideoRate / 125.0;
}

} // namespace Config
