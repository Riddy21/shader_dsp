// Mock-stage focused test for AudioRenderStageHistory without chaining to final output

#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"
#include "framework/csv_test_output.h"
#include "framework/test_main.h"

#define private public
#include "audio_render_stage/audio_render_stage_history.h"
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

		m_history2->update_audio_history_texture();
		AudioRenderStage::render(time);
		m_history2->set_tape_position(m_history2->get_tape_position() + m_history2->get_tape_speed_samples_per_buffer());
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

// Test playback at different positions

// Test playback with different sized tapes

// Test discontinuities with different window sizes
	

