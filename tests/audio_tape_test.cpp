// Focused unit tests for AudioTape record behavior

#include "catch2/catch_all.hpp"

#include "audio_render_stage/audio_render_stage_history.h"

#include <vector>

TEST_CASE("AudioTape record - dynamic growth and zero fill", "[audio_tape][record]") {
    const unsigned int frames_per_buffer = 4;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;

    AudioTape tape(frames_per_buffer, sample_rate, num_channels);

    // First write at default offset (0), channel-major input
    std::vector<float> frame1 = {
        // ch0
        1.f, 2.f, 3.f, 4.f,
        // ch1
        10.f, 20.f, 30.f, 40.f
    };

    tape.record(frame1.data(), std::optional<unsigned int>{});

    REQUIRE(tape.m_data.size() == num_channels);
    REQUIRE(tape.m_data[0].size() == frames_per_buffer);
    REQUIRE(tape.m_data[1].size() == frames_per_buffer);

    for (unsigned int i = 0; i < frames_per_buffer; ++i) {
        REQUIRE(tape.m_data[0][i] == Catch::Approx(frame1[i + 0 * frames_per_buffer]));
        REQUIRE(tape.m_data[1][i] == Catch::Approx(frame1[i + 1 * frames_per_buffer]));
    }

    // Second write with a forward gap; ensure zeros are inserted, and new data lands at offset
    std::vector<float> frame2 = {
        // ch0
        5.f, 6.f, 7.f, 8.f,
        // ch1
        50.f, 60.f, 70.f, 80.f
    };

    const unsigned int gap_offset = 8; // leaves a 4-sample zero gap between writes (indices 4..7)
    tape.record(frame2.data(), std::optional<unsigned int>{gap_offset});

    REQUIRE(tape.m_data[0].size() == gap_offset + frames_per_buffer);
    REQUIRE(tape.m_data[1].size() == gap_offset + frames_per_buffer);

    // Gap (4..7) must be zeros
    for (unsigned int i = frames_per_buffer; i < gap_offset; ++i) {
        REQUIRE(tape.m_data[0][i] == Catch::Approx(0.f));
        REQUIRE(tape.m_data[1][i] == Catch::Approx(0.f));
    }

    // New data at 8..11
    for (unsigned int i = 0; i < frames_per_buffer; ++i) {
        REQUIRE(tape.m_data[0][gap_offset + i] == Catch::Approx(frame2[i + 0 * frames_per_buffer]));
        REQUIRE(tape.m_data[1][gap_offset + i] == Catch::Approx(frame2[i + 1 * frames_per_buffer]));
    }
}

TEST_CASE("AudioTape record - fixed size window shifts and drops oldest", "[audio_tape][record]") {
    const unsigned int frames_per_buffer = 4;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;
    const unsigned int capacity = 8; // per-channel capacity

    AudioTape tape(frames_per_buffer, sample_rate, num_channels, capacity);

    // Write a block that extends beyond the current window end, forcing a forward shift of 2
    std::vector<float> frame = {
        // ch0
        1.f, 2.f, 3.f, 4.f,
        // ch1
        10.f, 20.f, 30.f, 40.f
    };

    const unsigned int write_offset = 6; // write_end = 10, causes shift by 2 for capacity=8
    tape.record(frame.data(), std::optional<unsigned int>{write_offset});

    // Capacity remains fixed
    REQUIRE(tape.m_data[0].size() == capacity);
    REQUIRE(tape.m_data[1].size() == capacity);

    // After shift, local write starts at index 4 and fills 4..7
    for (unsigned int i = 0; i < 4; ++i) {
        REQUIRE(tape.m_data[0][4 + i] == Catch::Approx(frame[i + 0 * frames_per_buffer]));
        REQUIRE(tape.m_data[1][4 + i] == Catch::Approx(frame[i + 1 * frames_per_buffer]));
    }

    // Older region (0..3) remains zero-filled
    for (unsigned int i = 0; i < 4; ++i) {
        REQUIRE(tape.m_data[0][i] == Catch::Approx(0.f));
        REQUIRE(tape.m_data[1][i] == Catch::Approx(0.f));
    }
}


