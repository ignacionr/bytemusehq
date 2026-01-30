/**
 * Unit tests for the Config class.
 */

#include <gtest/gtest.h>
#include "config/config.h"

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the singleton instance
        config = &Config::Instance();
    }

    Config* config;
};

// Test that the singleton returns the same instance
TEST_F(ConfigTest, SingletonReturnsSameInstance) {
    Config& instance1 = Config::Instance();
    Config& instance2 = Config::Instance();
    EXPECT_EQ(&instance1, &instance2);
}

// Test default values for GetString
TEST_F(ConfigTest, GetStringReturnsDefaultForMissingKey) {
    wxString result = config->GetString("nonexistent.key", "default_value");
    EXPECT_EQ(result, "default_value");
}

// Test default values for GetInt
TEST_F(ConfigTest, GetIntReturnsDefaultForMissingKey) {
    int result = config->GetInt("nonexistent.key", 42);
    EXPECT_EQ(result, 42);
}

// Test default values for GetDouble
TEST_F(ConfigTest, GetDoubleReturnsDefaultForMissingKey) {
    double result = config->GetDouble("nonexistent.key", 3.14);
    EXPECT_DOUBLE_EQ(result, 3.14);
}

// Test default values for GetBool
TEST_F(ConfigTest, GetBoolReturnsDefaultForMissingKey) {
    bool result = config->GetBool("nonexistent.key", true);
    EXPECT_TRUE(result);
}

// Test Set and Get roundtrip for strings
TEST_F(ConfigTest, SetAndGetString) {
    config->Set("test.string.key", wxString("test_value"));
    wxString result = config->GetString("test.string.key", "default");
    EXPECT_EQ(result, "test_value");
}

// Test Set and Get roundtrip for integers
TEST_F(ConfigTest, SetAndGetInt) {
    config->Set("test.int.key", 123);
    int result = config->GetInt("test.int.key", 0);
    EXPECT_EQ(result, 123);
}

// Test Set and Get roundtrip for bools
TEST_F(ConfigTest, SetAndGetBool) {
    config->Set("test.bool.key", true);
    bool result = config->GetBool("test.bool.key", false);
    EXPECT_TRUE(result);
}

// Test config directory path is not empty
TEST_F(ConfigTest, ConfigDirPathNotEmpty) {
    wxString configDir = config->GetConfigDir();
    EXPECT_FALSE(configDir.IsEmpty());
}

// Test config file path is not empty and ends with config.json
TEST_F(ConfigTest, ConfigFilePathValid) {
    wxString configPath = config->GetConfigFilePath();
    EXPECT_FALSE(configPath.IsEmpty());
    EXPECT_TRUE(configPath.EndsWith("config.json"));
}
