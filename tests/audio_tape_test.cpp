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


TEST_CASE("AudioTape playback - dynamic growth returns channel-major and zeros out of range", "[audio_tape][playback]") {
    const unsigned int frames_per_buffer = 4;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;

    AudioTape tape(frames_per_buffer, sample_rate, num_channels);

    // Record one channel-major frame at offset 0
    std::vector<float> frame1 = {
        // ch0
        1.f, 2.f, 3.f, 4.f,
        // ch1
        10.f, 20.f, 30.f, 40.f
    };
    tape.record(frame1.data(), std::optional<unsigned int>{});

    SECTION("Exact playback at start returns recorded samples in channel-major order") {
        auto out = tape.playback(frames_per_buffer, std::optional<unsigned int>{0});
        REQUIRE(out.size() == frames_per_buffer * num_channels);
        for (unsigned int i = 0; i < frames_per_buffer; ++i) {
            REQUIRE(out[i + 0 * frames_per_buffer] == Catch::Approx(frame1[i + 0 * frames_per_buffer]));
            REQUIRE(out[i + 1 * frames_per_buffer] == Catch::Approx(frame1[i + 1 * frames_per_buffer]));
        }
    }

    SECTION("Playback partially beyond end pads with zeros") {
        // Start at sample 2, request 4 => expect [2,3,0,0] per channel
        auto out = tape.playback(frames_per_buffer, std::optional<unsigned int>{2});

        REQUIRE(out[0] == Catch::Approx(3.f));
        REQUIRE(out[1] == Catch::Approx(4.f));
        REQUIRE(out[2] == Catch::Approx(0.f));
        REQUIRE(out[3] == Catch::Approx(0.f));

        REQUIRE(out[4 + 0] == Catch::Approx(30.f));
        REQUIRE(out[4 + 1] == Catch::Approx(40.f));
        REQUIRE(out[4 + 2] == Catch::Approx(0.f));
        REQUIRE(out[4 + 3] == Catch::Approx(0.f));
    }

    SECTION("Playback fully beyond end returns zeros") {
        auto out = tape.playback(frames_per_buffer, std::optional<unsigned int>{10});
        for (float v : out) {
            REQUIRE(v == Catch::Approx(0.f));
        }
    }

    SECTION("Interleaved playback at start returns frame-interleaved samples") {
        auto out = tape.playback(frames_per_buffer, std::optional<unsigned int>{0}, true /*interleaved*/);
        REQUIRE(out.size() == frames_per_buffer * num_channels);
        // Expect: [ch0[0], ch1[0], ch0[1], ch1[1], ...]
        REQUIRE(out[0] == Catch::Approx(1.f));
        REQUIRE(out[1] == Catch::Approx(10.f));
        REQUIRE(out[2] == Catch::Approx(2.f));
        REQUIRE(out[3] == Catch::Approx(20.f));
        REQUIRE(out[4] == Catch::Approx(3.f));
        REQUIRE(out[5] == Catch::Approx(30.f));
        REQUIRE(out[6] == Catch::Approx(4.f));
        REQUIRE(out[7] == Catch::Approx(40.f));
    }
}

TEST_CASE("AudioTape playback - fixed-size window respects sliding window", "[audio_tape][playback]") {
    const unsigned int frames_per_buffer = 4;
    const unsigned int sample_rate = 44100;
    const unsigned int num_channels = 2;
    const unsigned int capacity = 8; // per-channel capacity

    AudioTape tape(frames_per_buffer, sample_rate, num_channels, capacity);

    std::vector<float> frame = {
        // ch0
        5.f, 6.f, 7.f, 8.f,
        // ch1
        50.f, 60.f, 70.f, 80.f
    };

    const unsigned int write_offset = 6; // causes window shift; written at global 6..9
    tape.record(frame.data(), std::optional<unsigned int>{write_offset});

    SECTION("Playback at written region returns data") {
        auto out = tape.playback(frames_per_buffer, std::optional<unsigned int>{write_offset});
        for (unsigned int i = 0; i < frames_per_buffer; ++i) {
            REQUIRE(out[i] == Catch::Approx(frame[i + 0 * frames_per_buffer]));
            REQUIRE(out[frames_per_buffer + i] == Catch::Approx(frame[i + 1 * frames_per_buffer]));
        }
    }

    SECTION("Playback before current window returns zeros") {
        auto out = tape.playback(frames_per_buffer, std::optional<unsigned int>{0});
        for (float v : out) {
            REQUIRE(v == Catch::Approx(0.f));
        }
    }

    SECTION("Playback at unwritten indices within window returns zeros") {
        // Within window [2,10), indices 2..3 and 8..9 were never written; expect zeros
        auto out_head = tape.playback(2, std::optional<unsigned int>{2});
        REQUIRE(out_head.size() == 2 * num_channels);
        for (float v : out_head) {
            REQUIRE(v == Catch::Approx(0.f));
        }
    }

    SECTION("Interleaved playback at written region returns frame-interleaved samples") {
        auto out = tape.playback(frames_per_buffer, std::optional<unsigned int>{write_offset}, true /*interleaved*/);
        REQUIRE(out.size() == frames_per_buffer * num_channels);
        // Expect sequence: (5,50,6,60,7,70,8,80)
        REQUIRE(out[0] == Catch::Approx(5.f));
        REQUIRE(out[1] == Catch::Approx(50.f));
        REQUIRE(out[2] == Catch::Approx(6.f));
        REQUIRE(out[3] == Catch::Approx(60.f));
        REQUIRE(out[4] == Catch::Approx(7.f));
        REQUIRE(out[5] == Catch::Approx(70.f));
        REQUIRE(out[6] == Catch::Approx(8.f));
        REQUIRE(out[7] == Catch::Approx(80.f));
    }
}


