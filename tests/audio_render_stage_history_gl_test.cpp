// Mock-stage focused test for AudioRenderStageHistory without chaining to final output

#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "framework/csv_test_output.h"
#include "framework/test_main.h"

#define private public
#include "audio_render_stage_plugins/audio_render_stage_history.h"
#include "audio_core/audio_tape.h"
#undef private

#include "audio_core/audio_render_stage.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"

#include "audio_render_stage/audio_final_render_stage.h"
#include "tests/utils/audio_test_utils.h"
#include "audio_output/audio_player_output.h"
#include "audio_output/audio_file_output.h"
#include "graphics_core/graphics_display.h"
#include "graphics_views/debug_view.h"
#include <vector>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <iomanip>
#include <map>
#include <sstream>
#include <filesystem>

struct TestParams { int buffer_size; int num_channels; const char* name; };
using TestParam1 = std::integral_constant<int, 0>; // 256 x 2
using TestParam2 = std::integral_constant<int, 1>; // 512 x 2

constexpr TestParams get_test_params(int i) {
	constexpr TestParams P[] = { {256,2,"256x2"}, {512,2,"512x2"} };
	return P[i];
}

struct PlaybackTestParams { 
	int buffer_size; 
	int num_channels; 
	float speed; 
	const char* name; 
};

using PlaybackTestParam1 = std::integral_constant<int, 0>; // 256x1, 1.0x
using PlaybackTestParam2 = std::integral_constant<int, 1>; // 256x1, 0.5x
using PlaybackTestParam3 = std::integral_constant<int, 2>; // 256x2, 1.0x
using PlaybackTestParam4 = std::integral_constant<int, 3>; // 256x2, 0.5x
using PlaybackTestParam5 = std::integral_constant<int, 4>; // 512x3, 1.0x
using PlaybackTestParam6 = std::integral_constant<int, 5>; // 512x4, 1.0x

constexpr PlaybackTestParams get_playback_test_params(int i) {
	constexpr PlaybackTestParams P[] = { 
		{256, 1, 1.0f, "256x1_1.0x"},
		{256, 1, -0.5f, "256x1_0.5x"},
		{256, 2, 1.6f, "256x2_1.0x"},
		{256, 2, -0.3f, "256x2_0.5x"},
		{512, 3, 1.0f, "512x3_1.0x"},
		{512, 4, 1.5f, "512x4_1.0x"}
	};
	return P[i];
}

// Fragment shader for tape playback
static const char* kTapePlaybackFragSource = R"(
void main(){
    // Get the audio sample from tape history using TexCoord
	vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    // The function will use tape_position and tape_speed internally
    vec4 tape_sample = get_tape_history_samples(TexCoord);
    
    // Output the tape playback sample
    output_audio_texture = tape_sample + stream_audio;
    debug_audio_texture = output_audio_texture;
}
)";

class MockTapePlaybackStage : public AudioRenderStage {
public:
	MockTapePlaybackStage(unsigned int frames_per_buffer,
	                     unsigned int sample_rate,
	                     unsigned int num_channels,
	                     float window_seconds = 2.0f)
	: AudioRenderStage(frames_per_buffer, sample_rate, num_channels,
	                   kTapePlaybackFragSource,
	                   true, // use_shader_string
	                   std::vector<std::string>{
	                   	"build/shaders/global_settings.glsl",
	                   	"build/shaders/frag_shader_settings.glsl",
	                   	"build/shaders/tape_history_settings.glsl"}),
	  m_is_playing(false)
	{
		// Create tape history and all its textures/parameters
		m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, window_seconds);
		m_history2->create_parameters(m_active_texture_count);
		
		// Add all parameters
		auto params = m_history2->get_parameters();
		for (auto* param : params) {
			this->add_parameter(param);
		}
	}
	
	AudioRenderStageHistory2& get_history() { return *m_history2; }
	
	void play() { m_is_playing = true; }
	void stop() { m_is_playing = false; }
	bool is_playing() const { return m_is_playing; }

protected:
	void render(const unsigned int time) override {

		m_history2->update_audio_history_texture(time);
		AudioRenderStage::render(time);
	}

private:
	std::unique_ptr<AudioRenderStageHistory2> m_history2;
	bool m_is_playing;
	unsigned int m_current_frame;
};


TEMPLATE_TEST_CASE("AudioRenderStageHistory2 - record and playback with audio output", "[audio_history2][playback][audio_output][gl_test][template]",
			   PlaybackTestParam1, PlaybackTestParam2, PlaybackTestParam3, PlaybackTestParam4, PlaybackTestParam5, PlaybackTestParam6) {

	constexpr auto P = get_playback_test_params(TestType::value);
	constexpr int BUFFER_SIZE = P.buffer_size;
	constexpr int NUM_CHANNELS = P.num_channels;
	constexpr float PLAYBACK_SPEED = P.speed;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float BASE_AMPLITUDE = 0.2f; // Base amplitude, increases by 0.2 per channel
	constexpr int RECORD_DURATION_SECONDS = 8;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 2;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	
	// Create final render stage
	AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	
	// Connect playback stage to final stage
	REQUIRE(playback_stage.connect_render_stage(&final_stage));
	
	// Initialize stages
	REQUIRE(playback_stage.initialize());
	REQUIRE(final_stage.initialize());
	
	context.prepare_draw();
	REQUIRE(playback_stage.bind());
	REQUIRE(final_stage.bind());

	
	SECTION("Record sine wave and playback at different speeds with audio output") {
		// Collect input sine wave data per channel
		std::vector<std::vector<float>> input_samples_per_channel(NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			input_samples_per_channel[ch].reserve(SAMPLE_RATE * RECORD_DURATION_SECONDS);
		}
		
		// Record sine wave to tape - generate per channel with increasing amplitudes
		std::vector<float> phases(NUM_CHANNELS, 0.0f);
		for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
			// Generate sine wave per channel with different amplitudes
			// Channel 0: 0.2, Channel 1: 0.4, Channel 2: 0.6, Channel 3: 0.8, etc.
			std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				float channel_amplitude = BASE_AMPLITUDE * (ch + 1);
				// Generate sine wave for this specific channel
				auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
				//auto sine_buffer = generate_constant_buffer(channel_amplitude, BUFFER_SIZE, 1);
				// Copy to channel-major buffer (sine_buffer is interleaved but has only 1 channel)
				for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
					channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
				}
				phases[ch] += BUFFER_SIZE;
			}
			
			// Collect input samples (de-interleave from channel-major format)
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
					input_samples_per_channel[ch].push_back(channel_major_buffer[ch * BUFFER_SIZE + i]);
				}
			}
			
			tape->record(channel_major_buffer.data());
		}
		
		REQUIRE(tape->size() > 0);
		
		// Setup audio output (only if enabled)
		AudioPlayerOutput* audio_output = nullptr;
		if (is_audio_output_enabled()) {
			audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
			REQUIRE(audio_output->open());
			REQUIRE(audio_output->start());
		}
		
		// Initialize output sample vectors for this speed (per channel)
		std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
		}
		
		// Configure playback at the parameterized speed
		playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
		auto middle_position = tape->size() / 2;
		playback_stage.get_history().set_tape_position(middle_position);
		playback_stage.play();

		// Check the speed setting
		int speed_samples_per_buffer = *static_cast<const int*>(playback_stage.get_history().m_tape_speed->get_value());
		REQUIRE(static_cast<float>(speed_samples_per_buffer) == Catch::Approx(PLAYBACK_SPEED * BUFFER_SIZE).margin(1.00f));
		
		// Render and play audio
		std::vector<float> recorded_output;
		recorded_output.reserve(BUFFER_SIZE * NUM_PLAYBACK_FRAMES * NUM_CHANNELS);
		
		unsigned int frame_count = 0;
		const unsigned int max_frames = NUM_PLAYBACK_FRAMES;
		
		for (int frame = 0; frame < max_frames; ++frame) {
			global_time->set_value(frame);
			global_time->render();

			// Render playback stage (updates tape history texture)
			playback_stage.render(frame);
			
			// Render final stage
			final_stage.render(frame);
			
			// Get output from final stage
			auto output_param = final_stage.find_parameter("final_output_audio_texture");
			REQUIRE(output_param != nullptr);
			
			const float* output_data = static_cast<const float*>(output_param->get_value());
			REQUIRE(output_data != nullptr);
			
			// Store for verification
			// NOTE: final_output_audio_texture from AudioFinalRenderStage IS interleaved
			// Layout: [frame0_ch0, frame0_ch1, ..., frame0_chN, frame1_ch0, frame1_ch1, ..., frame1_chN, ...]
			// De-interleave to separate channels for CSV export
			for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
				auto channel = i % NUM_CHANNELS;
				output_samples_per_channel[channel].push_back(output_data[i]);
			}
			
			// Push to audio output (output_data is interleaved format)
			if (audio_output) {
				while (!audio_output->is_ready()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				audio_output->push(output_data);
			}
			
			frame_count++;
			
			// Stop playback if we've reached the end of the tape
			if (playback_stage.get_history().get_tape_position() >= tape->size()) {
				playback_stage.stop();
				printf("Playback complete with speed %f\n", PLAYBACK_SPEED);
				break;
			}
		}
		// Stop playback
		playback_stage.stop();
		
		// Wait for audio to finish playing and close audio output
		if (audio_output) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			audio_output->close();
			delete audio_output;
		}
		
		if (is_csv_output_enabled()) {
			SECTION("Write input sine wave to CSV") {
				std::ofstream csv_file("input_sine_wave.csv");
				REQUIRE(csv_file.is_open());
				
				// Write header
				csv_file << "sample_index,time_seconds";
				for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
					csv_file << ",channel_" << ch;
				}
				csv_file << std::endl;
				
				// Write data (all channels)
				unsigned int num_samples = input_samples_per_channel[0].size();
				for (unsigned int i = 0; i < num_samples; ++i) {
					double time_seconds = static_cast<double>(i) / SAMPLE_RATE;
					csv_file << i << "," << std::fixed << std::setprecision(9) << time_seconds;
					for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
						csv_file << "," << input_samples_per_channel[ch][i];
					}
					csv_file << std::endl;
				}
				
				csv_file.close();
				std::cout << "Wrote input sine wave to input_sine_wave.csv (" << num_samples << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
			}
		}
		
		if (is_csv_output_enabled()) {
			SECTION("Write output audio to CSV") {
				// Verify we have the correct number of channels before writing
				REQUIRE(output_samples_per_channel.size() == NUM_CHANNELS);
				
				// Create filename with speed and channels (e.g., "output_audio_speed_1.000000_channels_3.csv")
				std::ostringstream filename_stream;
				filename_stream << "output_audio_speed_" << std::fixed << std::setprecision(6) << PLAYBACK_SPEED 
				               << "_channels_" << NUM_CHANNELS << ".csv";
				std::string filename = filename_stream.str();
				
				// Use CSV framework utility for consistent format
				CSVTestOutput csv_writer(filename, SAMPLE_RATE);
				REQUIRE(csv_writer.is_open());
				
				// Verify all channels have data before writing
				for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
					REQUIRE(output_samples_per_channel[ch].size() > 0);
				}
				
				csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
				csv_writer.close();
				
				std::cout << "Wrote output audio to " << filename << " (" << output_samples_per_channel[0].size() << " samples, " 
				          << NUM_CHANNELS << " channels, speed=" << PLAYBACK_SPEED << "x)" << std::endl;
			}
		}
	}

	delete global_time;
}

