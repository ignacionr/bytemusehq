/**
 * Main entry point for ByteMuseHQ unit tests.
 * Uses Google Test framework.
 */

#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
