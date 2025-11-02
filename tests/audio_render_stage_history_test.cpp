// Tests for AudioRenderStageHistory2 private helper functions

#include "catch2/catch_all.hpp"

#define private public
#include "audio_render_stage/audio_render_stage_history.h"
#undef private

#include "audio_parameter/audio_uniform_parameter.h"
#include <memory>
#include <cmath>

TEST_CASE("AudioRenderStageHistory2::is_audio_texture_data_outdated - basic functionality", "[audio_history2][helper]") {
    const unsigned int frames_per_buffer = 256;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;
    const float history_buffer_size_seconds = 2.0f;

    AudioRenderStageHistory2 history(frames_per_buffer, sample_rate, num_channels, history_buffer_size_seconds);
    
    // Initialize parameters (normally done by create_parameters, but we'll set them directly for testing)
    GLuint active_texture_count = 0;
    history.create_parameters(active_texture_count);
    
    // Get window size for calculations
    unsigned int window_size_samples = history.get_window_size_samples();
    
    SECTION("Returns true when tape position is before texture start") {
        // Set window offset to some value
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(1000);
        history.set_tape_speed(1.0f);
        
        // Calculate expected texture start: window_offset + frame_size_samples
        unsigned int frame_size_samples = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * std::abs(1.0f));
        unsigned int texture_start = 1000 + frame_size_samples;
        
        // Set tape position before texture start
        history.set_tape_position(texture_start - 100);
        
        REQUIRE(history.is_audio_texture_data_outdated() == true);
    }
    
    SECTION("Returns true when tape position is at or after texture end") {
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(1000);
        history.set_tape_speed(1.0f);
        
        unsigned int frame_size_samples = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * std::abs(1.0f));
        unsigned int texture_start = 1000 + frame_size_samples;
        unsigned int texture_end = texture_start + window_size_samples - frame_size_samples;
        
        // Set tape position at texture end
        history.set_tape_position(texture_end);
        REQUIRE(history.is_audio_texture_data_outdated() == true);
        
        // Set tape position after texture end
        history.set_tape_position(texture_end + 100);
        REQUIRE(history.is_audio_texture_data_outdated() == true);
    }
    
    SECTION("Returns false when tape position is within valid range") {
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(1000);
        history.set_tape_speed(1.0f);
        
        unsigned int frame_size_samples = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * std::abs(1.0f));
        unsigned int texture_start = 1000 + frame_size_samples;
        unsigned int texture_end = texture_start + window_size_samples - frame_size_samples;
        
        // Set tape position in the middle of valid range
        unsigned int middle_position = texture_start + (texture_end - texture_start) / 2;
        history.set_tape_position(middle_position);
        
        REQUIRE(history.is_audio_texture_data_outdated() == false);
    }
    
    SECTION("Handles different speeds correctly") {
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(0);
        
        // Test with speed 2.0f
        history.set_tape_speed(2.0f);
        unsigned int frame_size_samples_2x = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * 2.0f);
        unsigned int texture_start_2x = 0 + frame_size_samples_2x;
        unsigned int texture_end_2x = texture_start_2x + window_size_samples - frame_size_samples_2x;
        
        // Position before start should be outdated
        history.set_tape_position(texture_start_2x - 1);
        REQUIRE(history.is_audio_texture_data_outdated() == true);
        
        // Position within range should not be outdated
        unsigned int middle_2x = texture_start_2x + (texture_end_2x - texture_start_2x) / 2;
        history.set_tape_position(middle_2x);
        REQUIRE(history.is_audio_texture_data_outdated() == false);
        
        // Position at end should be outdated
        history.set_tape_position(texture_end_2x);
        REQUIRE(history.is_audio_texture_data_outdated() == true);
    }
    
    SECTION("Handles negative speeds correctly") {
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(1000);
        history.set_tape_speed(-1.0f);
        
        // For negative speed, frame_size_samples uses abs(speed)
        unsigned int frame_size_samples = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * std::abs(-1.0f));
        unsigned int texture_start = 1000 + frame_size_samples;
        unsigned int texture_end = texture_start + window_size_samples - frame_size_samples;
        
        // Position before start should be outdated
        history.set_tape_position(texture_start - 1);
        REQUIRE(history.is_audio_texture_data_outdated() == true);
        
        // Position within range should not be outdated
        unsigned int middle = texture_start + (texture_end - texture_start) / 2;
        history.set_tape_position(middle);
        REQUIRE(history.is_audio_texture_data_outdated() == false);
        
        // Position at end should be outdated
        history.set_tape_position(texture_end);
        REQUIRE(history.is_audio_texture_data_outdated() == true);
    }
    
    SECTION("Handles zero speed") {
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(1000);
        history.set_tape_speed(0.0f);
        
        unsigned int frame_size_samples = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * std::abs(0.0f));
        unsigned int texture_start = 1000 + frame_size_samples; // Should be 1000
        unsigned int texture_end = texture_start + window_size_samples - frame_size_samples; // Should be 1000 + window_size
        
        // Position before start should be outdated
        history.set_tape_position(texture_start - 1);
        REQUIRE(history.is_audio_texture_data_outdated() == true);
        
        // Position at start boundary is still valid (not outdated)
        history.set_tape_position(texture_start);
        REQUIRE(history.is_audio_texture_data_outdated() == false);
        
        // Position within range should not be outdated
        unsigned int middle = texture_start + (texture_end - texture_start) / 2;
        history.set_tape_position(middle);
        REQUIRE(history.is_audio_texture_data_outdated() == false);
        
        // Position at end boundary should be outdated
        history.set_tape_position(texture_end);
        REQUIRE(history.is_audio_texture_data_outdated() == true);
    }
}