TEMPLATE_TEST_CASE("AudioRenderStageHistory2 - mock tape playback stage buffer output with continuity check", "[audio_history2][playback][buffer_output][continuity][gl_test][template]",
			   PlaybackTestParam1, PlaybackTestParam2, PlaybackTestParam3, PlaybackTestParam4, PlaybackTestParam5, PlaybackTestParam6) {

	constexpr auto P = get_playback_test_params(TestType::value);
	constexpr int BUFFER_SIZE = P.buffer_size;
	constexpr int NUM_CHANNELS = P.num_channels;
	constexpr float PLAYBACK_SPEED = P.speed;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float BASE_AMPLITUDE = 0.2f; // Base amplitude, increases by 0.2 per channel
	constexpr int RECORD_DURATION_SECONDS = 8;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 2;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	
	// Initialize playback stage (no final render stage needed)
	REQUIRE(playback_stage.initialize());
	
	context.prepare_draw();
	REQUIRE(playback_stage.bind());
	
	SECTION("Record sine wave and playback at different speeds with buffer output and continuity check") {
		// Record sine wave to tape - generate per channel with increasing amplitudes
		std::vector<float> phases(NUM_CHANNELS, 0.0f);
		for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
			// Generate sine wave per channel with different amplitudes
			// Channel 0: 0.2, Channel 1: 0.4, Channel 2: 0.6, Channel 3: 0.8, etc.
			std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				float channel_amplitude = BASE_AMPLITUDE * (ch + 1);
				// Generate sine wave for this specific channel
				auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
				// Copy to channel-major buffer (sine_buffer is interleaved but has only 1 channel)
				for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
					channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
				}
				phases[ch] += BUFFER_SIZE;
			}
			
			tape->record(channel_major_buffer.data());
		}
		
		REQUIRE(tape->size() > 0);
		
		// Initialize output sample vectors for this speed (per channel)
		std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
		}
		
		// Configure playback at the parameterized speed
		playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
		auto middle_position = tape->size() / 2;
		playback_stage.get_history().set_tape_position(middle_position);
		playback_stage.play();

		// Check the speed setting
		int speed_samples_per_buffer = *static_cast<const int*>(playback_stage.get_history().m_tape_speed->get_value());
		REQUIRE(static_cast<float>(speed_samples_per_buffer) == Catch::Approx(PLAYBACK_SPEED * BUFFER_SIZE).margin(1.00f));
		
		// Render and capture output to buffer (no final render stage)
		unsigned int frame_count = 0;
		const unsigned int max_frames = NUM_PLAYBACK_FRAMES;
		
		for (int frame = 0; frame < max_frames; ++frame) {
			global_time->set_value(frame);
			global_time->render();
			
			// Render playback stage (updates tape history texture)
			playback_stage.render(frame);
			
			// Get output directly from playback stage (not from final render stage)
			// NOTE: When storing without final render stage, output_audio_texture is NOT interleaved.
			// It's channel-major: texture is [width=frames_per_buffer, height=num_channels]
			// Layout: [ch0_sample0, ch0_sample1, ..., ch0_sampleN, ch1_sample0, ch1_sample1, ..., ch1_sampleN, ...]
			auto output_param = playback_stage.find_parameter("output_audio_texture");
			REQUIRE(output_param != nullptr);
			
			const float* output_data = static_cast<const float*>(output_param->get_value());
			REQUIRE(output_data != nullptr);
			
			// Store for verification
			// Convert from channel-major to per-channel vectors
			// Channel-major layout: [ch0_buffer, ch1_buffer, ch2_buffer, ...]
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				const float* channel_data = output_data + (ch * BUFFER_SIZE);
				for (int i = 0; i < BUFFER_SIZE; ++i) {
					output_samples_per_channel[ch].push_back(channel_data[i]);
				}
			}
			
			frame_count++;
			
			// Stop playback if we've reached the end of the tape
			if (playback_stage.get_history().get_tape_position() >= tape->size()) {
				playback_stage.stop();
				break;
			}
		}
		// Stop playback
		playback_stage.stop();
		
		SECTION("Continuity and Discontinuity Check") {
			// Check for sample-to-sample discontinuities per channel
			// Similar to audio_tape_render_stage_gl_test.cpp and audio_effect_render_stage_gl_test.cpp
			constexpr float DISCONTINUITY_THRESHOLD = 0.15f; // Conservative threshold for multi-tone content
			
			for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
				const auto& samples = output_samples_per_channel[ch];
				std::vector<size_t> discontinuity_indices;
				std::vector<float> discontinuity_magnitudes;
				
				// Calculate sample-to-sample differences to detect discontinuities
				for (size_t i = 1; i < samples.size(); ++i) {
					float sample_diff = std::abs(samples[i] - samples[i - 1]);
					if (sample_diff > DISCONTINUITY_THRESHOLD) {
						discontinuity_indices.push_back(i);
						discontinuity_magnitudes.push_back(sample_diff);
					}
				}
				
				INFO("Channel " << ch << " analysis:");
				INFO("  Total samples: " << samples.size());
				INFO("  Discontinuity threshold: " << DISCONTINUITY_THRESHOLD);
				INFO("  Found " << discontinuity_indices.size() << " discontinuities");
				
				if (!discontinuity_indices.empty()) {
					INFO("  First 5 discontinuity magnitudes:");
					for (size_t i = 0; i < std::min(discontinuity_indices.size(), size_t(5)); ++i) {
						INFO("    Sample " << discontinuity_indices[i] << ": " << discontinuity_magnitudes[i]);
					}
				}
				
				// Verify continuity - no discontinuities should occur
				REQUIRE(discontinuity_indices.size() == 0);
			}
		}
		
		SECTION("Amplitude and Signal Characteristics") {
			// Verify that output has reasonable amplitude characteristics
			for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
				const auto& samples = output_samples_per_channel[ch];
				
				// Check that we have samples
				REQUIRE(samples.size() > 0);
				
				// Check amplitude range (should be reasonable)
				float max_amplitude = 0.0f;
				for (float sample : samples) {
					max_amplitude = std::max(max_amplitude, std::abs(sample));
				}
				
				INFO("Channel " << ch << " max amplitude: " << max_amplitude);
				// Max amplitude should be reasonable (not clipped, not zero)
				REQUIRE(max_amplitude > 0.01f); // Should have some signal
				REQUIRE(max_amplitude < 2.0f);   // Should not be excessively loud
			}
		}
		
		if (is_csv_output_enabled()) {
			SECTION("Export to CSV") {
				// Verify we have the correct number of channels before writing
				REQUIRE(output_samples_per_channel.size() == NUM_CHANNELS);
				
				// Create CSV filename with test parameters
				std::ostringstream csv_filename_stream;
				csv_filename_stream << "playback_history_buffer_speed_" << std::fixed << std::setprecision(6) << PLAYBACK_SPEED 
				                   << "_channels_" << NUM_CHANNELS << ".csv";
				std::string csv_filename = csv_filename_stream.str();
				
				// Verify all channels have data before writing
				for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
					REQUIRE(output_samples_per_channel[ch].size() > 0);
				}
				
				// Write output data to CSV using framework utility
				CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
				REQUIRE(csv_writer.is_open());
				csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
				csv_writer.close();
				
				INFO("CSV file written to: " << csv_filename);
				INFO("To analyze discontinuities, run:");
				INFO("  cd playground && python3 analyze_discontinuities.py ../" << csv_filename << " --detect-only");
				INFO("Or view all samples:");
				INFO("  cd playground && python3 analyze_discontinuities.py ../" << csv_filename);
			}
		}
	}

	delete global_time;
}

