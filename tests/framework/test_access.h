#pragma once
#ifndef TEST_ACCESS_H
#define TEST_ACCESS_H

#include <type_traits>
#include <utility>

/**
 * @brief TestAccess class provides access to private members of a class for testing purposes.
 * 
 * Usage:
 * - To access private member: TestAccess<MyClass>::get(instance, &MyClass::m_privateVariable)
 * - To access private method: TestAccess<MyClass>::call(instance, &MyClass::privateMethod, args...)
 */
template<typename Class>
class TestAccess {
public:
    // Access private member variables
    template<typename MemberType, typename InstanceType>
    static MemberType& get(InstanceType& instance, MemberType Class::* member) {
        return instance.*member;
    }

    // Access private member variables (const version)
    template<typename MemberType, typename InstanceType>
    static const MemberType& get(const InstanceType& instance, MemberType Class::* member) {
        return instance.*member;
    }

    // Call private member functions
    template<typename ReturnType, typename... Args, typename... CallArgs, typename InstanceType>
    static ReturnType call(InstanceType& instance, ReturnType (Class::*method)(Args...), CallArgs&&... args) {
        return (instance.*method)(std::forward<CallArgs>(args)...);
    }

    // Call private member functions (const version)
    template<typename ReturnType, typename... Args, typename... CallArgs, typename InstanceType>
    static ReturnType call(const InstanceType& instance, ReturnType (Class::*method)(Args...) const, CallArgs&&... args) {
        return (instance.*method)(std::forward<CallArgs>(args)...);
    }
};

#endif // TEST_ACCESS_H