// Parameterized tests across different channel counts and seconds-based offsets
using Ch1 = std::integral_constant<int, 1>;
using Ch2 = std::integral_constant<int, 2>;
using Ch3 = std::integral_constant<int, 3>;

TEMPLATE_TEST_CASE("AudioTape playback - parameterized channels and seconds vs samples",
                   "[audio_tape][playback][template]",
                   Ch1, Ch2, Ch3) {
    constexpr unsigned int BUFFER_SIZE = 4;   // frames_per_buffer
    constexpr unsigned int SAMPLE_RATE = 4;   // so that 0.5s -> 2 samples exactly
    constexpr unsigned int NUM_CHANNELS = TestType::value;

    AudioTape tape(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);

    // Prepare one channel-major frame with distinct values per channel and index
    std::vector<float> frame(NUM_CHANNELS * BUFFER_SIZE);
    for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
        for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
            frame[ch * BUFFER_SIZE + i] = static_cast<float>((ch + 1) * 100 + (i + 1));
        }
    }
    tape.record(frame.data(), std::optional<unsigned int>{});

    SECTION("Samples offset=1 returns shifted data and zero tail (channel-major)") {
        auto out = tape.playback(std::nullopt, std::optional<unsigned int>{1}, false);
        REQUIRE(out.size() == NUM_CHANNELS * BUFFER_SIZE);
        for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
            // expected: values at indices 1..3, then 0
            REQUIRE(out[ch * BUFFER_SIZE + 0] == Catch::Approx(static_cast<float>((ch + 1) * 100 + 2)));
            REQUIRE(out[ch * BUFFER_SIZE + 1] == Catch::Approx(static_cast<float>((ch + 1) * 100 + 3)));
            REQUIRE(out[ch * BUFFER_SIZE + 2] == Catch::Approx(static_cast<float>((ch + 1) * 100 + 4)));
            REQUIRE(out[ch * BUFFER_SIZE + 3] == Catch::Approx(0.f));
        }
    }

    SECTION("Samples offset=1 returns interleaved sequence when requested") {
        auto out = tape.playback(std::nullopt, std::optional<unsigned int>{1}, true);
        REQUIRE(out.size() == NUM_CHANNELS * BUFFER_SIZE);
        // Verify first two frames worth of interleaved samples
        // frame 0 at offset 1 -> index 1 per channel
        for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
            REQUIRE(out[0 * NUM_CHANNELS + ch] == Catch::Approx(static_cast<float>((ch + 1) * 100 + 2)));
        }
        // frame 1 at offset 2 -> index 2 per channel
        for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
            REQUIRE(out[1 * NUM_CHANNELS + ch] == Catch::Approx(static_cast<float>((ch + 1) * 100 + 3)));
        }
    }

    SECTION("Seconds offset=0.5s (==2 samples) returns shifted data and zero tail") {
        auto out = tape.playback(std::nullopt, std::optional<float>{0.5f}, false);
        REQUIRE(out.size() == NUM_CHANNELS * BUFFER_SIZE);
        for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
            // expected: indices 2..3 then 0,0
            REQUIRE(out[ch * BUFFER_SIZE + 0] == Catch::Approx(static_cast<float>((ch + 1) * 100 + 3)));
            REQUIRE(out[ch * BUFFER_SIZE + 1] == Catch::Approx(static_cast<float>((ch + 1) * 100 + 4)));
            REQUIRE(out[ch * BUFFER_SIZE + 2] == Catch::Approx(0.f));
            REQUIRE(out[ch * BUFFER_SIZE + 3] == Catch::Approx(0.f));
        }
    }
}