TEST_CASE("AudioRenderStageHistory2 - tape stop functionality", "[audio_history2][tape_stop][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float AMPLITUDE = 0.5f;
	constexpr int RECORD_DURATION_SECONDS = 2;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	
	// Create final render stage
	AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	
	// Connect playback stage to final stage
	REQUIRE(playback_stage.connect_render_stage(&final_stage));
	
	// Initialize stages
	REQUIRE(playback_stage.initialize());
	REQUIRE(final_stage.initialize());
	
	context.prepare_draw();
	REQUIRE(playback_stage.bind());
	REQUIRE(final_stage.bind());
	
	// Record sine wave to tape
	std::vector<float> phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, AMPLITUDE, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data());
	}
	
	REQUIRE(tape->size() > 0);
	
	SECTION("Tape stops automatically at end during forward playback") {
		// Start playback near the end
		playback_stage.get_history().set_tape_speed(1.0f);
		unsigned int tape_size = tape->size();
		unsigned int start_position = tape_size - (static_cast<unsigned int>(BUFFER_SIZE) * 5); // Start 5 buffers before end
		playback_stage.get_history().set_tape_position(start_position);
		playback_stage.play();
		
		REQUIRE(playback_stage.get_history().is_tape_stopped() == false);
		REQUIRE(playback_stage.get_history().is_tape_at_end() == false);
		
		// Render frames until tape reaches end
		unsigned int frame = 0;
		bool reached_end = false;
		for (int i = 0; i < 20; ++i) { // Enough frames to reach end
			global_time->set_value(frame);
			global_time->render();
			
			playback_stage.render(frame);
			final_stage.render(frame);
			
			// Check if tape has stopped
			if (playback_stage.get_history().is_tape_stopped()) {
				reached_end = true;
				REQUIRE(playback_stage.get_history().is_tape_at_end() == true);
				REQUIRE(playback_stage.get_history().get_tape_position() >= tape_size);
				break;
			}
			
			frame++;
		}
		
		REQUIRE(reached_end == true);
		REQUIRE(playback_stage.get_history().is_tape_stopped() == true);
	}
	
	SECTION("Tape stops automatically at beginning during backward playback") {
		// Start playback near the beginning
		playback_stage.get_history().set_tape_speed(-1.0f);
		unsigned int start_position = static_cast<unsigned int>(BUFFER_SIZE * 2); // Start 5 buffers from beginning
		playback_stage.get_history().set_tape_position(start_position);
		playback_stage.get_history().start_tape(); // Ensure tape is not stopped
		playback_stage.play();
		
		REQUIRE(playback_stage.get_history().is_tape_stopped() == false);
		REQUIRE(playback_stage.get_history().is_tape_at_beginning() == false);
		
		// Render frames until tape reaches beginning
		unsigned int frame = 0;
		bool reached_beginning = false;
		for (int i = 0; i < 20; ++i) { // Enough frames to reach beginning
			global_time->set_value(frame);
			global_time->render();
			
			playback_stage.render(frame);
			final_stage.render(frame);
			
			// Check if tape has stopped
			if (playback_stage.get_history().is_tape_stopped()) {
				reached_beginning = true;
				REQUIRE(playback_stage.get_history().is_tape_at_beginning() == true);
				REQUIRE(playback_stage.get_history().get_tape_position() == 0u);
				break;
			}
			
			frame++;
		}
		
		REQUIRE(reached_beginning == true);
		REQUIRE(playback_stage.get_history().is_tape_stopped() == true);
	}
	
	SECTION("Manual stop and start functionality") {
		playback_stage.get_history().set_tape_speed(1.0f);
		playback_stage.get_history().set_tape_position(tape->size() / 2);
		playback_stage.get_history().start_tape();
		playback_stage.play();
		
		REQUIRE(playback_stage.get_history().is_tape_stopped() == false);
		
		// Render a few frames
		for (int i = 0; i < 5; ++i) {
			global_time->set_value(i);
			global_time->render();
			playback_stage.render(i);
			final_stage.render(i);
		}
		
		unsigned int position_before_stop = playback_stage.get_history().get_tape_position();
		
		// Manually stop the tape
		playback_stage.get_history().stop_tape();
		REQUIRE(playback_stage.get_history().is_tape_stopped() == true);
		REQUIRE(playback_stage.get_history().get_tape_speed_ratio() == 0.0f);
		
		// Render more frames - position should not change
		for (int i = 5; i < 10; ++i) {
			global_time->set_value(i);
			global_time->render();
			playback_stage.render(i);
			final_stage.render(i);
		}
		
		unsigned int position_after_stop = playback_stage.get_history().get_tape_position();
		REQUIRE(position_after_stop == position_before_stop);
		
		// Start the tape again and set speed
		playback_stage.get_history().start_tape();
		playback_stage.get_history().set_tape_speed(1.0f);
		REQUIRE(playback_stage.get_history().is_tape_stopped() == false);
		
		// Render more frames - position should advance
		unsigned int position_before_start = playback_stage.get_history().get_tape_position();
		for (int i = 10; i < 15; ++i) {
			global_time->set_value(i);
			global_time->render();
			playback_stage.render(i);
			final_stage.render(i);
		}
		
		unsigned int position_after_start = playback_stage.get_history().get_tape_position();
		REQUIRE(position_after_start > position_before_start);
	}
	
	SECTION("Shader outputs zeros when tape is stopped") {
		playback_stage.get_history().set_tape_speed(1.0f);
		playback_stage.get_history().set_tape_position(tape->size() / 2);
		playback_stage.get_history().start_tape();
		playback_stage.play();
		
		// Render a few frames to get some audio output
		for (int i = 0; i < 3; ++i) {
			global_time->set_value(i);
			global_time->render();
			playback_stage.render(i);
			final_stage.render(i);
		}
		
		// Get output before stopping
		auto output_param_before = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param_before != nullptr);
		const float* output_before = static_cast<const float*>(output_param_before->get_value());
		REQUIRE(output_before != nullptr);
		
		// Check that we have some non-zero samples before stopping
		bool has_non_zero_before = false;
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			if (std::abs(output_before[i]) > 0.001f) {
				has_non_zero_before = true;
				break;
			}
		}
		REQUIRE(has_non_zero_before == true);
		
		// Stop the tape
		playback_stage.get_history().stop_tape();
		REQUIRE(playback_stage.get_history().is_tape_stopped() == true);
		
		// Render frames after stopping
		for (int i = 3; i < 6; ++i) {
			global_time->set_value(i);
			global_time->render();
			playback_stage.render(i);
			final_stage.render(i);
		}
		
		// Get output after stopping
		auto output_param_after = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param_after != nullptr);
		const float* output_after = static_cast<const float*>(output_param_after->get_value());
		REQUIRE(output_after != nullptr);
		
		// Check that all samples are zero (or very close to zero)
		bool all_zero = true;
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			if (std::abs(output_after[i]) > 0.001f) {
				all_zero = false;
				INFO("Non-zero sample found at index " << i << ": " << output_after[i]);
				break;
			}
		}
		REQUIRE(all_zero == true);
	}
	
	SECTION("Tape state flags are correct") {
		// Test at beginning
		playback_stage.get_history().set_tape_position(0u);
		REQUIRE(playback_stage.get_history().is_tape_at_beginning() == true);
		REQUIRE(playback_stage.get_history().is_tape_at_end() == false);
		
		// Test in middle
		playback_stage.get_history().set_tape_position(tape->size() / 2);
		REQUIRE(playback_stage.get_history().is_tape_at_beginning() == false);
		REQUIRE(playback_stage.get_history().is_tape_at_end() == false);
		
		// Test at end
		playback_stage.get_history().set_tape_position(tape->size());
		REQUIRE(playback_stage.get_history().is_tape_at_beginning() == false);
		REQUIRE(playback_stage.get_history().is_tape_at_end() == true);
		
		// Test beyond end
		playback_stage.get_history().set_tape_position(tape->size() + 1000);
		REQUIRE(playback_stage.get_history().is_tape_at_end() == true);
	}
	
	SECTION("Tape stops at end and outputs zeros") {
		playback_stage.get_history().set_tape_speed(1.0f);
		unsigned int tape_size = tape->size();
		// Start very close to end
		unsigned int start_position = tape_size - static_cast<unsigned int>(BUFFER_SIZE);
		playback_stage.get_history().set_tape_position(start_position);
		playback_stage.get_history().start_tape();
		playback_stage.play();
		
		// Render until tape stops
		unsigned int frame = 0;
		for (int i = 0; i < 10; ++i) {
			global_time->set_value(frame);
			global_time->render();
			
			playback_stage.render(frame);
			final_stage.render(frame);
			
			if (playback_stage.get_history().is_tape_stopped()) {
				break;
			}
			frame++;
		}
		
		REQUIRE(playback_stage.get_history().is_tape_stopped() == true);
		REQUIRE(playback_stage.get_history().is_tape_at_end() == true);
		
		// Render a few more frames after stopping
		for (int i = 0; i < 3; ++i) {
			global_time->set_value(frame);
			global_time->render();
			playback_stage.render(frame);
			final_stage.render(frame);
			frame++;
		}
		
		// Check that output is zeros
		auto output_param = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param != nullptr);
		const float* output_data = static_cast<const float*>(output_param->get_value());
		REQUIRE(output_data != nullptr);
		
		bool all_zero = true;
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			if (std::abs(output_data[i]) > 0.001f) {
				all_zero = false;
				INFO("Non-zero sample found at index " << i << ": " << output_data[i]);
				break;
			}
		}
		REQUIRE(all_zero == true);
	}
	
	SECTION("Tape stops at beginning and outputs zeros") {
		playback_stage.get_history().set_tape_speed(-1.0f);
		// Start close to beginning
		unsigned int start_position = static_cast<unsigned int>(BUFFER_SIZE * 2);
		playback_stage.get_history().set_tape_position(start_position);
		playback_stage.get_history().start_tape();
		playback_stage.play();
		
		// Render until tape stops
		unsigned int frame = 0;
		for (int i = 0; i < 10; ++i) {
			global_time->set_value(frame);
			global_time->render();
			
			playback_stage.render(frame);
			final_stage.render(frame);
			
			if (playback_stage.get_history().is_tape_stopped()) {
				break;
			}
			frame++;
		}
		
		REQUIRE(playback_stage.get_history().is_tape_stopped() == true);
		REQUIRE(playback_stage.get_history().is_tape_at_beginning() == true);
		
		// Render a few more frames after stopping
		for (int i = 0; i < 3; ++i) {
			global_time->set_value(frame);
			global_time->render();
			playback_stage.render(frame);
			final_stage.render(frame);
			frame++;
		}
		
		// Check that output is zeros
		auto output_param = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param != nullptr);
		const float* output_data = static_cast<const float*>(output_param->get_value());
		REQUIRE(output_data != nullptr);
		
		bool all_zero = true;
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			if (std::abs(output_data[i]) > 0.001f) {
				all_zero = false;
				INFO("Non-zero sample found at index " << i << ": " << output_data[i]);
				break;
			}
		}
		REQUIRE(all_zero == true);
	}

	delete global_time;
}

