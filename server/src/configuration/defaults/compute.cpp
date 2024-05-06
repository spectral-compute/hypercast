#include "configuration/configuration.hpp"

#include <cassert>

namespace Config
{

/**
 * Fills in the compute trade-off.
 *
 * Currently, this is tuned crudely and rather non-generically for the Ryzen 7950X. This model will probably be improved
 * over time.
 */
void fillInCompute(Root &config)
{
    for (auto &[name, channel]: config.channels) {
        for (Quality &q: channel.qualities) {
            assert(q.video.width);
            assert(q.video.height);
            assert(q.video.frameRate.type == FrameRate::fps);

            /* Get the stream parameters. */
            unsigned int width = *q.video.width;
            unsigned int height = *q.video.height;
            unsigned int fps =
                (q.video.frameRate.numerator + q.video.frameRate.denominator - 1) / q.video.frameRate.denominator;

            /* Choose a preset based on the above. */
            std::optional<H26xPreset> &preset = q.video.h26xPreset;
            if (preset) {
                continue;
            }
            if (fps >= 60) {
                preset = H26xPreset::ultrafast;
            }
            else if (width <= 1920 && height <= 1080) {
                preset = (fps <= 30) ? H26xPreset::medium : H26xPreset::faster;
            }
            else if (width <= 3840 && height <= 2160) {
                preset = (fps <= 30) ? H26xPreset::faster : H26xPreset::superfast;
            }
            else {
                preset = H26xPreset::ultrafast;
            }
        }
    }
}

} // namespace Config
