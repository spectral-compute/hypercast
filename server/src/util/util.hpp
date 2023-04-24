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

/**
 * Split a string into a fixed number of parts.
 *
 * @param string The string to split.
 * @param parts The string views to fill in with the components. Must not be empty.
 * @param separator The separator to use. Space by default.
 * @throws std::invalid_argument If the separator does not appear exactly `parts.size() - 1` times.
 */
void split(std::string_view string, std::initializer_list<std::reference_wrapper<std::string_view>> parts,
           char separator = ' ');

} // namespace Util

/// @}