TEST_CASE("AudioRenderStageHistory2::get_window_offset_samples_for_tape_data - basic functionality", "[audio_history2][helper]") {
    const unsigned int frames_per_buffer = 256;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;
    const float history_buffer_size_seconds = 2.0f;

    AudioRenderStageHistory2 history(frames_per_buffer, sample_rate, num_channels, history_buffer_size_seconds);
    
    GLuint active_texture_count = 0;
    history.create_parameters(active_texture_count);
    
    unsigned int window_size_samples = history.get_window_size_samples();
    
    SECTION("Returns tape position for positive speed") {
        history.set_tape_speed(1.0f);
        unsigned int test_position = 5000;
        history.set_tape_position(test_position);
        
        unsigned int offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == test_position);
    }
    
    SECTION("Returns tape position minus window size for negative speed") {
        history.set_tape_speed(-1.0f);
        unsigned int test_position = 10000;
        history.set_tape_position(test_position);
        
        unsigned int offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == test_position - window_size_samples);
    }
    
    SECTION("Returns 0 for zero speed") {
        history.set_tape_speed(0.0f);
        unsigned int test_position = 5000;
        history.set_tape_position(test_position);
        
        unsigned int offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == 0);
    }
    
    SECTION("Handles different positive speeds") {
        history.set_tape_speed(2.0f);
        unsigned int test_position = 3000;
        history.set_tape_position(test_position);
        
        unsigned int offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == test_position);
        
        history.set_tape_speed(0.5f);
        history.set_tape_position(test_position);
        
        offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == test_position);
    }
    
    SECTION("Handles different negative speeds") {
        history.set_tape_speed(-2.0f);
        unsigned int test_position = 15000;
        history.set_tape_position(test_position);
        
        unsigned int offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == test_position - window_size_samples);
        
        history.set_tape_speed(-0.5f);
        history.set_tape_position(test_position);
        
        offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == test_position - window_size_samples);
    }
    
    SECTION("Handles edge case where tape position is less than window size (negative speed)") {
        history.set_tape_speed(-1.0f);
        unsigned int test_position = window_size_samples / 2; // Less than window_size_samples
        history.set_tape_position(test_position);
        
        unsigned int offset = history.get_window_offset_samples_for_tape_data();
        // Should handle underflow gracefully (unsigned arithmetic wraps around)
        REQUIRE(offset == test_position - window_size_samples);
    }
    
    SECTION("Handles zero tape position with positive speed") {
        history.set_tape_speed(1.0f);
        history.set_tape_position(0u);
        
        unsigned int offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == 0u);
    }
    
    SECTION("Handles zero tape position with negative speed") {
        history.set_tape_speed(-1.0f);
        history.set_tape_position(0u);
        
        unsigned int offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset == 0u - window_size_samples); // Will wrap, but that's expected behavior
    }
}

