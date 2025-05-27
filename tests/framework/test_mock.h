#pragma once
#ifndef TEST_MOCK_H
#define TEST_MOCK_H

#include <functional>
#include <map>
#include <string>
#include <stdexcept>
#include <any>
#include <typeindex>

/**
 * @brief MockRegistry is a singleton class that manages function mocks.
 * 
 * This allows replacing function implementations at runtime for testing.
 */
class MockRegistry {
public:
    static MockRegistry& getInstance() {
        static MockRegistry instance;
        return instance;
    }

    // Register a mock function
    template<typename Func>
    void registerMock(const std::string& name, Func mockFunc) {
        mocks[name] = std::any(mockFunc);
        mockTypes[name] = std::type_index(typeid(Func));
    }

    // Get a mock function
    template<typename Func>
    Func getMock(const std::string& name) {
        auto it = mocks.find(name);
        if (it == mocks.end()) {
            throw std::runtime_error("Mock function not found: " + name);
        }
        
        auto typeIt = mockTypes.find(name);
        if (typeIt->second != std::type_index(typeid(Func))) {
            throw std::runtime_error("Type mismatch for mock function: " + name);
        }
        
        return std::any_cast<Func>(it->second);
    }

    // Check if a mock exists
    bool hasMock(const std::string& name) {
        return mocks.find(name) != mocks.end();
    }

    // Reset all mocks
    void reset() {
        mocks.clear();
        mockTypes.clear();
    }

private:
    MockRegistry() = default;
    ~MockRegistry() = default;
    
    // Deleted copy constructor and assignment operator
    MockRegistry(const MockRegistry&) = delete;
    MockRegistry& operator=(const MockRegistry&) = delete;

    std::map<std::string, std::any> mocks;
    std::map<std::string, std::type_index> mockTypes;
};

/**
 * @brief Mock class provides a simple interface to the MockRegistry.
 */
class Mock {
public:
    template<typename Func>
    static void when(const std::string& name, Func mockFunc) {
        MockRegistry::getInstance().registerMock(name, mockFunc);
    }

    template<typename Func>
    static Func get(const std::string& name) {
        return MockRegistry::getInstance().getMock<Func>(name);
    }

    static bool exists(const std::string& name) {
        return MockRegistry::getInstance().hasMock(name);
    }

    static void reset() {
        MockRegistry::getInstance().reset();
    }
};

// Macro to make mocking cleaner
#define MOCK_FUNCTION(returnType, name, ...) \
    if (Mock::exists(#name)) { \
        return Mock::get<std::function<returnType(__VA_ARGS__)>>(#name)(__VA_ARGS__); \
    }

#endif // TEST_MOCK_H