TEST_CASE("AudioRenderStageHistory2 - dynamic speed changes with continuity check", "[audio_history2][dynamic_speed][continuity][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float LEFT_AMPLITUDE = 0.5f;   // Left channel amplitude
	constexpr float RIGHT_AMPLITUDE = 0.25f;  // Right channel amplitude (half of left)
	constexpr int RECORD_DURATION_SECONDS = 4;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 3;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;
	constexpr float MIN_SPEED = -1.0f;
	constexpr float MAX_SPEED = 1.0f;
	

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	
	// Create final render stage
	AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	
	// Connect playback stage to final stage
	REQUIRE(playback_stage.connect_render_stage(&final_stage));
	
	// Initialize stages
	REQUIRE(playback_stage.initialize());
	REQUIRE(final_stage.initialize());
	
	context.prepare_draw();
	REQUIRE(playback_stage.bind());
	REQUIRE(final_stage.bind());
	
	// Record sine wave to tape with different amplitudes per channel
	std::vector<float> channel_amplitudes = {LEFT_AMPLITUDE, RIGHT_AMPLITUDE}; // Left and right channel amplitudes
	std::vector<float> phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			float amplitude = (ch < channel_amplitudes.size()) ? channel_amplitudes[ch] : 0.5f;
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data());
	}
	
	REQUIRE(tape->size() > 0);
	
	// Setup audio output (only if enabled)
	AudioPlayerOutput* audio_output = nullptr;
	if (is_audio_output_enabled()) {
		printf("Audio output enabled - initializing AudioPlayerOutput\n");
		audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
		bool opened = audio_output->open();
		bool started = audio_output->start();
		printf("Audio device opened: %s, started: %s\n", opened ? "yes" : "no", started ? "yes" : "no");
		REQUIRE(opened);
		REQUIRE(started);
	} else {
		printf("Audio output NOT enabled (ENABLE_AUDIO_OUTPUT not set)\n");
	}
	
	// Initialize output sample vectors (per channel)
	std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
	}
	
	// Start playback in the middle
	playback_stage.get_history().set_tape_position(tape->size() / 2);
	playback_stage.get_history().start_tape();
	playback_stage.play();
	
	// Render with dynamically changing speed
	float previous_speed = MAX_SPEED;
	int frames_rendered = 0;
	for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
		global_time->set_value(frame);
		global_time->render();
		
		// Calculate speed using sine wave: smoothly transitions from MAX_SPEED to MIN_SPEED
		// Map frame progress [0, 1] to angle [0, π/2], use sine for smooth transition
		float progress = static_cast<float>(frame) / static_cast<float>(NUM_PLAYBACK_FRAMES);
		constexpr float PI = 3.14159265358979323846f;
		float angle = progress * PI / 2.0f; // 0 to π/2
		// Use sin(angle) to map [0, π/2] to [0, 1] smoothly
		// This creates a smooth transition from MAX_SPEED (at 0) to MIN_SPEED (at π/2)
		float speed = MAX_SPEED + (MIN_SPEED - MAX_SPEED) * std::sin(angle);
		
		// Verify speed changes are continuous (small delta per frame)
		if (frame > 0) {
			float speed_delta = std::abs(speed - previous_speed);
			float max_speed_delta_per_frame = std::abs(MAX_SPEED - MIN_SPEED) / static_cast<float>(NUM_PLAYBACK_FRAMES) * 2.0f; // Allow 2x for smooth curves
			REQUIRE(speed_delta <= max_speed_delta_per_frame);
		}
		previous_speed = speed;
		
		playback_stage.get_history().set_tape_speed(speed);
		
		// Render playback stage (updates tape history texture)
		playback_stage.render(frame);
		
		// Render final stage
		final_stage.render(frame);
		
		// Get output from final stage
		auto output_param = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param != nullptr);
		const float* output_data = static_cast<const float*>(output_param->get_value());
		REQUIRE(output_data != nullptr);
		
		// Store for verification (de-interleave from interleaved format)
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			auto channel = i % NUM_CHANNELS;
			output_samples_per_channel[channel].push_back(output_data[i]);
		}
		
		// Push to audio output
		if (audio_output) {
			while (!audio_output->is_ready()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			audio_output->push(output_data);
		}
		
		frames_rendered++;
		
		// Stop if tape has stopped (reached boundary)
		if (playback_stage.get_history().is_tape_stopped()) {
			printf("Tape stopped at frame %d (boundary reached)\n", frame);
			break;
		}
	}
	printf("Rendered %d frames out of %d requested\n", frames_rendered, NUM_PLAYBACK_FRAMES);
	
	// Stop playback
	playback_stage.stop();
	
	// Wait for audio to finish and close audio output
	if (audio_output) {
		size_t queued_bytes = audio_output->queued_bytes();
		printf("Audio queue has %zu bytes queued, waiting for playback to finish...\n", queued_bytes);
		// Wait for audio queue to drain (playback duration + buffer time)
		const int total_playback_ms = (PLAYBACK_DURATION_SECONDS * 1000) + 500;
		int waited_ms = 0;
		while (queued_bytes > 0 && waited_ms < total_playback_ms) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			waited_ms += 10;
			queued_bytes = audio_output->queued_bytes();
			if (waited_ms % 100 == 0) {
				printf("  Waited %d ms, %zu bytes still queued\n", waited_ms, queued_bytes);
			}
		}
		printf("Finished waiting after %d ms, final queue size: %zu bytes\n", waited_ms, audio_output->queued_bytes());
		// Additional small wait to ensure audio finishes
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		audio_output->close();
		delete audio_output;
	}
	
	SECTION("Continuity check with dynamic speed changes") {
		constexpr float DISCONTINUITY_THRESHOLD = 0.2f; // Slightly higher threshold for speed changes
		
		for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			std::vector<size_t> discontinuity_indices;
			
			// Calculate sample-to-sample differences to detect discontinuities
			for (size_t i = 1; i < samples.size(); ++i) {
				float sample_diff = std::abs(samples[i] - samples[i - 1]);
				if (sample_diff > DISCONTINUITY_THRESHOLD) {
					discontinuity_indices.push_back(i);
				}
			}
			
			INFO("Channel " << ch << " analysis:");
			INFO("  Total samples: " << samples.size());
			INFO("  Discontinuity threshold: " << DISCONTINUITY_THRESHOLD);
			INFO("  Found " << discontinuity_indices.size() << " discontinuities");
			
			// Verify continuity - speed changes should be smooth, no discontinuities
			REQUIRE(discontinuity_indices.size() == 0);
		}
	}
	
	if (is_csv_output_enabled()) {
		SECTION("Export dynamic speed playback to CSV") {
			// Create CSV output directory in build/tests/csv_output/
			std::string csv_output_dir = "build/tests/csv_output";
			// Create directory if it doesn't exist (simple approach - will fail silently if exists)
			system(("mkdir -p " + csv_output_dir).c_str());
			
			std::ostringstream csv_filename_stream;
			csv_filename_stream << csv_output_dir << "/dynamic_speed_playback_channels_" << NUM_CHANNELS << ".csv";
			std::string csv_filename = csv_filename_stream.str();
			
			CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
			REQUIRE(csv_writer.is_open());
			csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
			csv_writer.close();
			
			INFO("CSV file written to: " << csv_filename);
		}
	}

	delete global_time;
}

