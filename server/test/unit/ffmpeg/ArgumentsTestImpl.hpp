#pragma once

#include <string>
#include <vector>

namespace Ffmpeg
{

class Arguments;

} // namespace Ffmpeg

void check(const std::vector<std::string> &ref, const Ffmpeg::Arguments &test);
