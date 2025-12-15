#pragma once
#ifndef TEST_MAIN_H
#define TEST_MAIN_H

#include "catch2/catch_all.hpp"
#include <cstdlib>
#include <string>
#include <algorithm>

// This must be included once in each test file to reset mocks before each test
#define TEST_CASE_WITH_MOCKS(name, tags) \
    TEST_CASE(name, tags) { \
        struct MockResetter { \
            MockResetter() { } \
            ~MockResetter() { } \
        } mockResetter;

/**
 * @brief Check if audio output is enabled via ENABLE_AUDIO_OUTPUT environment variable
 * @return true if ENABLE_AUDIO_OUTPUT is set to "1" or "true" (case-insensitive), false otherwise
 */
inline bool is_audio_output_enabled() {
    const char* env = std::getenv("ENABLE_AUDIO_OUTPUT");
    if (!env) return false;
    std::string value(env);
    // Convert to lowercase for case-insensitive comparison
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == "1" || value == "true" || value == "yes";
}

/**
 * @brief Check if CSV output is enabled via ENABLE_CSV_OUTPUT environment variable
 * @return true if ENABLE_CSV_OUTPUT is set to "1" or "true" (case-insensitive), false otherwise
 */
inline bool is_csv_output_enabled() {
    const char* env = std::getenv("ENABLE_CSV_OUTPUT");
    if (!env) return false;
    std::string value(env);
    // Convert to lowercase for case-insensitive comparison
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == "1" || value == "true" || value == "yes";
}

#endif // TEST_MAIN_H