TEST_CASE("AudioRenderStageHistory2 - forward loop with continuity check", "[audio_history2][tape_loop][continuity][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float LEFT_AMPLITUDE = 0.5f;
	constexpr float RIGHT_AMPLITUDE = 0.25f;
	constexpr int RECORD_DURATION_SECONDS = 2;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 3; // Play longer than recorded to test looping
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;
	constexpr float PLAYBACK_SPEED = 1.0f;

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	
	// Create final render stage
	AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	
	// Connect playback stage to final stage
	REQUIRE(playback_stage.connect_render_stage(&final_stage));
	
	// Initialize stages
	REQUIRE(playback_stage.initialize());
	REQUIRE(final_stage.initialize());
	
	context.prepare_draw();
	REQUIRE(playback_stage.bind());
	REQUIRE(final_stage.bind());
	
	// Record sine wave to tape with different amplitudes per channel
	std::vector<float> channel_amplitudes = {LEFT_AMPLITUDE, RIGHT_AMPLITUDE};
	std::vector<float> phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			float amplitude = (ch < channel_amplitudes.size()) ? channel_amplitudes[ch] : 0.5f;
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data());
	}
	
	REQUIRE(tape->size() > 0);
	
	// Enable loop
	playback_stage.get_history().set_tape_loop(true);
	REQUIRE(playback_stage.get_history().is_tape_loop_enabled() == true);
	
	// Initialize output sample vectors (per channel)
	std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
	}
	
	// Start playback
	playback_stage.get_history().set_tape_position(0u);
	playback_stage.get_history().start_tape();
	playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
	playback_stage.play();
	
	// Render with looping
	for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
		global_time->set_value(frame);
		global_time->render();
		
		// Render playback stage (updates tape history texture)
		playback_stage.render(frame);
		
		// Render final stage
		final_stage.render(frame);
		
		// Get output from final stage
		auto output_param = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param != nullptr);
		const float* output_data = static_cast<const float*>(output_param->get_value());
		REQUIRE(output_data != nullptr);
		
		// Store for verification (de-interleave from interleaved format)
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			auto channel = i % NUM_CHANNELS;
			output_samples_per_channel[channel].push_back(output_data[i]);
		}
	}
	
	// Stop playback
	playback_stage.stop();
	
	SECTION("Continuity check with forward loop") {
		constexpr float DISCONTINUITY_THRESHOLD = 0.3f; // Higher threshold for loop wrap points
		
		for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			std::vector<size_t> discontinuity_indices;
			
			// Calculate sample-to-sample differences to detect discontinuities
			for (size_t i = 1; i < samples.size(); ++i) {
				float sample_diff = std::abs(samples[i] - samples[i - 1]);
				if (sample_diff > DISCONTINUITY_THRESHOLD) {
					discontinuity_indices.push_back(i);
				}
			}
			
			INFO("Channel " << ch << " analysis:");
			INFO("  Total samples: " << samples.size());
			INFO("  Discontinuity threshold: " << DISCONTINUITY_THRESHOLD);
			INFO("  Found " << discontinuity_indices.size() << " discontinuities");
			
			// When looping, small discontinuities at wrap points are expected if tape length
			// doesn't perfectly match the sine wave period. Allow up to 2 discontinuities
			// (one per wrap) for a 3-second playback of a 2-second tape.
			REQUIRE(discontinuity_indices.size() <= 2);
		}
		
		// Verify tape is still playing (not stopped) after looping
		REQUIRE(playback_stage.get_history().is_tape_stopped() == false);
	}
	
	if (is_csv_output_enabled()) {
		SECTION("Export forward loop playback to CSV") {
			std::string csv_output_dir = "build/tests/csv_output";
			system(("mkdir -p " + csv_output_dir).c_str());
			
			std::ostringstream csv_filename_stream;
			csv_filename_stream << csv_output_dir << "/forward_loop_playback_channels_" << NUM_CHANNELS << ".csv";
			std::string csv_filename = csv_filename_stream.str();
			
			CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
			REQUIRE(csv_writer.is_open());
			csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
			csv_writer.close();
			
			INFO("CSV file written to: " << csv_filename);
		}
	}

	delete global_time;
}

TEST_CASE("AudioRenderStageHistory2 - backward loop with continuity check", "[audio_history2][tape_loop][continuity][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float LEFT_AMPLITUDE = 0.5f;
	constexpr float RIGHT_AMPLITUDE = 0.25f;
	constexpr int RECORD_DURATION_SECONDS = 2;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 3; // Play longer than recorded to test looping
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;
	constexpr float PLAYBACK_SPEED = -1.0f; // Negative speed for backward playback

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	
	// Create final render stage
	AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	
	// Connect playback stage to final stage
	REQUIRE(playback_stage.connect_render_stage(&final_stage));
	
	// Initialize stages
	REQUIRE(playback_stage.initialize());
	REQUIRE(final_stage.initialize());
	
	context.prepare_draw();
	REQUIRE(playback_stage.bind());
	REQUIRE(final_stage.bind());
	
	// Record sine wave to tape with different amplitudes per channel
	std::vector<float> channel_amplitudes = {LEFT_AMPLITUDE, RIGHT_AMPLITUDE};
	std::vector<float> phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			float amplitude = (ch < channel_amplitudes.size()) ? channel_amplitudes[ch] : 0.5f;
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data());
	}
	
	REQUIRE(tape->size() > 0);
	
	// Enable loop
	playback_stage.get_history().set_tape_loop(true);
	REQUIRE(playback_stage.get_history().is_tape_loop_enabled() == true);
	
	// Initialize output sample vectors (per channel)
	std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
	}
	
	// Start playback from middle (going backwards will wrap to end)
	playback_stage.get_history().set_tape_position(tape->size() / 2);
	playback_stage.get_history().start_tape();
	playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
	playback_stage.play();
	
	// Render with looping
	for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
		global_time->set_value(frame);
		global_time->render();
		
		// Render playback stage (updates tape history texture)
		playback_stage.render(frame);
		
		// Render final stage
		final_stage.render(frame);
		
		// Get output from final stage
		auto output_param = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param != nullptr);
		const float* output_data = static_cast<const float*>(output_param->get_value());
		REQUIRE(output_data != nullptr);
		
		// Store for verification (de-interleave from interleaved format)
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			auto channel = i % NUM_CHANNELS;
			output_samples_per_channel[channel].push_back(output_data[i]);
		}
	}
	
	// Stop playback
	playback_stage.stop();
	
	SECTION("Continuity check with backward loop") {
		constexpr float DISCONTINUITY_THRESHOLD = 0.3f; // Higher threshold for loop wrap points
		
		for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			std::vector<size_t> discontinuity_indices;
			
			// Calculate sample-to-sample differences to detect discontinuities
			for (size_t i = 1; i < samples.size(); ++i) {
				float sample_diff = std::abs(samples[i] - samples[i - 1]);
				if (sample_diff > DISCONTINUITY_THRESHOLD) {
					discontinuity_indices.push_back(i);
				}
			}
			
			INFO("Channel " << ch << " analysis:");
			INFO("  Total samples: " << samples.size());
			INFO("  Discontinuity threshold: " << DISCONTINUITY_THRESHOLD);
			INFO("  Found " << discontinuity_indices.size() << " discontinuities");
			
			// When looping, small discontinuities at wrap points are expected if tape length
			// doesn't perfectly match the sine wave period. Allow up to 2 discontinuities
			// (one per wrap) for a 3-second playback of a 2-second tape.
			REQUIRE(discontinuity_indices.size() <= 2);
		}
		
		// Verify tape is still playing (not stopped) after looping
		REQUIRE(playback_stage.get_history().is_tape_stopped() == false);
	}
	
		if (is_csv_output_enabled()) {
			SECTION("Export backward loop playback to CSV") {
				std::string csv_output_dir = "build/tests/csv_output";
				system(("mkdir -p " + csv_output_dir).c_str());
				
				std::ostringstream csv_filename_stream;
				csv_filename_stream << csv_output_dir << "/backward_loop_playback_channels_" << NUM_CHANNELS << ".csv";
				std::string csv_filename = csv_filename_stream.str();
				
				CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
				REQUIRE(csv_writer.is_open());
				csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
				csv_writer.close();
				
				INFO("CSV file written to: " << csv_filename);
			}
		}

	delete global_time;
}

TEST_CASE("AudioTape - load from WAV file and playback", "[audio_tape][wav_load][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float WINDOW_SIZE_SECONDS = 2.0f;
	
	// Test with different WAV files from media directory
	std::vector<std::string> wav_files = {
		"media/sine.wav",
		"media/test.wav"
	};
	
	for (const auto& wav_file : wav_files) {
		SECTION("Load and playback: " + wav_file) {
			SDLWindow window(BUFFER_SIZE, 2); // Assume stereo for now
			GLContext context;
			
			// Global time buffer
			auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
			global_time->set_value(0);
			REQUIRE(global_time->initialize());
			
			// Load WAV file into AudioTape
			auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE);
			REQUIRE(tape != nullptr);
			REQUIRE(tape->size() > 0);
			
			// Get the number of channels from the tape
			const unsigned int num_channels = tape->num_channels();
			INFO("Loaded tape with " << num_channels << " channels, " << tape->size() << " samples per channel");
			
			// Create mock playback stage
			MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels, WINDOW_SIZE_SECONDS);
			playback_stage.get_history().set_tape(tape);
			
			// Create final render stage
			AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels);
			
			// Connect playback stage to final stage
			REQUIRE(playback_stage.connect_render_stage(&final_stage));
			
			// Initialize stages
			REQUIRE(playback_stage.initialize());
			REQUIRE(final_stage.initialize());
			
			context.prepare_draw();
			REQUIRE(playback_stage.bind());
			REQUIRE(final_stage.bind());
			
			// Set playback speed and start position
			playback_stage.get_history().set_tape_speed(1.0f);
			playback_stage.get_history().set_tape_position(0u);
			playback_stage.get_history().start_tape();
			playback_stage.play();
			
			// Calculate how many frames to render (play back the entire tape)
			const unsigned int tape_size_samples = tape->size();
			const unsigned int frames_to_render = (tape_size_samples / BUFFER_SIZE) + 10; // Add some extra frames
			
			// Collect output samples
			std::vector<std::vector<float>> output_samples_per_channel(num_channels);
			for (unsigned int ch = 0; ch < num_channels; ++ch) {
				output_samples_per_channel[ch].reserve(frames_to_render * BUFFER_SIZE);
			}
			
			// Render frames
			unsigned int frame = 0;
			for (unsigned int i = 0; i < frames_to_render; ++i) {
				global_time->set_value(frame);
				global_time->render();
				
				playback_stage.render(frame);
				final_stage.render(frame);
				
				// Extract output samples
				auto* output_param = final_stage.find_parameter("output_audio_texture");
				if (output_param) {
					const float* output_data = static_cast<const float*>(output_param->get_value());
					REQUIRE(output_data != nullptr);
					
					// Store for verification (de-interleave from interleaved format)
					// Layout: [frame0_ch0, frame0_ch1, ..., frame0_chN, frame1_ch0, frame1_ch1, ..., frame1_chN, ...]
					for (unsigned int j = 0; j < BUFFER_SIZE; ++j) {
						for (unsigned int ch = 0; ch < num_channels; ++ch) {
							output_samples_per_channel[ch].push_back(output_data[j * num_channels + ch]);
						}
					}
				}
				
				// Stop if we've reached the end of the tape
				if (playback_stage.get_history().is_tape_stopped() || 
				    playback_stage.get_history().is_tape_at_end()) {
					break;
				}
				
				frame++;
			}
			
			// Verify we got some output
			REQUIRE(output_samples_per_channel[0].size() > 0);
			
			// Verify the output is not all zeros (should contain audio data)
			bool has_non_zero_samples = false;
			for (unsigned int ch = 0; ch < num_channels; ++ch) {
				for (float sample : output_samples_per_channel[ch]) {
					if (std::abs(sample) > 0.001f) {
						has_non_zero_samples = true;
						break;
					}
				}
				if (has_non_zero_samples) break;
			}
			REQUIRE(has_non_zero_samples == true);
			
			// Verify tape position advanced
			REQUIRE(playback_stage.get_history().get_tape_position() > 0);
			
			// Verify tape size matches expected
			REQUIRE(tape->size() > 0);
			REQUIRE(tape->size_in_seconds() > 0.0f);
			
			if (is_csv_output_enabled()) {
				SECTION("Export WAV playback to CSV: " + wav_file) {
					std::string csv_output_dir = "build/tests/csv_output";
					system(("mkdir -p " + csv_output_dir).c_str());
					
					// Create filename from WAV filename
					std::string csv_filename = csv_output_dir + "/wav_load_playback_" + 
					                           wav_file.substr(wav_file.find_last_of("/") + 1) + ".csv";
					
					CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
					REQUIRE(csv_writer.is_open());
					csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
					csv_writer.close();
					
					INFO("CSV file written to: " << csv_filename);
				}
			}
			
			delete global_time;
		}
	}
}

