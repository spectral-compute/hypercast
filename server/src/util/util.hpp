#include <filesystem>
#include <vector>

/**
 * @defgroup util Miscellaneous utilities
 */
/// @addtogroup util
/// @{

/**
 * Miscellaneous utilities.
 */
namespace Util
{

/**
 * Concatenate vectors of vectors of bytes.
 */
std::vector<std::byte> concatenate(std::vector<std::vector<std::byte>> dataParts);

/**
 * Synchronously read the contents of a file.
 */
std::vector<std::byte> readFile(const std::filesystem::path &path);

/**
 * Replace all instances of a string with another string in a source string.
 */
std::string replaceAll(std::string_view string, std::string_view token, std::string_view replacement);

} // namespace util

/// @}