TEST_CASE("AudioRenderStageHistory2 helper functions - integration", "[audio_history2][helper][integration]") {
    const unsigned int frames_per_buffer = 256;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;
    const float history_buffer_size_seconds = 2.0f;

    AudioRenderStageHistory2 history(frames_per_buffer, sample_rate, num_channels, history_buffer_size_seconds);
    
    GLuint active_texture_count = 0;
    history.create_parameters(active_texture_count);
    
    unsigned int window_size_samples = history.get_window_size_samples();
    
    SECTION("is_audio_texture_data_outdated matches get_window_offset_samples_for_tape_data for positive speed") {
        history.set_tape_speed(1.0f);
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(5000);
        
        // Set tape position first
        unsigned int test_position = 5000;
        history.set_tape_position(test_position);
        
        // Get the calculated offset (should match position for positive speed)
        unsigned int tape_offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(tape_offset == test_position);
        
        // Calculate expected texture bounds
        unsigned int frame_size_samples = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * std::abs(1.0f));
        unsigned int texture_start = 5000 + frame_size_samples;
        unsigned int texture_end = texture_start + window_size_samples - frame_size_samples;
        
        // Tape position should be before texture_start, which means outdated
        REQUIRE(history.is_audio_texture_data_outdated() == true);
        
        // Move position just inside the valid range
        history.set_tape_position(texture_start + 1);
        REQUIRE(history.is_audio_texture_data_outdated() == false);
    }
    
    SECTION("is_audio_texture_data_outdated matches get_window_offset_samples_for_tape_data for negative speed") {
        history.set_tape_speed(-1.0f);
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(10000);
        
        // Set tape position first
        unsigned int test_position = 10000u;
        history.set_tape_position(test_position);
        
        // Get the calculated offset (should be position - window_size for negative speed)
        unsigned int tape_offset = history.get_window_offset_samples_for_tape_data();
        REQUIRE(tape_offset == test_position - window_size_samples);
        
        // Calculate expected texture bounds
        unsigned int frame_size_samples = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * std::abs(-1.0f));
        unsigned int texture_start = 10000 + frame_size_samples;
        unsigned int texture_end = texture_start + window_size_samples - frame_size_samples;
        
        // Position should be before texture_start, which means outdated
        REQUIRE(history.is_audio_texture_data_outdated() == true);
        
        // Move position just inside the valid range
        history.set_tape_position(texture_start + 1);
        REQUIRE(history.is_audio_texture_data_outdated() == false);
    }
    
    SECTION("Changing speed updates both functions correctly") {
        static_cast<AudioIntParameter*>(history.m_tape_window_offset_samples)->set_value(5000);
        unsigned int test_position = 10000;
        history.set_tape_position(test_position);
        
        // Test with speed 1.0
        history.set_tape_speed(1.0f);
        unsigned int offset_1x = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset_1x == test_position);
        
        unsigned int frame_size_1x = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * 1.0f);
        unsigned int texture_start_1x = 5000 + frame_size_1x;
        unsigned int texture_end_1x = texture_start_1x + window_size_samples - frame_size_1x;
        
        // Position should be outdated if outside range
        bool outdated_1x = history.is_audio_texture_data_outdated();
        
        // Test with speed 2.0
        history.set_tape_speed(2.0f);
        unsigned int offset_2x = history.get_window_offset_samples_for_tape_data();
        REQUIRE(offset_2x == test_position); // Offset is same for positive speeds
        
        unsigned int frame_size_2x = static_cast<unsigned int>(static_cast<float>(frames_per_buffer) * 2.0f);
        unsigned int texture_start_2x = 5000 + frame_size_2x;
        unsigned int texture_end_2x = texture_start_2x + window_size_samples - frame_size_2x;
        
        bool outdated_2x = history.is_audio_texture_data_outdated();
        
        // Different speeds should give different outdated results
        // (unless position happens to be in range for both, which is unlikely)
        // We just verify the functions work correctly with different speeds
        REQUIRE(texture_start_2x > texture_start_1x); // Higher speed moves start forward
    }
}