TEST_CASE("AudioTape - load from WAV file with different speeds", "[audio_tape][wav_load][speed][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float WINDOW_SIZE_SECONDS = 2.0f;
	const std::string wav_file = "media/test.wav";
	
	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Load WAV file into AudioTape
	auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE);
	REQUIRE(tape != nullptr);
	REQUIRE(tape->size() > 0);
	
	const unsigned int num_channels = tape->num_channels();
	
	// Test different playback speeds
	std::vector<float> speeds = {1.0f, 0.5f, 2.0f, -1.0f};
	
	for (float speed : speeds) {
		SECTION("Playback at speed " + std::to_string(speed) + "x") {
			// Create mock playback stage
			MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels, WINDOW_SIZE_SECONDS);
			playback_stage.get_history().set_tape(tape);
			
			// Create final render stage
			AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels);
			
			// Connect and initialize
			REQUIRE(playback_stage.connect_render_stage(&final_stage));
			REQUIRE(playback_stage.initialize());
			REQUIRE(final_stage.initialize());
			
			context.prepare_draw();
			REQUIRE(playback_stage.bind());
			REQUIRE(final_stage.bind());
			
			// Set playback speed and start position
			playback_stage.get_history().set_tape_speed(speed);
			if (speed < 0.0f) {
				// For reverse playback, start near the end
				playback_stage.get_history().set_tape_position(tape->size() - BUFFER_SIZE * 10);
			} else {
				playback_stage.get_history().set_tape_position(0u);
			}
			playback_stage.get_history().start_tape();
			playback_stage.play();
			
			// Render a few frames to verify playback works
			unsigned int frame = 0;
			for (unsigned int i = 0; i < 20; ++i) {
				global_time->set_value(frame);
				global_time->render();
				
				playback_stage.render(frame);
				final_stage.render(frame);
				
				// Check that tape position is advancing (or moving backwards for negative speed)
				if (i > 5) { // Give it a few frames to start
					if (speed > 0.0f) {
						REQUIRE(playback_stage.get_history().get_tape_position() > 0);
					} else if (speed < 0.0f) {
						// For reverse, position should decrease or wrap around
						unsigned int pos = playback_stage.get_history().get_tape_position();
						REQUIRE(pos <= tape->size());
					}
				}
				
				// Stop if we've reached a boundary
				if (playback_stage.get_history().is_tape_stopped()) {
					break;
				}
				
				frame++;
			}
			
			// Verify speed was set correctly
			REQUIRE(std::abs(playback_stage.get_history().get_tape_speed_ratio() - speed) < 0.01f);
		}
	}
	
	delete global_time;
}

TEST_CASE("AudioTape - load from WAV file with continuous speed change forward to backward", "[audio_tape][wav_load][speed_change][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	// Use a smaller window (1.0 second) so it's less than the tape duration and window updates are audible
	constexpr float WINDOW_SIZE_SECONDS = 1.0f;
	const std::string wav_file = "media/test.wav";
	constexpr int TOTAL_FRAMES = 200; // Number of frames to render
	
	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Load WAV file into AudioTape
	auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE);
	REQUIRE(tape != nullptr);
	REQUIRE(tape->size() > 0);
	
	const unsigned int num_channels = tape->num_channels();
	const float tape_duration_seconds = tape->size_in_seconds();
	
	// Verify window is smaller than tape duration
	REQUIRE(WINDOW_SIZE_SECONDS < tape_duration_seconds);
	
	// Create mock playback stage with smaller window
	MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	
	// Create final render stage
	AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels);
	
	// Connect and initialize
	REQUIRE(playback_stage.connect_render_stage(&final_stage));
	REQUIRE(playback_stage.initialize());
	REQUIRE(final_stage.initialize());
	
	context.prepare_draw();
	REQUIRE(playback_stage.bind());
	REQUIRE(final_stage.bind());
	
	// Set up audio output
	AudioPlayerOutput* audio_output = nullptr;
	if (is_audio_output_enabled()) {
		audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, num_channels);
		REQUIRE(audio_output->open());
		REQUIRE(audio_output->start());
	}
	
	// Start at middle of tape so window updates are audible as we play forward/backward
	unsigned int start_position = tape->size() / 2;
	playback_stage.get_history().set_tape_position(start_position);
	playback_stage.get_history().start_tape();
	playback_stage.play();
	
	INFO("Tape duration: " << tape_duration_seconds << " seconds");
	INFO("Window size: " << WINDOW_SIZE_SECONDS << " seconds");
	INFO("Starting position: " << start_position << " samples (" << (start_position / static_cast<float>(SAMPLE_RATE)) << " seconds)");
	
	// Collect output samples
	std::vector<std::vector<float>> output_samples_per_channel(num_channels);
	for (unsigned int ch = 0; ch < num_channels; ++ch) {
		output_samples_per_channel[ch].reserve(TOTAL_FRAMES * BUFFER_SIZE);
	}
	
	// Render frames with continuous speed change from forward to backward
	unsigned int frame = 0;
	for (unsigned int i = 0; i < TOTAL_FRAMES; ++i) {
		global_time->set_value(frame);
		global_time->render();
		
		// Calculate speed: starts at 1.0, goes to -1.0, then back to 1.0
		// Creates a smooth transition: forward -> stop -> backward -> stop -> forward
		float progress = static_cast<float>(i) / static_cast<float>(TOTAL_FRAMES);
		float speed;
		if (progress < 0.25f) {
			// Forward: 1.0 to 0.0
			speed = 1.0f - (progress / 0.25f);
		} else if (progress < 0.5f) {
			// Backward: 0.0 to -1.0
			speed = -((progress - 0.25f) / 0.25f);
		} else if (progress < 0.75f) {
			// Backward: -1.0 to 0.0
			speed = -1.0f + ((progress - 0.5f) / 0.25f);
		} else {
			// Forward: 0.0 to 1.0
			speed = (progress - 0.75f) / 0.25f;
		}
		
		playback_stage.get_history().set_tape_speed(speed);
		
		playback_stage.render(frame);
		final_stage.render(frame);
		
		// Extract output samples
		auto* output_param = final_stage.find_parameter("final_output_audio_texture");
		if (output_param) {
			const float* output_data = static_cast<const float*>(output_param->get_value());
			REQUIRE(output_data != nullptr);
			
			// Store for verification (de-interleave from interleaved format)
			// final_output_audio_texture is interleaved: [frame0_ch0, frame0_ch1, ..., frame0_chN, frame1_ch0, ...]
			for (int i = 0; i < BUFFER_SIZE * num_channels; ++i) {
				auto channel = i % num_channels;
				output_samples_per_channel[channel].push_back(output_data[i]);
			}
			
			// Push to audio output (output_data is interleaved format)
			if (audio_output) {
				while (!audio_output->is_ready()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				audio_output->push(output_data);
			}
		}
		
		// Stop if we've reached a boundary (unless looping)
		if (playback_stage.get_history().is_tape_stopped() && speed != 0.0f) {
			// If stopped and speed is not zero, restart
			playback_stage.get_history().start_tape();
		}
		
		frame++;
	}
	
	// Clean up audio output
	if (audio_output) {
		audio_output->stop();
		audio_output->close();
		delete audio_output;
	}
	
	// Verify we got output
	REQUIRE(output_samples_per_channel[0].size() > 0);
	
	// Verify the output is not all zeros
	bool has_non_zero_samples = false;
	for (unsigned int ch = 0; ch < num_channels; ++ch) {
		for (float sample : output_samples_per_channel[ch]) {
			if (std::abs(sample) > 0.001f) {
				has_non_zero_samples = true;
				break;
			}
		}
		if (has_non_zero_samples) break;
	}
	REQUIRE(has_non_zero_samples == true);
	
	SECTION("Continuity check with continuous speed changes") {
		constexpr float DISCONTINUITY_THRESHOLD = 0.3f; // Higher threshold for speed transitions
		
		for (unsigned int ch = 0; ch < num_channels; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			std::vector<size_t> discontinuity_indices;
			
			// Calculate sample-to-sample differences to detect discontinuities
			for (size_t i = 1; i < samples.size(); ++i) {
				float sample_diff = std::abs(samples[i] - samples[i - 1]);
				if (sample_diff > DISCONTINUITY_THRESHOLD) {
					discontinuity_indices.push_back(i);
				}
			}
			
			INFO("Channel " << ch << " analysis:");
			INFO("  Total samples: " << samples.size());
			INFO("  Discontinuity threshold: " << DISCONTINUITY_THRESHOLD);
			INFO("  Found " << discontinuity_indices.size() << " discontinuities");
			
			// Allow some discontinuities at speed transition points, but not too many
			// Speed changes should be relatively smooth
			REQUIRE(discontinuity_indices.size() < samples.size() / 100); // Less than 1% discontinuities
		}
	}
	
	if (is_csv_output_enabled()) {
		SECTION("Export continuous speed change playback to CSV") {
			std::string csv_output_dir = "build/tests/csv_output";
			system(("mkdir -p " + csv_output_dir).c_str());
			
			std::string csv_filename = csv_output_dir + "/wav_load_continuous_speed_change.csv";
			
			CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
			REQUIRE(csv_writer.is_open());
			csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
			csv_writer.close();
			
			INFO("CSV file written to: " << csv_filename);
		}
	}
	
	delete global_time;
}

