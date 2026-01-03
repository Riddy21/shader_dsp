#pragma once

#include <cmath>
#include <algorithm>
#include <type_traits>
#include <SDL2/SDL.h>

/**
 * SmoothValue - A template class for smooth visual transitions
 * 
 * Provides smooth animation between values while keeping the underlying target
 * value immediately accessible. The visual representation smoothly animates
 * to the target using spring-damper physics.
 * 
 * Template Parameters:
 *   T - Numeric type (float, int, double, etc.)
 * 
 * Usage:
 *   SmoothValue<float> rotation;
 *   rotation.set_target(3.14f);  // Immediate underlying change
 *   rotation.update(dt);          // Update visual value
 *   float visual = rotation.get_current();  // Smooth visual value
 *   float actual = rotation.get_target();   // Immediate underlying value
 */
template<typename T>
class SmoothValue {
    static_assert(std::is_arithmetic_v<T>, "SmoothValue requires an arithmetic type");

public:
    /**
     * Constructor
     * @param initial_value Initial value for both target and current
     * @param frequency Spring frequency (higher = faster response, default 8.0)
     * @param damping Damping factor (1.0 = critical damping, default 1.0)
     */
    explicit SmoothValue(
        T initial_value = T{},
        float frequency = 8.0f,
        float damping = 1.0f
    ) : m_target(initial_value),
        m_current(initial_value),
        m_velocity(0.0f),
        m_frequency(frequency),
        m_damping(damping),
        m_last_update_time_ms(0) {
    }

    /**
     * Set the target value (immediate underlying change)
     * The visual value will smoothly animate to this target
     */
    void set_target(T target) {
        m_target = target;
    }

    /**
     * Get the target value (immediate underlying value)
     * Use this for logic, calculations, and queries
     */
    T get_target() const {
        return m_target;
    }

    /**
     * Get the current visual value (smoothly animated)
     * Use this for rendering and visual representation
     */
    T get_current() const {
        return m_current;
    }

    /**
     * Update the smooth transition
     * Call this once per frame with delta time
     * @param dt Delta time in seconds (will be clamped for stability)
     */
    void update(float dt) {
        // Clamp dt to avoid instabilities from large frame time jumps
        dt = std::min(dt, 0.05f);

        // Convert to float for calculations (works for int types too)
        float current_f = static_cast<float>(m_current);
        float target_f = static_cast<float>(m_target);

        // Calculate difference
        float diff = target_f - current_f;

        // Spring-damper physics
        // Spring force: k * x - c * v
        // k = freq^2, c = 2 * damping * freq
        float accel = (m_frequency * m_frequency * diff) - 
                      (2.0f * m_damping * m_frequency * m_velocity);

        // Semi-implicit Euler integration for stability
        m_velocity += accel * dt;
        current_f += m_velocity * dt;
        m_current = static_cast<T>(current_f);

        // Snap to target if very close and slow (prevents jitter)
        const float snap_threshold = 0.001f;
        const float velocity_threshold = 0.01f;
        if (std::abs(diff) < snap_threshold && std::abs(m_velocity) < velocity_threshold) {
            m_current = m_target;
            m_velocity = 0.0f;
        }
    }

    /**
     * Update using SDL_GetTicks() for automatic time tracking
     * Call this once per frame (no need to pass dt manually)
     */
    void update() {
        Uint32 current_time = SDL_GetTicks();
        
        if (m_last_update_time_ms == 0) {
            // First frame, snap to target
            m_current = m_target;
            m_velocity = 0.0f;
            m_last_update_time_ms = current_time;
            return;
        }

        float dt = (current_time - m_last_update_time_ms) / 1000.0f;
        m_last_update_time_ms = current_time;
        
        update(dt);
    }

    /**
     * Instantly snap to target (no animation)
     * Useful for initialization or when instant change is needed
     */
    void snap_to_target() {
        m_current = m_target;
        m_velocity = 0.0f;
    }

    /**
     * Set spring frequency (higher = faster response)
     * @param frequency Spring frequency (default 8.0)
     */
    void set_frequency(float frequency) {
        m_frequency = std::max(0.1f, frequency);
    }

