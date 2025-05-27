#pragma once
#ifndef TEST_MAIN_H
#define TEST_MAIN_H

#include "catch2/catch_all.hpp"
#include "tests/framework/test_access.h"
#include "tests/framework/test_mock.h"

// This must be included once in each test file to reset mocks before each test
#define TEST_CASE_WITH_MOCKS(name, tags) \
    TEST_CASE(name, tags) { \
        struct MockResetter { \
            MockResetter() { Mock::reset(); } \
            ~MockResetter() { Mock::reset(); } \
        } mockResetter; \

#endif // TEST_MAIN_H