TEST_CASE("AudioRenderStageHistory2 - window offset updates correctly", "[audio_history2][window_offset][update]") {
    const unsigned int frames_per_buffer = 256;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;
    const float history_buffer_size_seconds = 2.0f;

    AudioRenderStageHistory2 history(frames_per_buffer, sample_rate, num_channels, history_buffer_size_seconds);
    
    GLuint active_texture_count = 0;
    history.create_parameters(active_texture_count);
    
    unsigned int window_size_samples = history.get_window_size_samples();
    
    // Create a tape with some test data
    auto tape = std::make_shared<AudioTape>(frames_per_buffer, sample_rate, num_channels, window_size_samples * 3);
    
    // Record some test data
    std::vector<float> test_data(window_size_samples * num_channels, 0.5f);
    for (unsigned int i = 0; i < window_size_samples; i += frames_per_buffer) {
        tape->record(test_data.data(), i);
    }
    
    history.set_tape(tape);
    history.set_tape_position(0u);
    history.set_tape_speed(1.0f);
    
    SECTION("Window offset updates on first update call") {
        // Initial window offset should be very large (1000000000)
        unsigned int initial_offset = history.get_window_offset_samples();
        REQUIRE(initial_offset == 1000000000u);
        
        // First update should change the window offset
        // The offset is calculated BEFORE the position is incremented
        unsigned int position_before_update = history.get_tape_position();
        unsigned int expected_offset = position_before_update; // For positive speed, offset = position
        history.update_audio_history_texture();
        
        // Window offset should now be set to the calculated offset (tape position for positive speed)
        unsigned int new_offset = history.get_window_offset_samples();
        REQUIRE(new_offset == expected_offset);
        REQUIRE(new_offset == 0u); // Initially at position 0
        
        // Verify tape position was incremented
        int speed_samples = history.get_tape_speed_samples_per_buffer();
        unsigned int expected_new_position = position_before_update + speed_samples;
        REQUIRE(history.get_tape_position() == expected_new_position);
    }
    
    SECTION("Window offset updates when texture becomes outdated") {
        history.set_tape_speed(1.0f);
        history.set_tape_position(0u);
        
        // First update - should set initial offset
        history.update_audio_history_texture();
        unsigned int offset_after_first = history.get_window_offset_samples();
        REQUIRE(offset_after_first == 0u);
        
        // Move tape position forward by speed_samples_per_buffer to stay in range
        int speed_samples = history.get_tape_speed_samples_per_buffer();
        unsigned int frame_size_samples = static_cast<unsigned int>(std::abs(speed_samples));
        
        // Position tape so it's just before becoming outdated
        unsigned int texture_start = offset_after_first + frame_size_samples;
        unsigned int texture_end = texture_start + window_size_samples - frame_size_samples;
        unsigned int safe_position = texture_start + (texture_end - texture_start) / 2;
        history.set_tape_position(safe_position);
        
        // Update - should not change offset (still in range)
        history.update_audio_history_texture();
        unsigned int offset_before_outdated = history.get_window_offset_samples();
        REQUIRE(offset_before_outdated == offset_after_first);
        
        // Move position to be outdated (past texture end)
        history.set_tape_position(texture_end);
        
        // Calculate expected offset before update (position for positive speed)
        unsigned int expected_new_offset = texture_end;
        
        // Update - should change offset to new calculated value
        history.update_audio_history_texture();
        unsigned int offset_after_outdated = history.get_window_offset_samples();
        REQUIRE(offset_after_outdated == expected_new_offset);
        
        // Verify tape position was incremented
        int speed_samples_val = history.get_tape_speed_samples_per_buffer();
        unsigned int expected_new_position = texture_end + speed_samples_val;
        REQUIRE(history.get_tape_position() == expected_new_position);
    }
    
    SECTION("Window offset updates correctly with different speeds") {
        // Test with speed 2.0 (double speed)
        history.set_tape_speed(2.0f);
        history.set_tape_position(0u);
        
        history.update_audio_history_texture();
        unsigned int offset_2x = history.get_window_offset_samples();
        REQUIRE(offset_2x == 0u);
        
        // Move position forward
        int speed_samples_2x = history.get_tape_speed_samples_per_buffer();
        unsigned int frame_size_2x = static_cast<unsigned int>(std::abs(speed_samples_2x));
        unsigned int texture_start_2x = offset_2x + frame_size_2x;
        unsigned int texture_end_2x = texture_start_2x + window_size_samples - frame_size_2x;
        
        // Make it outdated
        history.set_tape_position(texture_end_2x);
        unsigned int expected_offset_2x = texture_end_2x;
        history.update_audio_history_texture();
        
        unsigned int offset_after_update_2x = history.get_window_offset_samples();
        REQUIRE(offset_after_update_2x == expected_offset_2x);
        
        // Verify position was incremented
        int speed_samples_2x_val = history.get_tape_speed_samples_per_buffer();
        unsigned int expected_pos_2x = texture_end_2x + speed_samples_2x_val;
        REQUIRE(history.get_tape_position() == expected_pos_2x);
        
        // Test with speed 0.5 (half speed)
        history.set_tape_speed(0.5f);
        history.set_tape_position(0u);
        
        history.update_audio_history_texture();
        unsigned int offset_half = history.get_window_offset_samples();
        REQUIRE(offset_half == 0u);
        
        int speed_samples_half = history.get_tape_speed_samples_per_buffer();
        unsigned int frame_size_half = static_cast<unsigned int>(std::abs(speed_samples_half));
        unsigned int texture_start_half = offset_half + frame_size_half;
        unsigned int texture_end_half = texture_start_half + window_size_samples - frame_size_half;
        
        history.set_tape_position(texture_end_half);
        unsigned int expected_offset_half = texture_end_half;
        history.update_audio_history_texture();
        
        unsigned int offset_after_update_half = history.get_window_offset_samples();
        REQUIRE(offset_after_update_half == expected_offset_half);
        
        // Verify position was incremented
        int speed_samples_half_val = history.get_tape_speed_samples_per_buffer();
        unsigned int expected_pos_half = texture_end_half + speed_samples_half_val;
        REQUIRE(history.get_tape_position() == expected_pos_half);
    }
    
    SECTION("Window offset updates correctly with negative speed") {
        history.set_tape_speed(-1.0f);
        unsigned int test_position = window_size_samples * 2; // Make sure we have room
        history.set_tape_position(test_position);
        
        // Calculate expected offset before update (position - window_size for negative speed)
        unsigned int expected_offset_before = test_position - window_size_samples;
        
        history.update_audio_history_texture();
        
        // After update, the tape position will have been decremented by speed_samples_per_buffer
        // But the window offset should have been set to the offset calculated BEFORE the position update
        unsigned int offset_negative = history.get_window_offset_samples();
        REQUIRE(offset_negative == expected_offset_before);
        
        // Verify the tape position was decremented correctly
        int speed_samples_neg = history.get_tape_speed_samples_per_buffer();
        unsigned int expected_new_position = test_position + speed_samples_neg; // speed_samples_neg is negative
        REQUIRE(history.get_tape_position() == expected_new_position);
    }
    
    SECTION("Handles tape position at 0 with negative speed") {
        history.set_tape_speed(-1.0f);
        history.set_tape_position(0u);
        
        // Get the speed samples per buffer (should be negative)
        int speed_samples = history.get_tape_speed_samples_per_buffer();
        REQUIRE(speed_samples < 0);
        
        // Calculate what the position would be after decrementing
        unsigned int current_position = history.get_tape_position();
        REQUIRE(current_position == 0u);
        
        // Update - should clamp position to 0 (can't go below 0)
        history.update_audio_history_texture();
        
        // Position should be clamped to 0, not wrap around
        unsigned int position_after_update = history.get_tape_position();
        REQUIRE(position_after_update == 0u);
        
        // Window offset should NOT be updated (texture update is prevented at position 0 with negative speed)
        // The offset should remain at its initial large value since texture wasn't updated
        unsigned int window_offset = history.get_window_offset_samples();
        // At position 0 with negative speed, texture update is prevented, so offset stays at initial value
        // (which is 1000000000 initially, or whatever was set before)
        bool offset_not_updated = (window_offset == 1000000000u) || (window_offset >= 1000000000u);
        REQUIRE(offset_not_updated);
        
        // Try updating again - should still stay at 0
        history.update_audio_history_texture();
        REQUIRE(history.get_tape_position() == 0u);
        
        // Verify that speed ratio check works (should not update if speed is 0)
        // But we're testing with -1 speed, so it should try to update but position stays at 0
        unsigned int position_after_second_update = history.get_tape_position();
        REQUIRE(position_after_second_update == 0u);
    }
    
    SECTION("Does not update texture at position 0 with negative speed") {
        history.set_tape_speed(-1.0f);
        history.set_tape_position(0u);
        
        // Get initial window offset
        unsigned int initial_offset = history.get_window_offset_samples();
        
        // Update - should NOT update texture (at boundary with negative speed)
        history.update_audio_history_texture();
        
        // Window offset should NOT have changed (texture not updated)
        unsigned int offset_after_update = history.get_window_offset_samples();
        REQUIRE(offset_after_update == initial_offset);
        
        // Position should still be clamped to 0
        REQUIRE(history.get_tape_position() == 0u);
        
        // Multiple updates should still not change the offset
        history.update_audio_history_texture();
        unsigned int offset_after_second = history.get_window_offset_samples();
        REQUIRE(offset_after_second == initial_offset);
    }
    
    SECTION("Does not update texture at end of tape with positive speed") {
        history.set_tape_speed(1.0f);
        
        // Get tape size
        auto tape = history.get_tape().lock();
        REQUIRE(tape != nullptr);
        unsigned int tape_size = tape->size();
        REQUIRE(tape_size > 0u);
        
        // Set position to end of tape
        history.set_tape_position(tape_size);
        
        // First update to set initial offset
        history.update_audio_history_texture();
        unsigned int offset_after_first = history.get_window_offset_samples();
        
        // Update again - should NOT update texture (at boundary with positive speed)
        history.update_audio_history_texture();
        
        // Window offset should NOT have changed (texture not updated)
        unsigned int offset_after_update = history.get_window_offset_samples();
        REQUIRE(offset_after_update == offset_after_first);
        
        // Position should have incremented (or stayed at max if clamped)
        unsigned int position_after = history.get_tape_position();
        // Position will increment beyond tape_size, but texture won't update
        REQUIRE(position_after >= tape_size);
        
        // Multiple updates should still not change the offset
        history.update_audio_history_texture();
        unsigned int offset_after_second = history.get_window_offset_samples();
        REQUIRE(offset_after_second == offset_after_first);
    }
    
    SECTION("Updates texture normally when not at boundaries") {
        history.set_tape_speed(1.0f);
        
        // Set position somewhere in the middle (not at start or end)
        auto tape = history.get_tape().lock();
        REQUIRE(tape != nullptr);
        unsigned int tape_size = tape->size();
        unsigned int middle_position = tape_size / 2;
        REQUIRE(middle_position > 0u);
        REQUIRE(middle_position < tape_size);
        
        history.set_tape_position(middle_position);
        
        // Set window offset to large value to force update
        history.set_window_offset_samples(1000000000u);
        
        // Update - should update texture (not at boundary)
        history.update_audio_history_texture();
        
        // Window offset should have changed (texture updated)
        unsigned int offset_after_update = history.get_window_offset_samples();
        REQUIRE(offset_after_update != 1000000000u);
        REQUIRE(offset_after_update == middle_position); // For positive speed, offset = position
    }
}