TEST_CASE("AudioTape - load from WAV file with loop enabled", "[audio_tape][wav_load][loop][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	// Use a smaller window (1.0 second) so it's less than the tape duration and window updates are audible
	constexpr float WINDOW_SIZE_SECONDS = 1.0f;
	const std::string wav_file = "media/test.wav";
	constexpr int TOTAL_FRAMES = 500; // Enough frames to loop multiple times
	
	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Load WAV file into AudioTape
	auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE);
	REQUIRE(tape != nullptr);
	REQUIRE(tape->size() > 0);
	
	const unsigned int num_channels = tape->num_channels();
	const unsigned int tape_size_samples = tape->size();
	const float tape_duration_seconds = tape->size_in_seconds();
	
	// Verify window is smaller than tape duration
	REQUIRE(WINDOW_SIZE_SECONDS < tape_duration_seconds);
	
	// Create mock playback stage with smaller window
	MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	
	// Create final render stage
	AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels);
	
	// Connect and initialize
	REQUIRE(playback_stage.connect_render_stage(&final_stage));
	REQUIRE(playback_stage.initialize());
	REQUIRE(final_stage.initialize());
	
	context.prepare_draw();
	REQUIRE(playback_stage.bind());
	REQUIRE(final_stage.bind());
	
	// Set up audio output
	AudioPlayerOutput* audio_output = nullptr;
	if (is_audio_output_enabled()) {
		audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, num_channels);
		REQUIRE(audio_output->open());
		REQUIRE(audio_output->start());
	}
	
	// Enable loop
	playback_stage.get_history().set_tape_loop(true);
	REQUIRE(playback_stage.get_history().is_tape_loop_enabled() == true);
	
	// Set playback speed and start position in the middle of the tape
	// This ensures window updates are audible as we loop
	playback_stage.get_history().set_tape_speed(1.0f);
	unsigned int start_position = tape_size_samples / 2;
	playback_stage.get_history().set_tape_position(start_position);
	playback_stage.get_history().start_tape();
	playback_stage.play();
	
	INFO("Tape duration: " << tape_duration_seconds << " seconds");
	INFO("Window size: " << WINDOW_SIZE_SECONDS << " seconds");
	INFO("Starting position: " << start_position << " samples (" << (start_position / static_cast<float>(SAMPLE_RATE)) << " seconds)");
	
	// Collect output samples
	std::vector<std::vector<float>> output_samples_per_channel(num_channels);
	for (unsigned int ch = 0; ch < num_channels; ++ch) {
		output_samples_per_channel[ch].reserve(TOTAL_FRAMES * BUFFER_SIZE);
	}
	
	// Track loop count
	unsigned int loop_count = 0;
	unsigned int last_position = 0;
	
	// Render frames
	unsigned int frame = 0;
	for (unsigned int i = 0; i < TOTAL_FRAMES; ++i) {
		global_time->set_value(frame);
		global_time->render();
		
		playback_stage.render(frame);
		final_stage.render(frame);
		
		// Check if we've looped (position wrapped around)
		unsigned int current_position = playback_stage.get_history().get_tape_position();
		if (current_position < last_position && last_position > tape_size_samples / 2) {
			loop_count++;
			INFO("Loop detected at frame " << i << ", loop count: " << loop_count);
		}
		last_position = current_position;
		
		// Extract output samples
		auto* output_param = final_stage.find_parameter("final_output_audio_texture");
		if (output_param) {
			const float* output_data = static_cast<const float*>(output_param->get_value());
			REQUIRE(output_data != nullptr);
			
			// Store for verification (de-interleave from interleaved format)
			// final_output_audio_texture is interleaved: [frame0_ch0, frame0_ch1, ..., frame0_chN, frame1_ch0, ...]
			for (int i = 0; i < BUFFER_SIZE * num_channels; ++i) {
				auto channel = i % num_channels;
				output_samples_per_channel[channel].push_back(output_data[i]);
			}
			
			// Push to audio output (output_data is interleaved format)
			if (audio_output) {
				while (!audio_output->is_ready()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				audio_output->push(output_data);
			}
		}
		
		// Verify tape never stops (because loop is enabled)
		REQUIRE(playback_stage.get_history().is_tape_stopped() == false);
		
		frame++;
	}
	
	// Clean up audio output
	if (audio_output) {
		audio_output->stop();
		audio_output->close();
		delete audio_output;
	}
	
	// Verify we got output
	REQUIRE(output_samples_per_channel[0].size() > 0);
	
	// Verify the output is not all zeros
	bool has_non_zero_samples = false;
	for (unsigned int ch = 0; ch < num_channels; ++ch) {
		for (float sample : output_samples_per_channel[ch]) {
			if (std::abs(sample) > 0.001f) {
				has_non_zero_samples = true;
				break;
			}
		}
		if (has_non_zero_samples) break;
	}
	REQUIRE(has_non_zero_samples == true);
	
	// Verify we looped at least once (given enough frames)
	unsigned int expected_loops = (TOTAL_FRAMES * BUFFER_SIZE) / tape_size_samples;
	if (expected_loops > 0) {
		REQUIRE(loop_count > 0);
		INFO("Expected at least " << expected_loops << " loops, detected " << loop_count);
	}
	
	SECTION("Continuity check with loop") {
		constexpr float DISCONTINUITY_THRESHOLD = 0.3f; // Higher threshold for loop wrap points
		
		for (unsigned int ch = 0; ch < num_channels; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			std::vector<size_t> discontinuity_indices;
			
			// Calculate sample-to-sample differences to detect discontinuities
			for (size_t i = 1; i < samples.size(); ++i) {
				float sample_diff = std::abs(samples[i] - samples[i - 1]);
				if (sample_diff > DISCONTINUITY_THRESHOLD) {
					discontinuity_indices.push_back(i);
				}
			}
			
			INFO("Channel " << ch << " analysis:");
			INFO("  Total samples: " << samples.size());
			INFO("  Discontinuity threshold: " << DISCONTINUITY_THRESHOLD);
			INFO("  Found " << discontinuity_indices.size() << " discontinuities");
			INFO("  Loop count: " << loop_count);
			
			// When looping, small discontinuities at wrap points are expected
			// Allow up to loop_count discontinuities (one per loop wrap)
			REQUIRE(discontinuity_indices.size() <= loop_count + 2); // +2 for safety margin
		}
	}
	
	if (is_csv_output_enabled()) {
		SECTION("Export loop playback to CSV") {
			std::string csv_output_dir = "build/tests/csv_output";
			system(("mkdir -p " + csv_output_dir).c_str());
			
			std::string csv_filename = csv_output_dir + "/wav_load_loop_playback.csv";
			
			CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
			REQUIRE(csv_writer.is_open());
			csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
			csv_writer.close();
			
			INFO("CSV file written to: " << csv_filename);
		}
	}
	
	delete global_time;
}
TEST_CASE("AudioTape - load from WAV file with start and end points", "[audio_tape][wav_load][range][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float WINDOW_SIZE_SECONDS = 2.0f;
	const std::string wav_file = "media/test.wav";
	
	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	SECTION("Load portion of WAV file (1.0 to 3.0 seconds)") {
		constexpr float start_seconds = 1.0f;
		constexpr float end_seconds = 3.0f;
		
		// Load WAV file portion into AudioTape
		auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE, start_seconds, end_seconds);
		REQUIRE(tape != nullptr);
		REQUIRE(tape->size() > 0);
		
		const unsigned int num_channels = tape->num_channels();
		const unsigned int tape_size_samples = tape->size();
		const float expected_duration = end_seconds - start_seconds;
		const unsigned int expected_samples = static_cast<unsigned int>(expected_duration * SAMPLE_RATE);
		
		INFO("Loaded tape with " << num_channels << " channels, " << tape_size_samples << " samples per channel");
		INFO("Expected duration: " << expected_duration << " seconds, " << expected_samples << " samples");
		
		// Verify the tape size matches the expected range
		REQUIRE(std::abs(static_cast<int>(tape_size_samples) - static_cast<int>(expected_samples)) < 100);
		REQUIRE(tape->size_in_seconds() >= expected_duration * 0.95f);
		REQUIRE(tape->size_in_seconds() <= expected_duration * 1.05f);
		
		// Create mock playback stage
		MockTapePlaybackStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels, WINDOW_SIZE_SECONDS);
		playback_stage.get_history().set_tape(tape);
		
		// Create final render stage
		AudioFinalRenderStage final_stage(BUFFER_SIZE, SAMPLE_RATE, num_channels);
		
		// Connect and initialize
		REQUIRE(playback_stage.connect_render_stage(&final_stage));
		REQUIRE(playback_stage.initialize());
		REQUIRE(final_stage.initialize());
		
		context.prepare_draw();
		REQUIRE(playback_stage.bind());
		REQUIRE(final_stage.bind());
		
		// Set up audio output
		AudioPlayerOutput* audio_output = nullptr;
		if (is_audio_output_enabled()) {
			audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, num_channels);
			REQUIRE(audio_output->open());
			REQUIRE(audio_output->start());
		}
		
		// Set playback speed and start position
		playback_stage.get_history().set_tape_speed(1.0f);
		playback_stage.get_history().set_tape_position(0u);
		playback_stage.get_history().start_tape();
		playback_stage.play();
		
		// Calculate how many frames to render
		const unsigned int frames_to_render = (tape_size_samples / BUFFER_SIZE) + 10;
		
		// Collect output samples
		std::vector<std::vector<float>> output_samples_per_channel(num_channels);
		for (unsigned int ch = 0; ch < num_channels; ++ch) {
			output_samples_per_channel[ch].reserve(frames_to_render * BUFFER_SIZE);
		}
		
		// Render frames
		unsigned int frame = 0;
		for (unsigned int i = 0; i < frames_to_render; ++i) {
			global_time->set_value(frame);
			global_time->render();
			
			playback_stage.render(frame);
			final_stage.render(frame);
			
			// Extract output samples
			auto* output_param = final_stage.find_parameter("final_output_audio_texture");
			if (output_param) {
				const float* output_data = static_cast<const float*>(output_param->get_value());
				REQUIRE(output_data != nullptr);
				
				for (int j = 0; j < BUFFER_SIZE * num_channels; ++j) {
					auto channel = j % num_channels;
					output_samples_per_channel[channel].push_back(output_data[j]);
				}
				
				if (audio_output) {
					while (!audio_output->is_ready()) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
					audio_output->push(output_data);
				}
			}
			
			if (playback_stage.get_history().is_tape_stopped() || 
			    playback_stage.get_history().is_tape_at_end()) {
				break;
			}
			
			frame++;
		}
		
		if (audio_output) {
			audio_output->stop();
			audio_output->close();
			delete audio_output;
		}
		
		REQUIRE(output_samples_per_channel[0].size() > 0);
	}
	
	SECTION("Load from start only (first 2 seconds)") {
		constexpr float end_seconds = 2.0f;
		
		auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE, std::nullopt, end_seconds);
		REQUIRE(tape != nullptr);
		REQUIRE(tape->size() > 0);
		
		const unsigned int expected_samples = static_cast<unsigned int>(end_seconds * SAMPLE_RATE);
		const unsigned int tape_size_samples = tape->size();
		
		REQUIRE(std::abs(static_cast<int>(tape_size_samples) - static_cast<int>(expected_samples)) < 100);
		REQUIRE(tape->size_in_seconds() >= end_seconds * 0.95f);
		REQUIRE(tape->size_in_seconds() <= end_seconds * 1.05f);
	}
	
	SECTION("Load from middle point to end") {
		constexpr float start_seconds = 2.0f;
		
		auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE, start_seconds, std::nullopt);
		REQUIRE(tape != nullptr);
		REQUIRE(tape->size() > 0);
		
		REQUIRE(tape->size() > 0);
		REQUIRE(tape->size_in_seconds() > 0.0f);
	}
	
	delete global_time;
}