    /**
     * Set damping factor
     * @param damping 1.0 = critical damping (no overshoot), < 1.0 = underdamped (oscillates), > 1.0 = overdamped (slower)
     */
    void set_damping(float damping) {
        m_damping = std::max(0.1f, damping);
    }

    /**
     * Get current spring frequency
     */
    float get_frequency() const {
        return m_frequency;
    }

    /**
     * Get current damping factor
     */
    float get_damping() const {
        return m_damping;
    }

    /**
     * Check if the value has reached its target (within threshold)
     * @param threshold Maximum difference to consider "at target"
     */
    bool is_at_target(float threshold = 0.001f) const {
        float diff = std::abs(static_cast<float>(m_target) - static_cast<float>(m_current));
        return diff < threshold && std::abs(m_velocity) < 0.01f;
    }

    /**
     * Reset update timer (useful if component is hidden/shown)
     */
    void reset_timer() {
        m_last_update_time_ms = 0;
    }

    // Operator overloads for convenience
    SmoothValue& operator=(T value) {
        set_target(value);
        return *this;
    }

    operator T() const {
        return get_current();  // Default to visual value for convenience
    }

private:
    T m_target;              // Immediate underlying value
    T m_current;             // Smoothly animated visual value
    float m_velocity;        // Velocity for spring physics
    float m_frequency;       // Spring frequency
    float m_damping;         // Damping factor
    Uint32 m_last_update_time_ms;  // For automatic time tracking
};

// Specialization for integer types to avoid precision issues
template<>
class SmoothValue<int> {
public:
    explicit SmoothValue(
        int initial_value = 0,
        float frequency = 8.0f,
        float damping = 1.0f
    ) : m_target(initial_value),
        m_current_f(static_cast<float>(initial_value)),
        m_velocity(0.0f),
        m_frequency(frequency),
        m_damping(damping),
        m_last_update_time_ms(0) {
    }

    void set_target(int target) {
        m_target = target;
    }

    int get_target() const {
        return m_target;
    }

    int get_current() const {
        return static_cast<int>(std::round(m_current_f));
    }

    void update(float dt) {
        dt = std::min(dt, 0.05f);

        float target_f = static_cast<float>(m_target);
        float diff = target_f - m_current_f;

        float accel = (m_frequency * m_frequency * diff) - 
                      (2.0f * m_damping * m_frequency * m_velocity);

        m_velocity += accel * dt;
        m_current_f += m_velocity * dt;

        const float snap_threshold = 0.1f;  // Larger threshold for int
        const float velocity_threshold = 0.1f;
        if (std::abs(diff) < snap_threshold && std::abs(m_velocity) < velocity_threshold) {
            m_current_f = target_f;
            m_velocity = 0.0f;
        }
    }

    void update() {
        Uint32 current_time = SDL_GetTicks();
        
        if (m_last_update_time_ms == 0) {
            m_current_f = static_cast<float>(m_target);
            m_velocity = 0.0f;
            m_last_update_time_ms = current_time;
            return;
        }

        float dt = (current_time - m_last_update_time_ms) / 1000.0f;
        m_last_update_time_ms = current_time;
        update(dt);
    }

    void snap_to_target() {
        m_current_f = static_cast<float>(m_target);
        m_velocity = 0.0f;
    }

    void set_frequency(float frequency) {
        m_frequency = std::max(0.1f, frequency);
    }

    void set_damping(float damping) {
        m_damping = std::max(0.1f, damping);
    }

    float get_frequency() const { return m_frequency; }
    float get_damping() const { return m_damping; }

    bool is_at_target(float threshold = 0.1f) const {
        float diff = std::abs(static_cast<float>(m_target) - m_current_f);
        return diff < threshold && std::abs(m_velocity) < 0.1f;
    }

    void reset_timer() {
        m_last_update_time_ms = 0;
    }

    SmoothValue& operator=(int value) {
        set_target(value);
        return *this;
    }

    operator int() const {
        return get_current();
    }

private:
    int m_target;
    float m_current_f;  // Internal float representation for smooth animation
    float m_velocity;
    float m_frequency;
    float m_damping;
    Uint32 m_last_update_time_ms;
};

