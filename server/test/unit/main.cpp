#include "data.hpp"

#include <gtest/gtest.h>

/**
 * The directory containing the test stuff.
 *
 * This is initialized by main(), below.
 */
std::filesystem::path testDir;

/**
 * A main() to capture argv[0].
 */
int main(int argc, char **argv)
{
    testDir = std::filesystem::absolute(std::filesystem::path(argv[0]).parent_path().parent_path());
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