TEST_CASE("AudioTape - export to WAV file", "[audio_tape][wav_export][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int SAMPLE_RATE = 44100;
	const std::string wav_file = "media/test.wav";
	const std::string output_dir = "build/tests/tape_exports";
	
	// Create output directory
	system(("mkdir -p " + output_dir).c_str());
	
	SECTION("Export full tape loaded from WAV file") {
		// Load WAV file into AudioTape
		auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE);
		REQUIRE(tape != nullptr);
		REQUIRE(tape->size() > 0);
		
		const unsigned int num_channels = tape->num_channels();
		const unsigned int tape_size_samples = tape->size();
		const float tape_duration = tape->size_in_seconds();
		
		// Export to WAV file
		const std::string output_file = output_dir + "/exported_full_tape.wav";
		bool export_success = tape->export_to_wav_file(output_file);
		REQUIRE(export_success == true);
		
		// Verify file was created
		REQUIRE(std::filesystem::exists(output_file));
		
		// Validate WAV header
		REQUIRE(validate_wav_header(output_file, num_channels, SAMPLE_RATE, 16));
		
		// Read back the exported file
		auto exported_data = read_wav_audio_data(output_file);
		REQUIRE(!exported_data.empty());
		
		// Verify sample count matches
		const unsigned int expected_samples = tape_size_samples * num_channels;
		REQUIRE(exported_data.size() == expected_samples);
		
		// Verify we can load it back
		auto reloaded_tape = AudioTape::load_from_wav_file(output_file, BUFFER_SIZE, SAMPLE_RATE);
		REQUIRE(reloaded_tape != nullptr);
		REQUIRE(reloaded_tape->size() == tape_size_samples);
		REQUIRE(reloaded_tape->num_channels() == num_channels);
		REQUIRE(reloaded_tape->sample_rate() == SAMPLE_RATE);
	}
	
	SECTION("Export portion of tape (with start/end points)") {
		constexpr float start_seconds = 1.0f;
		constexpr float end_seconds = 3.0f;
		
		// Load portion of WAV file
		auto tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE, start_seconds, end_seconds);
		REQUIRE(tape != nullptr);
		REQUIRE(tape->size() > 0);
		
		const unsigned int num_channels = tape->num_channels();
		const unsigned int tape_size_samples = tape->size();
		const float expected_duration = end_seconds - start_seconds;
		
		// Export to WAV file
		const std::string output_file = output_dir + "/exported_portion_tape.wav";
		bool export_success = tape->export_to_wav_file(output_file);
		REQUIRE(export_success == true);
		
		// Verify file was created
		REQUIRE(std::filesystem::exists(output_file));
		
		// Validate WAV header
		REQUIRE(validate_wav_header(output_file, num_channels, SAMPLE_RATE, 16));
		
		// Verify we can load it back
		auto reloaded_tape = AudioTape::load_from_wav_file(output_file, BUFFER_SIZE, SAMPLE_RATE);
		REQUIRE(reloaded_tape != nullptr);
		REQUIRE(reloaded_tape->size() == tape_size_samples);
		REQUIRE(std::abs(reloaded_tape->size_in_seconds() - expected_duration) < 0.1f);
	}
	
	SECTION("Export recorded tape") {
		constexpr int NUM_CHANNELS = 2;
		constexpr float TEST_FREQUENCY = 440.0f;
		constexpr float AMPLITUDE = 0.5f;
		constexpr int RECORD_DURATION_SECONDS = 2;
		constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
		
		// Create empty tape
		auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
		
		// Record sine wave to tape
		std::vector<float> phases(NUM_CHANNELS, 0.0f);
		for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
			std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, AMPLITUDE, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
				for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
					channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
				}
				phases[ch] += BUFFER_SIZE;
			}
			tape->record(channel_major_buffer.data());
		}
		
		REQUIRE(tape->size() > 0);
		
		// Export to WAV file
		const std::string output_file = output_dir + "/exported_recorded_tape.wav";
		bool export_success = tape->export_to_wav_file(output_file);
		REQUIRE(export_success == true);
		
		// Verify file was created
		REQUIRE(std::filesystem::exists(output_file));
		
		// Validate WAV header
		REQUIRE(validate_wav_header(output_file, NUM_CHANNELS, SAMPLE_RATE, 16));
		
		// Read back and verify it contains the expected frequency
		auto exported_data = read_wav_audio_data(output_file);
		REQUIRE(!exported_data.empty());
		REQUIRE(detect_frequency_int16(exported_data, TEST_FREQUENCY, SAMPLE_RATE, NUM_CHANNELS, 0.1f));
	}
	
	SECTION("Export empty tape (should fail)") {
		auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, 2);
		
		const std::string output_file = output_dir + "/exported_empty_tape.wav";
		bool export_success = tape->export_to_wav_file(output_file);
		REQUIRE(export_success == false);
		
		// File should not be created
		REQUIRE(!std::filesystem::exists(output_file));
	}
	
	SECTION("Round-trip test: load -> export -> load") {
		// Load original file
		auto original_tape = AudioTape::load_from_wav_file(wav_file, BUFFER_SIZE, SAMPLE_RATE);
		REQUIRE(original_tape != nullptr);
		const unsigned int original_size = original_tape->size();
		const unsigned int original_channels = original_tape->num_channels();
		
		// Export
		const std::string output_file = output_dir + "/round_trip_test.wav";
		bool export_success = original_tape->export_to_wav_file(output_file);
		REQUIRE(export_success == true);
		
		// Load back
		auto reloaded_tape = AudioTape::load_from_wav_file(output_file, BUFFER_SIZE, SAMPLE_RATE);
		REQUIRE(reloaded_tape != nullptr);
		
		// Verify properties match
		REQUIRE(reloaded_tape->size() == original_size);
		REQUIRE(reloaded_tape->num_channels() == original_channels);
		REQUIRE(reloaded_tape->sample_rate() == SAMPLE_RATE);
		
		// Compare sample data by playing back and comparing
		// Use playback method to access data (since m_data is private)
		const unsigned int compare_samples = std::min(original_size, reloaded_tape->size());
		const unsigned int compare_frames = (compare_samples / BUFFER_SIZE) + 1;
		
		for (unsigned int frame = 0; frame < compare_frames; ++frame) {
			unsigned int offset = frame * BUFFER_SIZE;
			if (offset >= compare_samples) break;
			
			unsigned int frames_to_compare = std::min(static_cast<unsigned int>(BUFFER_SIZE), compare_samples - offset);
			
			auto original_playback = original_tape->playback(frames_to_compare, offset, false);
			auto reloaded_playback = reloaded_tape->playback(frames_to_compare, offset, false);
			
			REQUIRE(original_playback.size() == reloaded_playback.size());
			
			const float tolerance = 0.01f; // Allow tolerance for int16 conversion
			for (size_t i = 0; i < original_playback.size(); ++i) {
				REQUIRE(std::abs(original_playback[i] - reloaded_playback[i]) < tolerance);
			}
		}
	}
}
