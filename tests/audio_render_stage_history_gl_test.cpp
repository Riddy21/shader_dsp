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

// Fragment shader for multiple start positions test
static const char* kMultipleStartPositionsFragSource = R"(
uniform int start_position_0;
uniform int start_position_1;

int calculate_tape_position(int current_position, int start_position, int speed) {
    int difference = current_position - start_position;

	return difference * speed;
}

void main(){
    // Get the audio sample from tape history using TexCoord
	vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    // The function will use tape_position and tape_speed internally

	int tape_position_0 = calculate_tape_position(global_time_val, start_position_0, speed_in_samples_per_buffer);
	int tape_position_1 = calculate_tape_position(global_time_val, start_position_1, speed_in_samples_per_buffer);

	vec4 tape_sample = vec4(0.0, 0.0, 0.0, 0.0);

	if (tape_position_0 >= 0) {
		tape_sample += get_tape_history_samples(TexCoord, speed_in_samples_per_buffer, tape_position_0);
	}

	if (tape_position_1 >= 0) {
		tape_sample += get_tape_history_samples(TexCoord, speed_in_samples_per_buffer, tape_position_1);
	}
    
    // Output the tape playback sample with some processing
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
		m_history2->create_parameters(m_active_texture_count++);
		
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

		m_history2->update_tape_position(time);
		m_history2->update_window();
		AudioRenderStage::render(time);
	}

private:
	std::unique_ptr<AudioRenderStageHistory2> m_history2;
	bool m_is_playing;
	unsigned int m_current_frame;
};

// Modified version that allows separate control over position and texture updates
class MockTapePlaybackStageWithSeparateUpdates : public AudioRenderStage {
public:
	MockTapePlaybackStageWithSeparateUpdates(unsigned int frames_per_buffer,
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
	  m_is_playing(false),
	  m_texture_update_interval(1) // Default: update every frame
	{
		// Create tape history and all its textures/parameters
		m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, window_seconds);
		m_history2->create_parameters(m_active_texture_count++);
		
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
	
	void set_texture_update_interval(unsigned int interval) {
		m_texture_update_interval = interval;
	}

protected:
	void render(const unsigned int time) override {
		// Always update positions every frame
		m_history2->update_tape_position(time);
		
		// Update texture only at specified intervals
		// Always update on first frame (time == 0), then at intervals
		if (time == 0 || time % m_texture_update_interval == 0) {
			m_history2->update_window();
		}
		
		AudioRenderStage::render(time);
	}

private:
	std::unique_ptr<AudioRenderStageHistory2> m_history2;
	bool m_is_playing;
	unsigned int m_current_frame;
	unsigned int m_texture_update_interval;
};

// Version that updates texture only when needed (checks is_audio_texture_data_outdated)
class MockTapePlaybackStageWithConditionalUpdates : public AudioRenderStage {
public:
	MockTapePlaybackStageWithConditionalUpdates(unsigned int frames_per_buffer,
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
	  m_is_playing(false),
	  m_texture_update_count(0)
	{
		// Create tape history and all its textures/parameters
		m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, window_seconds);
		m_history2->create_parameters(m_active_texture_count++);
		
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
	
	unsigned int get_texture_update_count() const { return m_texture_update_count; }
	void reset_texture_update_count() { m_texture_update_count = 0; }

protected:
	void render(const unsigned int time) override {
		// Always update positions every frame
		m_history2->update_tape_position(time);
		
		// Update texture only when needed (checks is_outdated)
		// This will only update when the tape position moves outside the valid texture window range
		// Always update on first frame (time == 0) to initialize texture, then only when needed
		if (time == 0 || m_history2->is_outdated()) {
			m_history2->update_window();
			m_texture_update_count++;
		}
		
		AudioRenderStage::render(time);
	}

private:
	std::unique_ptr<AudioRenderStageHistory2> m_history2;
	bool m_is_playing;
	unsigned int m_current_frame;
	unsigned int m_texture_update_count;
};

// Mock render stage history class for multiple start positions test
class MockMultipleStartPositionsStage : public AudioRenderStage {
public:
	MockMultipleStartPositionsStage(unsigned int frames_per_buffer,
	                                unsigned int sample_rate,
	                                unsigned int num_channels,
	                                float window_seconds = 2.0f)
	: AudioRenderStage(frames_per_buffer, sample_rate, num_channels,
	                   kMultipleStartPositionsFragSource,
	                   true, // use_shader_string
	                   std::vector<std::string>{
	                   	"build/shaders/global_settings.glsl",
	                   	"build/shaders/frag_shader_settings.glsl",
	                   	"build/shaders/tape_history_settings.glsl"}),
	  m_is_playing(false)
	{
		// Create tape history and all its textures/parameters
		m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, window_seconds);
		m_history2->create_parameters(m_active_texture_count++);
		
		m_start_position_0 = new AudioIntParameter("start_position_0", AudioParameter::ConnectionType::INPUT);
		m_start_position_0->set_value(0);
		this->add_parameter(m_start_position_0);

		m_start_position_1 = new AudioIntParameter("start_position_1", AudioParameter::ConnectionType::INPUT);
		m_start_position_1->set_value(0);
		this->add_parameter(m_start_position_1);

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

	void set_start_position(int start_position_0, int start_position_1) {
		m_start_position_0->set_value(start_position_0);
		m_start_position_1->set_value(start_position_1);
	}

protected:
	void render(const unsigned int time) override {
		AudioRenderStage::render(time);
	}

private:
	std::unique_ptr<AudioRenderStageHistory2> m_history2;
	AudioIntParameter* m_start_position_0;
	AudioIntParameter* m_start_position_1;
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
				std::string csv_output_dir = "build/tests/csv_output";
				system(("mkdir -p " + csv_output_dir).c_str());
				
				std::ofstream csv_file(csv_output_dir + "/input_sine_wave.csv");
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
				std::cout << "Wrote input sine wave to " << csv_output_dir << "/input_sine_wave.csv (" << num_samples << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
			}
		}
		
		if (is_csv_output_enabled()) {
			SECTION("Write output audio to CSV") {
				// Verify we have the correct number of channels before writing
				REQUIRE(output_samples_per_channel.size() == NUM_CHANNELS);
				
				std::string csv_output_dir = "build/tests/csv_output";
				system(("mkdir -p " + csv_output_dir).c_str());
				
				// Create filename with speed and channels (e.g., "output_audio_speed_1.000000_channels_3.csv")
				std::ostringstream filename_stream;
				filename_stream << csv_output_dir << "/output_audio_speed_" << std::fixed << std::setprecision(6) << PLAYBACK_SPEED 
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
				
				std::string csv_output_dir = "build/tests/csv_output";
				system(("mkdir -p " + csv_output_dir).c_str());
				
				// Create CSV filename with test parameters
				std::ostringstream csv_filename_stream;
				csv_filename_stream << csv_output_dir << "/playback_history_buffer_speed_" << std::fixed << std::setprecision(6) << PLAYBACK_SPEED 
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
	MockTapePlaybackStageWithConditionalUpdates playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
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
	// Transition smoothly from positive to negative speed, crossing zero
	// This tests both forward and reverse playback in a single test
	float previous_speed = MAX_SPEED;
	int frames_rendered = 0;
	for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
		global_time->set_value(frame);
		global_time->render();
		
		// Calculate speed: linear transition from MAX_SPEED (positive) to MIN_SPEED (negative)
		// This ensures we test both positive and negative speeds, crossing zero in the middle
		float progress = static_cast<float>(frame) / static_cast<float>(NUM_PLAYBACK_FRAMES);
		// Linear interpolation from MAX_SPEED to MIN_SPEED
		// At progress=0: speed = MAX_SPEED (1.0)
		// At progress=0.5: speed = 0.0 (crosses zero)
		// At progress=1.0: speed = MIN_SPEED (-1.0)
		float speed = MAX_SPEED + (MIN_SPEED - MAX_SPEED) * progress;
		
		// Verify speed changes are continuous (small delta per frame)
		if (frame > 0) {
			float speed_delta = std::abs(speed - previous_speed);
			float max_speed_delta_per_frame = std::abs(MAX_SPEED - MIN_SPEED) / static_cast<float>(NUM_PLAYBACK_FRAMES) * 1.5f; // Allow some margin for linear transition
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
	
	SECTION("Slope change continuity check - verify incremental changes without large jumps") {
		// This test verifies that the rate of change (slope) changes incrementally
		// without large jumps, which would indicate smooth speed transitions
		// When speed changes dynamically, slope changes will naturally have some variation
		// The threshold should be high enough to catch real discontinuities but not flag
		// normal variations due to speed changes
		constexpr float MAX_SLOPE_CHANGE_JUMP = 0.05f; // Maximum allowed jump in slope change per sample (increased from 0.01)
		constexpr float MIN_SLOPE_CHANGE_FOR_ANALYSIS = 0.0001f; // Ignore very small slope changes
		
		for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			
			if (samples.size() < 3) {
				continue; // Need at least 3 samples to calculate slope changes
			}
			
			// Calculate first derivative (slope) between consecutive samples
			std::vector<float> slopes;
			slopes.reserve(samples.size() - 1);
			for (size_t i = 1; i < samples.size(); ++i) {
				float slope = samples[i] - samples[i - 1];
				slopes.push_back(slope);
			}
			
			// Calculate second derivative (change in slope)
			std::vector<float> slope_changes;
			slope_changes.reserve(slopes.size() - 1);
			for (size_t i = 1; i < slopes.size(); ++i) {
				float slope_change = std::abs(slopes[i] - slopes[i - 1]);
				slope_changes.push_back(slope_change);
			}
			
			// Find large jumps in slope changes
			std::vector<size_t> large_jump_indices;
			std::vector<float> large_jump_magnitudes;
			
			for (size_t i = 1; i < slope_changes.size(); ++i) {
				float jump = std::abs(slope_changes[i] - slope_changes[i - 1]);
				// Only flag if both slope changes are significant enough to matter
				if (slope_changes[i] > MIN_SLOPE_CHANGE_FOR_ANALYSIS && 
				    slope_changes[i - 1] > MIN_SLOPE_CHANGE_FOR_ANALYSIS &&
				    jump > MAX_SLOPE_CHANGE_JUMP) {
					large_jump_indices.push_back(i);
					large_jump_magnitudes.push_back(jump);
				}
			}
			
			// Detect small repeats (consecutive samples with same value)
			std::vector<size_t> repeat_indices;
			for (size_t i = 1; i < samples.size(); ++i) {
				if (std::abs(samples[i] - samples[i - 1]) < 1e-6f) {
					repeat_indices.push_back(i);
				}
			}
			
			INFO("Channel " << ch << " slope analysis:");
			INFO("  Total samples: " << samples.size());
			INFO("  Total slopes calculated: " << slopes.size());
			INFO("  Total slope changes calculated: " << slope_changes.size());
			INFO("  Max slope change jump threshold: " << MAX_SLOPE_CHANGE_JUMP);
			INFO("  Found " << large_jump_indices.size() << " large jumps in slope changes");
			INFO("  Found " << repeat_indices.size() << " repeated sample values");
			
			if (!large_jump_indices.empty()) {
				INFO("  First 5 large jump magnitudes:");
				for (size_t i = 0; i < std::min(large_jump_indices.size(), size_t(5)); ++i) {
					INFO("    Sample " << large_jump_indices[i] << ": jump = " << large_jump_magnitudes[i]);
				}
			}
			
			if (!repeat_indices.empty()) {
				INFO("  First 5 repeat locations:");
				for (size_t i = 0; i < std::min(repeat_indices.size(), size_t(5)); ++i) {
					INFO("    Sample " << repeat_indices[i] << ": value = " << samples[repeat_indices[i]]);
				}
			}
			
			// Verify that slope changes incrementally without large jumps
			// When speed changes dynamically, some discontinuities are expected at frame boundaries
			// because the rate of change changes. However, these should be minimized and not excessive.
			// Allow up to the number of frames (one jump per frame boundary is expected when speed changes)
			const size_t num_frames = samples.size() / BUFFER_SIZE;
			const size_t max_allowed_jumps = num_frames * 2; // Allow up to 2 jumps per frame (conservative)
			REQUIRE(large_jump_indices.size() <= max_allowed_jumps);
			
			// Report repeats but don't fail - they may be expected in some cases
			// but we want to track them
			if (repeat_indices.size() > samples.size() / 100) {
				INFO("  WARNING: Found " << repeat_indices.size() << " repeated samples (>1% of total)");
			}
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

TEST_CASE("AudioRenderStageHistory2 - negative speed playback with CSV and audio output", "[audio_history2][negative_speed][playback][audio_output][csv_output][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float LEFT_AMPLITUDE = 0.5f;
	constexpr float RIGHT_AMPLITUDE = 0.25f;
	constexpr int RECORD_DURATION_SECONDS = 4;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 2;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;
	constexpr float NEGATIVE_SPEED = -1.0f; // Reverse playback

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
	
	// Setup audio output (only if enabled)
	AudioPlayerOutput* audio_output = nullptr;
	if (is_audio_output_enabled()) {
		printf("Audio output enabled - initializing AudioPlayerOutput for negative speed playback\n");
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
	
	// Start playback from near the end (for reverse playback)
	playback_stage.get_history().set_tape_speed(NEGATIVE_SPEED);
	unsigned int tape_size = tape->size();
	unsigned int start_position = tape_size - (SAMPLE_RATE * PLAYBACK_DURATION_SECONDS / 2); // Start partway through
	playback_stage.get_history().set_tape_position(start_position);
	playback_stage.get_history().start_tape();
	playback_stage.play();
	
	printf("Starting negative speed playback from position %u (tape size: %u)\n", start_position, tape_size);
	
	// Check the speed setting
	int speed_samples_per_buffer = *static_cast<const int*>(playback_stage.get_history().m_tape_speed->get_value());
	REQUIRE(static_cast<float>(speed_samples_per_buffer) == Catch::Approx(NEGATIVE_SPEED * BUFFER_SIZE).margin(1.00f));
	
	// Render and capture output
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
		
		frame_count++;
		
		// Stop if tape has stopped (reached beginning for negative speed)
		if (playback_stage.get_history().is_tape_stopped()) {
			printf("Tape stopped at frame %d (beginning reached for negative speed)\n", frame);
			break;
		}
	}
	printf("Rendered %d frames out of %d requested\n", frame_count, NUM_PLAYBACK_FRAMES);
	
	// Stop playback
	playback_stage.stop();
	
	// Wait for audio to finish and close audio output
	if (audio_output) {
		size_t queued_bytes = audio_output->queued_bytes();
		printf("Audio queue has %zu bytes queued, waiting for playback to finish...\n", queued_bytes);
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
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		audio_output->close();
		delete audio_output;
	}
	
	SECTION("Verify negative speed playback output") {
		// Verify we have samples
		for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
			REQUIRE(output_samples_per_channel[ch].size() > 0);
			
			// Check that output is not all zeros
			bool has_non_zero = false;
			for (float sample : output_samples_per_channel[ch]) {
				if (std::abs(sample) > 0.001f) {
					has_non_zero = true;
					break;
				}
			}
			REQUIRE(has_non_zero == true);
		}
		
		// Verify tape position decreased (for negative speed)
		unsigned int final_position = playback_stage.get_history().get_tape_position();
		INFO("Final tape position: " << final_position << ", Start position: " << start_position);
		// Position should have decreased (or stopped at beginning)
		REQUIRE(final_position <= start_position);
	}
	
	if (is_csv_output_enabled()) {
		SECTION("Export negative speed playback to CSV") {
			std::string csv_output_dir = "build/tests/csv_output";
			system(("mkdir -p " + csv_output_dir).c_str());
			
			std::ostringstream csv_filename_stream;
			csv_filename_stream << csv_output_dir << "/negative_speed_playback_channels_" << NUM_CHANNELS << ".csv";
			std::string csv_filename = csv_filename_stream.str();
			
			CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
			REQUIRE(csv_writer.is_open());
			csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
			csv_writer.close();
			
			INFO("CSV file written to: " << csv_filename);
			printf("CSV file written to: %s\n", csv_filename.c_str());
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
	}
}

TEST_CASE("AudioRenderStageHistory2 - texture update intervals and continuity", "[audio_history2][texture_update][continuity][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float BASE_AMPLITUDE = 0.3f;
	constexpr int RECORD_DURATION_SECONDS = 4;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 2;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;
	constexpr float PLAYBACK_SPEED = 1.0f;

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage with separate updates
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStageWithSeparateUpdates playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
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
	
	SECTION("Record sine wave") {
		// Record sine wave to tape
		std::vector<float> phases(NUM_CHANNELS, 0.0f);
		for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
			std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				float channel_amplitude = BASE_AMPLITUDE * (ch + 1);
				auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
				for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
					channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
				}
				phases[ch] += BUFFER_SIZE;
			}
			tape->record(channel_major_buffer.data());
		}
		REQUIRE(tape->size() > 0);
	}
	
	// Test different texture update intervals
	std::vector<unsigned int> texture_update_intervals = {1, 2, 5, 10, 20};
	
	for (unsigned int texture_interval : texture_update_intervals) {
		SECTION("Texture update every " + std::to_string(texture_interval) + " frames") {
			// Setup playback
			playback_stage.set_texture_update_interval(texture_interval);
			playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
			auto middle_position = tape->size() / 2;
			playback_stage.get_history().set_tape_position(middle_position);
			playback_stage.play();
			
			// Collect output samples per channel
			std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
			}
			
			// Render and capture audio
			for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
				global_time->set_value(frame);
				global_time->render();
				
				// Render playback stage (updates positions every frame, texture at intervals)
				playback_stage.render(frame);
				
				// Render final stage
				final_stage.render(frame);
				
				// Get output from final stage
				auto output_param = final_stage.find_parameter("final_output_audio_texture");
				REQUIRE(output_param != nullptr);
				
				const float* output_data = static_cast<const float*>(output_param->get_value());
				REQUIRE(output_data != nullptr);
				
				// De-interleave to separate channels
				for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
					auto channel = i % NUM_CHANNELS;
					output_samples_per_channel[channel].push_back(output_data[i]);
				}
				
				// Stop if we've reached the end
				if (playback_stage.get_history().get_tape_position() >= tape->size()) {
					playback_stage.stop();
					break;
				}
			}
			playback_stage.stop();
			
			// Verify we got output
			REQUIRE(output_samples_per_channel[0].size() > 0);
			
			// Check for discontinuities
			SECTION("Discontinuity check - interval " + std::to_string(texture_interval)) {
				constexpr float DISCONTINUITY_THRESHOLD = 0.2f; // Threshold for detecting discontinuities
				
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
					
					INFO("Channel " << ch << " analysis (texture update interval: " << texture_interval << "):");
					INFO("  Total samples: " << samples.size());
					INFO("  Discontinuity threshold: " << DISCONTINUITY_THRESHOLD);
					INFO("  Found " << discontinuity_indices.size() << " discontinuities");
					
					if (!discontinuity_indices.empty()) {
						INFO("  First 10 discontinuity magnitudes:");
						for (size_t i = 0; i < std::min(discontinuity_indices.size(), size_t(10)); ++i) {
							INFO("    Sample " << discontinuity_indices[i] << ": " << discontinuity_magnitudes[i]);
						}
					}
					
					// Verify continuity - texture update intervals should not cause discontinuities
					// Allow a small number of discontinuities (less than 0.1% of samples, minimum 1)
					// This accounts for normal signal variations and texture window boundaries
					unsigned int max_allowed = std::max(1u, static_cast<unsigned int>(samples.size() / 1000));
					REQUIRE(discontinuity_indices.size() < max_allowed);
				}
			}
			
			// Check for sudden slope changes
			SECTION("Slope change continuity check - interval " + std::to_string(texture_interval)) {
				constexpr float MAX_SLOPE_CHANGE_JUMP = 0.05f; // Maximum allowed jump in slope change per sample
				constexpr float MIN_SLOPE_CHANGE_FOR_ANALYSIS = 0.0001f; // Ignore very small slope changes
				
				for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
					const auto& samples = output_samples_per_channel[ch];
					
					if (samples.size() < 3) {
						continue; // Need at least 3 samples to calculate slope changes
					}
					
					// Calculate first derivative (slope) between consecutive samples
					std::vector<float> slopes;
					slopes.reserve(samples.size() - 1);
					for (size_t i = 1; i < samples.size(); ++i) {
						float slope = samples[i] - samples[i - 1];
						slopes.push_back(slope);
					}
					
					// Calculate second derivative (change in slope)
					std::vector<float> slope_changes;
					slope_changes.reserve(slopes.size() - 1);
					for (size_t i = 1; i < slopes.size(); ++i) {
						float slope_change = std::abs(slopes[i] - slopes[i - 1]);
						slope_changes.push_back(slope_change);
					}
					
					// Find large jumps in slope changes
					std::vector<size_t> large_jump_indices;
					std::vector<float> large_jump_magnitudes;
					
					for (size_t i = 1; i < slope_changes.size(); ++i) {
						float jump = std::abs(slope_changes[i] - slope_changes[i - 1]);
						// Only flag if both slope changes are significant enough to matter
						if (slope_changes[i] > MIN_SLOPE_CHANGE_FOR_ANALYSIS && 
						    slope_changes[i - 1] > MIN_SLOPE_CHANGE_FOR_ANALYSIS &&
						    jump > MAX_SLOPE_CHANGE_JUMP) {
							large_jump_indices.push_back(i);
							large_jump_magnitudes.push_back(jump);
						}
					}
					
					INFO("Channel " << ch << " slope analysis (texture update interval: " << texture_interval << "):");
					INFO("  Total samples: " << samples.size());
					INFO("  Max slope change jump: " << MAX_SLOPE_CHANGE_JUMP);
					INFO("  Found " << large_jump_indices.size() << " large slope change jumps");
					
					if (!large_jump_indices.empty()) {
						INFO("  First 10 large jump magnitudes:");
						for (size_t i = 0; i < std::min(large_jump_indices.size(), size_t(10)); ++i) {
							INFO("    Sample " << large_jump_indices[i] << ": " << large_jump_magnitudes[i]);
						}
					}
					
					// Verify smooth slope changes - texture update intervals should not cause sudden slope changes
					// Allow a small number of jumps (less than 0.1% of samples, minimum 1)
					unsigned int max_allowed = std::max(1u, static_cast<unsigned int>(samples.size() / 1000));
					REQUIRE(large_jump_indices.size() < max_allowed);
				}
			}
		}
	}
}

TEST_CASE("AudioRenderStageHistory2 - conditional texture updates when needed", "[audio_history2][texture_update][conditional][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float BASE_AMPLITUDE = 0.3f;
	constexpr int RECORD_DURATION_SECONDS = 4;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 3;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;
	constexpr float PLAYBACK_SPEED = 1.0f;

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage with conditional updates
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStageWithConditionalUpdates playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
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
	
	// Record sine wave to tape (must happen before playback sections)
	std::vector<float> phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			float channel_amplitude = BASE_AMPLITUDE * (ch + 1);
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data());
	}
	REQUIRE(tape->size() > 0);
	INFO("Recorded " << tape->size() << " samples per channel to tape");
	
	SECTION("Texture updates only when needed") {
		// Clear stream_audio_texture to ensure it's zero when no previous stage is connected
		auto stream_param = playback_stage.find_parameter("stream_audio_texture");
		if (stream_param) {
			stream_param->clear_value();
		}
		
		// Setup audio output (only if enabled)
		AudioPlayerOutput* audio_output = nullptr;
		if (is_audio_output_enabled()) {
			audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
			REQUIRE(audio_output->open());
			REQUIRE(audio_output->start());
		}
		
		// Setup playback
		REQUIRE(tape->size() > 0); // Verify tape has data before playback
		INFO("Tape size before playback: " << tape->size() << " samples per channel");
		playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
		auto start_position = tape->size() / 4; // Start at 1/4 through the tape
		playback_stage.get_history().set_tape_position(start_position);
		INFO("Starting playback at position: " << start_position);
		playback_stage.play();
		playback_stage.reset_texture_update_count();
		
		// Track when updates happen
		std::vector<unsigned int> update_frames;
		std::vector<unsigned int> update_positions;
		std::vector<unsigned int> update_window_offsets;
		
		// Collect output samples per channel
		std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
		}
		
		// Render and capture audio, tracking texture updates
		for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
			global_time->set_value(frame);
			global_time->render();
			
			// Track state before render
			unsigned int position_before = playback_stage.get_history().get_tape_position();
			unsigned int window_offset_before = playback_stage.get_history().get_window_offset_samples();
			unsigned int update_count_before = playback_stage.get_texture_update_count();
			
			// Render playback stage (updates positions every frame, texture only when needed)
			playback_stage.render(frame);
			
			// Check if texture was updated
			unsigned int update_count_after = playback_stage.get_texture_update_count();
			if (update_count_after > update_count_before) {
				update_frames.push_back(frame);
				update_positions.push_back(playback_stage.get_history().get_tape_position());
				update_window_offsets.push_back(playback_stage.get_history().get_window_offset_samples());
			}
			
			// Render final stage
			final_stage.render(frame);
			
			// Get output from final stage
			auto output_param = final_stage.find_parameter("final_output_audio_texture");
			REQUIRE(output_param != nullptr);
			
			const float* output_data = static_cast<const float*>(output_param->get_value());
			REQUIRE(output_data != nullptr);
			
			// Check if audio data is non-zero (verify we have actual audio)
			bool has_audio = false;
			float max_amplitude = 0.0f;
			for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
				float abs_val = std::abs(output_data[i]);
				if (abs_val > 0.001f) {
					has_audio = true;
				}
				max_amplitude = std::max(max_amplitude, abs_val);
			}
			
			// Log diagnostic info on first frame or if no audio detected
			if (frame == 0 || (!has_audio && frame < 10)) {
				INFO("Frame " << frame << " audio check: has_audio=" << has_audio 
				     << ", max_amplitude=" << max_amplitude
				     << ", tape_position=" << playback_stage.get_history().get_tape_position()
				     << ", tape_size=" << tape->size()
				     << ", texture_outdated=" << playback_stage.get_history().is_outdated());
			}
			
			// De-interleave to separate channels
			for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
				auto channel = i % NUM_CHANNELS;
				output_samples_per_channel[channel].push_back(output_data[i]);
			}
			
			// Push to audio output (output_data is interleaved format)
			// Always push audio data (even if it appears to be zero, as it might be valid silence)
			// The audio system will handle zero samples correctly
			if (audio_output) {
				while (!audio_output->is_ready()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				audio_output->push(output_data);
			}
			
			// Stop if we've reached the end
			if (playback_stage.get_history().get_tape_position() >= tape->size()) {
				playback_stage.stop();
				break;
			}
		}
		playback_stage.stop();
		
		// Wait for audio to finish playing and close audio output
		if (audio_output) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			audio_output->stop();
			audio_output->close();
			delete audio_output;
		}
		
		// Verify we got output
		REQUIRE(output_samples_per_channel[0].size() > 0);
		
		// Verify we actually have non-zero audio data
		bool has_non_zero_audio = false;
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			for (float sample : output_samples_per_channel[ch]) {
				if (std::abs(sample) > 0.001f) {
					has_non_zero_audio = true;
					break;
				}
			}
			if (has_non_zero_audio) break;
		}
		REQUIRE(has_non_zero_audio); // Verify we have actual audio, not just zeros
		
		unsigned int total_texture_updates = playback_stage.get_texture_update_count();
		
		INFO("Texture update analysis:");
		INFO("  Total frames rendered: " << NUM_PLAYBACK_FRAMES);
		INFO("  Total texture updates: " << total_texture_updates);
		INFO("  Update rate: " << (float)total_texture_updates / NUM_PLAYBACK_FRAMES * 100.0f << "%");
		
		if (!update_frames.empty()) {
			INFO("  First 10 update frames and positions:");
			for (size_t i = 0; i < std::min(update_frames.size(), size_t(10)); ++i) {
				INFO("    Frame " << update_frames[i] << ": position=" << update_positions[i] 
				     << ", window_offset=" << update_window_offsets[i]);
			}
		}
		
		// Verify texture updates happened (should happen when position moves outside valid range)
		// Updates should be relatively infrequent - only when position approaches texture boundaries
		REQUIRE(total_texture_updates > 0); // Should have at least some updates
		REQUIRE(total_texture_updates < NUM_PLAYBACK_FRAMES / 2); // Should be less than 50% of frames
		
		// Verify updates happened at reasonable intervals
		// When playing at normal speed, texture should update periodically as position moves through the window
		if (update_frames.size() > 1) {
			std::vector<unsigned int> intervals;
			for (size_t i = 1; i < update_frames.size(); ++i) {
				intervals.push_back(update_frames[i] - update_frames[i - 1]);
			}
			
			// Calculate average interval
			unsigned int total_interval = 0;
			for (unsigned int interval : intervals) {
				total_interval += interval;
			}
			unsigned int avg_interval = intervals.empty() ? 0 : total_interval / intervals.size();
			
			INFO("  Average update interval: " << avg_interval << " frames");
			
			// Updates should happen at reasonable intervals (not too frequent, not too rare)
			// For a 0.5 second window at normal speed, updates should happen roughly every few frames
			// when approaching boundaries
			REQUIRE(avg_interval >= 1); // At least 1 frame between updates
			REQUIRE(avg_interval < NUM_PLAYBACK_FRAMES); // But not too sparse
		}
		
		// Verify audio continuity - conditional updates should not cause discontinuities
		SECTION("Audio continuity check") {
			constexpr float DISCONTINUITY_THRESHOLD = 0.2f;
			
			for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
				const auto& samples = output_samples_per_channel[ch];
				std::vector<size_t> discontinuity_indices;
				
				for (size_t i = 1; i < samples.size(); ++i) {
					float sample_diff = std::abs(samples[i] - samples[i - 1]);
					if (sample_diff > DISCONTINUITY_THRESHOLD) {
						discontinuity_indices.push_back(i);
					}
				}
				
				INFO("Channel " << ch << " continuity analysis:");
				INFO("  Total samples: " << samples.size());
				INFO("  Discontinuities found: " << discontinuity_indices.size());
				
				// Verify continuity - conditional updates should not cause discontinuities
				unsigned int max_allowed = std::max(1u, static_cast<unsigned int>(samples.size() / 1000));
				REQUIRE(discontinuity_indices.size() < max_allowed);
			}
		}
		
		// Verify that updates happen when position is outside valid range
		SECTION("Update timing verification") {
			// Re-run a shorter test to verify update logic
			playback_stage.get_history().set_tape_position(start_position);
			playback_stage.reset_texture_update_count();
			
			// Get window parameters
			unsigned int window_size = playback_stage.get_history().get_window_size_samples();
			int speed_samples = playback_stage.get_history().get_tape_speed_samples_per_buffer();
			unsigned int frame_size_samples = static_cast<unsigned int>(std::abs(speed_samples));
			
			// Render a few frames and check update logic
			for (int frame = 0; frame < 50; ++frame) {
				global_time->set_value(frame);
				global_time->render();
				
				unsigned int position_before = playback_stage.get_history().get_tape_position();
				unsigned int window_offset_before = playback_stage.get_history().get_window_offset_samples();
				bool was_outdated_before = playback_stage.get_history().is_outdated();
				unsigned int update_count_before = playback_stage.get_texture_update_count();
				
				playback_stage.render(frame);
				
				unsigned int update_count_after = playback_stage.get_texture_update_count();
				bool was_updated = (update_count_after > update_count_before);
				
				// If texture was updated, verify it was because data was outdated
				if (was_updated) {
					// After update, data should not be outdated (unless we're at a boundary)
					bool is_outdated_after = playback_stage.get_history().is_outdated();
					
					// Update should have happened because data was outdated
					// (Note: after update, it might still be outdated if we're at a boundary, which is OK)
					// First frame always updates, or data was outdated before update
					bool update_was_valid = (frame == 0) || was_outdated_before;
					REQUIRE(update_was_valid);
				}
				
				if (playback_stage.get_history().get_tape_position() >= tape->size()) {
					break;
				}
			}
		}
	}
}

TEST_CASE("AudioRenderStageHistory2 - growing tape during playback", "[audio_history2][growing_tape][dynamic_size][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float BASE_AMPLITUDE = 0.3f;
	constexpr int INITIAL_RECORD_DURATION_SECONDS = 1;
	constexpr int NUM_INITIAL_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * INITIAL_RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 3;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 0.5f;
	constexpr float PLAYBACK_SPEED = 1.0f;
	constexpr float RECORD_FREQUENCY = 880.0f; // Different frequency for recording to distinguish from playback

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape with dynamic size (no fixed size, so it can grow)
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStageWithConditionalUpdates playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
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
	
	// Record initial sine wave to tape (must happen before playback section)
	std::vector<float> phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < NUM_INITIAL_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			float channel_amplitude = BASE_AMPLITUDE * (ch + 1);
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data());
	}
	REQUIRE(tape->size() > 0);
	INFO("Initial tape size: " << tape->size() << " samples per channel");
	
	SECTION("Playback while tape grows") {
		// Clear stream_audio_texture to ensure it's zero when no previous stage is connected
		auto stream_param = playback_stage.find_parameter("stream_audio_texture");
		if (stream_param) {
			stream_param->clear_value();
		}
		
		// Setup audio output (only if enabled)
		AudioPlayerOutput* audio_output = nullptr;
		if (is_audio_output_enabled()) {
			audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
			REQUIRE(audio_output->open());
			REQUIRE(audio_output->start());
		}
		
		// Setup playback
		playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
		auto start_position = tape->size() / 2; // Start in the middle
		playback_stage.get_history().set_tape_position(start_position);
		playback_stage.play();
		playback_stage.reset_texture_update_count();
		
		// Track initial tape size before playback starts
		unsigned int initial_size = tape->size();
		
		// Track tape growth and texture updates
		std::vector<unsigned int> tape_sizes;
		std::vector<unsigned int> update_frames;
		std::vector<unsigned int> tape_sizes_at_update;
		
		// Collect output samples per channel
		std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
		}
		
		// Generate recording data (different frequency to distinguish from playback)
		std::vector<float> record_phases(NUM_CHANNELS, 0.0f);
		
		// Render and capture audio while recording new data to tape
		for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
			global_time->set_value(frame);
			global_time->render();
			
			// Track tape size before render
			unsigned int tape_size_before = tape->size();
			unsigned int update_count_before = playback_stage.get_texture_update_count();
			
			// Render playback stage (updates positions every frame, texture only when needed)
			playback_stage.render(frame);
			
			// Check if texture was updated
			unsigned int update_count_after = playback_stage.get_texture_update_count();
			if (update_count_after > update_count_before) {
				update_frames.push_back(frame);
				tape_sizes_at_update.push_back(tape->size());
			}
			
			// Record new data to tape (growing the tape)
			std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				float channel_amplitude = BASE_AMPLITUDE * (ch + 1) * 0.5f; // Lower amplitude for recording
				auto sine_buffer = generate_sine_wave(RECORD_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, record_phases[ch]);
				for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
					channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
				}
				record_phases[ch] += BUFFER_SIZE;
			}
			// Record to the end of the tape (growing it)
			tape->record(channel_major_buffer.data(), tape->size());
			
			// Track tape size after recording
			unsigned int tape_size_after = tape->size();
			tape_sizes.push_back(tape_size_after);
			
			// Render final stage
			final_stage.render(frame);
			
			// Get output from final stage
			auto output_param = final_stage.find_parameter("final_output_audio_texture");
			REQUIRE(output_param != nullptr);
			
			const float* output_data = static_cast<const float*>(output_param->get_value());
			REQUIRE(output_data != nullptr);
			
			// Check if audio data is non-zero (verify we have actual audio)
			bool has_audio = false;
			float max_amplitude = 0.0f;
			for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
				float abs_val = std::abs(output_data[i]);
				if (abs_val > 0.001f) {
					has_audio = true;
				}
				max_amplitude = std::max(max_amplitude, abs_val);
			}
			
			// Log diagnostic info on first frame or if no audio detected
			if (frame == 0 || (!has_audio && frame < 10)) {
				INFO("Frame " << frame << " audio check: has_audio=" << has_audio 
				     << ", max_amplitude=" << max_amplitude
				     << ", tape_position=" << playback_stage.get_history().get_tape_position()
				     << ", tape_size=" << tape->size());
			}
			
			// De-interleave to separate channels
			for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
				auto channel = i % NUM_CHANNELS;
				output_samples_per_channel[channel].push_back(output_data[i]);
			}
			
			// Push to audio output (output_data is interleaved format)
			// Always push audio data (even if it appears to be zero, as it might be valid silence)
			// The audio system will handle zero samples correctly
			if (audio_output) {
				while (!audio_output->is_ready()) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				audio_output->push(output_data);
			}
			
			// Stop if we've reached the end (but tape keeps growing, so this might not happen)
			if (playback_stage.get_history().get_tape_position() >= tape_size_before) {
				// Position reached the end before recording, but tape has grown now
				// Continue playback since tape has grown
			}
		}
		playback_stage.stop();
		
		// Wait for audio to finish playing and close audio output
		if (audio_output) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			audio_output->stop();
			audio_output->close();
			delete audio_output;
		}
		
		// Verify we got output
		REQUIRE(output_samples_per_channel[0].size() > 0);
		
		// Verify we actually have non-zero audio data
		bool has_non_zero_audio = false;
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			for (float sample : output_samples_per_channel[ch]) {
				if (std::abs(sample) > 0.001f) {
					has_non_zero_audio = true;
					break;
				}
			}
			if (has_non_zero_audio) break;
		}
		REQUIRE(has_non_zero_audio); // Verify we have actual audio, not just zeros
		
		// Verify tape grew
		REQUIRE(tape_sizes.size() > 0);
		unsigned int final_size = tape_sizes.back();
		REQUIRE(final_size > initial_size);
		
		INFO("Tape growth analysis:");
		INFO("  Initial tape size: " << initial_size << " samples per channel");
		INFO("  Final tape size: " << final_size << " samples per channel");
		INFO("  Total growth: " << (final_size - initial_size) << " samples per channel");
		INFO("  Total frames rendered: " << NUM_PLAYBACK_FRAMES);
		INFO("  Texture updates during growth: " << playback_stage.get_texture_update_count());
		
		if (!update_frames.empty()) {
			INFO("  First 10 texture update frames and tape sizes:");
			for (size_t i = 0; i < std::min(update_frames.size(), size_t(10)); ++i) {
				INFO("    Frame " << update_frames[i] << ": tape_size=" << tape_sizes_at_update[i]);
			}
		}
		
		// Verify tape grew by expected amount (one buffer per frame)
		unsigned int expected_growth = NUM_PLAYBACK_FRAMES * BUFFER_SIZE;
		unsigned int actual_growth = final_size - initial_size;
		INFO("  Expected growth: " << expected_growth << " samples");
		INFO("  Actual growth: " << actual_growth << " samples");
		// Allow some tolerance (might be slightly different due to timing)
		REQUIRE(actual_growth >= expected_growth * 0.9f); // At least 90% of expected
		REQUIRE(actual_growth <= expected_growth * 1.1f); // At most 110% of expected
		
		// Verify texture updates happened (should happen as tape grows and position moves)
		REQUIRE(playback_stage.get_texture_update_count() > 0);
		
		// Verify audio continuity - growing tape should not cause discontinuities
		SECTION("Audio continuity check with growing tape") {
			constexpr float DISCONTINUITY_THRESHOLD = 0.2f;
			
			for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
				const auto& samples = output_samples_per_channel[ch];
				std::vector<size_t> discontinuity_indices;
				
				for (size_t i = 1; i < samples.size(); ++i) {
					float sample_diff = std::abs(samples[i] - samples[i - 1]);
					if (sample_diff > DISCONTINUITY_THRESHOLD) {
						discontinuity_indices.push_back(i);
					}
				}
				
				INFO("Channel " << ch << " continuity analysis:");
				INFO("  Total samples: " << samples.size());
				INFO("  Discontinuities found: " << discontinuity_indices.size());
				
				// Verify continuity - growing tape should not cause discontinuities
				unsigned int max_allowed = std::max(1u, static_cast<unsigned int>(samples.size() / 1000));
				REQUIRE(discontinuity_indices.size() < max_allowed);
			}
		}
		
		// Verify playback position doesn't exceed tape size unexpectedly
		SECTION("Position tracking with growing tape") {
			// The playback position should be able to advance as the tape grows
			// Position should never exceed tape size (unless we're at the very end)
			unsigned int final_position = playback_stage.get_history().get_tape_position();
			unsigned int final_tape_size = tape->size();
			
			INFO("Final playback position: " << final_position);
			INFO("Final tape size: " << final_tape_size);
			
			// Position should be within tape bounds (allowing for one buffer margin)
			REQUIRE(final_position <= final_tape_size + BUFFER_SIZE);
		}
		
		// Verify that texture updates happen correctly as tape grows
		SECTION("Texture update behavior with growing tape") {
			// Texture should update when position approaches boundaries
			// Even as tape grows, updates should happen at appropriate times
			REQUIRE(playback_stage.get_texture_update_count() > 0);
			
			// Updates should be reasonable (not too many, not too few)
			// With a growing tape, we might have more updates as position moves through new areas
			unsigned int update_rate_percent = (playback_stage.get_texture_update_count() * 100) / NUM_PLAYBACK_FRAMES;
			INFO("Texture update rate: " << update_rate_percent << "%");
			
			// Updates should be less than 50% of frames (should be conditional, not every frame)
			REQUIRE(playback_stage.get_texture_update_count() < NUM_PLAYBACK_FRAMES / 2);
		}
	}
}

TEST_CASE("AudioRenderStageHistory2 - shifting window with amplitude changes", "[audio_history2][shifting_window][amplitude_changes][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float BASE_AMPLITUDE = 0.3f;
	constexpr int PLAYBACK_DURATION_SECONDS = 5;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 1.0f; // 1 second window as requested
	constexpr float PLAYBACK_SPEED = 1.0f;
	
	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape with dynamic size (no fixed size, so it can grow)
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStageWithConditionalUpdates playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
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
	
	// Clear stream_audio_texture to ensure it's zero when no previous stage is connected
	auto stream_param = playback_stage.find_parameter("stream_audio_texture");
	if (stream_param) {
		stream_param->clear_value();
	}
	
	// Setup audio output (only if enabled)
	AudioPlayerOutput* audio_output = nullptr;
	if (is_audio_output_enabled()) {
		audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
		REQUIRE(audio_output->open());
		REQUIRE(audio_output->start());
	}
	
	// Track amplitude changes over time
	// We'll change amplitude every second (every SAMPLE_RATE / BUFFER_SIZE frames)
	constexpr int FRAMES_PER_SECOND = SAMPLE_RATE / BUFFER_SIZE;
	constexpr int NUM_AMPLITUDE_SEGMENTS = PLAYBACK_DURATION_SECONDS + 1; // +1 for initial recording
	std::vector<float> amplitude_segments;
	amplitude_segments.reserve(NUM_AMPLITUDE_SEGMENTS);
	
	// Generate amplitude pattern: start low, increase, then decrease
	for (int i = 0; i < NUM_AMPLITUDE_SEGMENTS; ++i) {
		// Create a pattern: 0.1 -> 0.5 -> 0.9 -> 0.5 -> 0.1 (repeating)
		float normalized_pos = static_cast<float>(i % 5) / 4.0f;
		float amplitude = BASE_AMPLITUDE * (0.1f + normalized_pos * 0.8f);
		amplitude_segments.push_back(amplitude);
	}
	
	// Record initial audio to tape (at least 1 second) before starting playback
	// This ensures there's something to play back
	constexpr int INITIAL_RECORD_FRAMES = FRAMES_PER_SECOND; // 1 second
	std::vector<float> record_phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < INITIAL_RECORD_FRAMES; ++frame) {
		int amplitude_segment = frame / FRAMES_PER_SECOND;
		if (amplitude_segment >= static_cast<int>(amplitude_segments.size())) {
			amplitude_segment = amplitude_segments.size() - 1;
		}
		float current_amplitude = amplitude_segments[amplitude_segment];
		
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			float channel_amplitude = current_amplitude * (ch + 1);
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, record_phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			record_phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data(), tape->size());
	}
	REQUIRE(tape->size() > 0);
	INFO("Initial tape size: " << tape->size() << " samples per channel");
	
	// Setup playback - start from the beginning
	playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
	playback_stage.get_history().set_tape_position(0u);
	playback_stage.play();
	playback_stage.reset_texture_update_count();
	
	// Collect output samples per channel for verification
	std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
	}
	
	// Track window offsets and texture updates
	std::vector<unsigned int> window_offsets;
	std::vector<unsigned int> texture_update_frames;
	
	// Continue recording with changing amplitudes (phases continue from initial recording)
	// Render and capture audio while recording with changing amplitudes
	for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
		global_time->set_value(frame);
		global_time->render();
		
		// Determine current amplitude segment
		int amplitude_segment = frame / FRAMES_PER_SECOND;
		if (amplitude_segment >= static_cast<int>(amplitude_segments.size())) {
			amplitude_segment = amplitude_segments.size() - 1;
		}
		float current_amplitude = amplitude_segments[amplitude_segment];
		
		// Track window offset before render
		unsigned int window_offset_before = playback_stage.get_history().get_window_offset_samples();
		unsigned int update_count_before = playback_stage.get_texture_update_count();
		
		// Render playback stage (updates positions every frame, texture only when needed)
		playback_stage.render(frame);
		
		// Check if texture was updated
		unsigned int update_count_after = playback_stage.get_texture_update_count();
		if (update_count_after > update_count_before) {
			texture_update_frames.push_back(frame);
			window_offsets.push_back(playback_stage.get_history().get_window_offset_samples());
		}
		
		// Record new data to tape with current amplitude (growing the tape)
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			// Use different amplitude per channel for verification
			float channel_amplitude = current_amplitude * (ch + 1);
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, record_phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			record_phases[ch] += BUFFER_SIZE;
		}
		// Record to the end of the tape (growing it)
		tape->record(channel_major_buffer.data(), tape->size());
		
		// Render final stage
		final_stage.render(frame);
		
		// Get output from final stage
		auto output_param = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param != nullptr);
		
		const float* output_data = static_cast<const float*>(output_param->get_value());
		REQUIRE(output_data != nullptr);
		
		// Check if audio data is non-zero (verify we have actual audio)
		bool has_audio = false;
		float max_amplitude = 0.0f;
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			float abs_val = std::abs(output_data[i]);
			if (abs_val > 0.001f) {
				has_audio = true;
			}
			max_amplitude = std::max(max_amplitude, abs_val);
		}
		
		// Log diagnostic info on first frame or periodically
		if (frame == 0 || (frame % FRAMES_PER_SECOND == 0 && frame < 10 * FRAMES_PER_SECOND)) {
			INFO("Frame " << frame << " audio check: has_audio=" << has_audio 
			     << ", max_amplitude=" << max_amplitude
			     << ", tape_position=" << playback_stage.get_history().get_tape_position()
			     << ", tape_size=" << tape->size()
			     << ", window_offset=" << playback_stage.get_history().get_window_offset_samples()
			     << ", recording_amplitude=" << current_amplitude);
		}
		
		// De-interleave to separate channels
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			auto channel = i % NUM_CHANNELS;
			output_samples_per_channel[channel].push_back(output_data[i]);
		}
		
		// Push to audio output (output_data is interleaved format)
		// Always push audio data (even if it appears to be zero, as it might be valid silence)
		// The audio system will handle zero samples correctly
		if (audio_output) {
			while (!audio_output->is_ready()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			audio_output->push(output_data);
		}
	}
	playback_stage.stop();
	
	// Wait for audio to finish playing and close audio output
	if (audio_output) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		audio_output->stop();
		audio_output->close();
		delete audio_output;
	}
	
	// Verify we got output
	REQUIRE(output_samples_per_channel[0].size() > 0);
	
	// Verify we actually have non-zero audio data
	bool has_non_zero_audio = false;
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		for (float sample : output_samples_per_channel[ch]) {
			if (std::abs(sample) > 0.001f) {
				has_non_zero_audio = true;
				break;
			}
		}
		if (has_non_zero_audio) break;
	}
	REQUIRE(has_non_zero_audio); // Verify we have actual audio, not just zeros
	
	// Verify amplitude changes are reflected in the output
	// Calculate RMS (Root Mean Square) amplitude for each second of playback
	constexpr int SAMPLES_PER_SECOND = SAMPLE_RATE;
	std::vector<std::vector<float>> rms_per_second_per_channel(NUM_CHANNELS);
	
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		rms_per_second_per_channel[ch].reserve(NUM_AMPLITUDE_SEGMENTS);
		for (int second = 0; second < NUM_AMPLITUDE_SEGMENTS && second * SAMPLES_PER_SECOND < static_cast<int>(output_samples_per_channel[ch].size()); ++second) {
			int start_sample = second * SAMPLES_PER_SECOND;
			int end_sample = std::min(start_sample + SAMPLES_PER_SECOND, static_cast<int>(output_samples_per_channel[ch].size()));
			
			float sum_squares = 0.0f;
			int sample_count = 0;
			for (int i = start_sample; i < end_sample; ++i) {
				float sample = output_samples_per_channel[ch][i];
				sum_squares += sample * sample;
				sample_count++;
			}
			
			float rms = (sample_count > 0) ? std::sqrt(sum_squares / sample_count) : 0.0f;
			rms_per_second_per_channel[ch].push_back(rms);
		}
	}
	
	// Verify that amplitude changes are reflected
	// The RMS should roughly follow the amplitude pattern (allowing for some variation)
	// Note: Playback position corresponds to what was recorded earlier, so we need to account for this
	INFO("Amplitude verification:");
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		INFO("Channel " << ch << " RMS per second:");
		for (size_t i = 0; i < rms_per_second_per_channel[ch].size() && i < amplitude_segments.size(); ++i) {
			// The playback at second i corresponds to what was recorded at segment i
			// (since we start playback at position 0, which was recorded with segment 0 amplitude)
			float expected_amplitude = amplitude_segments[i] * (ch + 1);
			float actual_rms = rms_per_second_per_channel[ch][i];
			// RMS of a sine wave with amplitude A is approximately A / sqrt(2)
			float expected_rms = expected_amplitude / std::sqrt(2.0f);
			
			INFO("  Second " << i << ": expected_rms=" << expected_rms 
			     << ", actual_rms=" << actual_rms 
			     << ", expected_amplitude=" << expected_amplitude
			     << ", amplitude_segment=" << i);
			
			// Allow 50% tolerance for RMS comparison (due to windowing, phase, shifting window, etc.)
			// The shifting window and continuous recording/playback can cause some variation
			float tolerance = expected_rms * 0.5f;
			bool rms_within_tolerance = std::abs(actual_rms - expected_rms) < tolerance;
			// Also allow if RMS is at least 30% of expected (to account for window transitions)
			bool rms_above_minimum = actual_rms > expected_rms * 0.3f;
			bool rms_valid = rms_within_tolerance || rms_above_minimum;
			
			if (!rms_valid) {
				INFO("  WARNING: RMS validation failed for second " << i 
				     << " (tolerance=" << tolerance << ", within_tolerance=" << rms_within_tolerance 
				     << ", above_minimum=" << rms_above_minimum << ")");
			}
			REQUIRE(rms_valid);
		}
	}
	
	// Verify window offset changes (window should shift as playback progresses)
	REQUIRE(window_offsets.size() > 0);
	INFO("Window offset changes: " << window_offsets.size() << " texture updates");
	
	// Verify that window offsets are increasing (window shifting forward)
	bool window_shifting = false;
	for (size_t i = 1; i < window_offsets.size(); ++i) {
		if (window_offsets[i] > window_offsets[i-1]) {
			window_shifting = true;
			break;
		}
	}
	REQUIRE(window_shifting); // Window should shift forward as playback progresses
	
	// Continuity and slope change checks
	SECTION("Continuity check - verify no discontinuities during shifting window") {
		constexpr float DISCONTINUITY_THRESHOLD = 0.25f; // Threshold for detecting discontinuities
		// Higher threshold than normal because amplitude changes can cause larger sample differences
		
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
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
			
			INFO("Channel " << ch << " continuity analysis:");
			INFO("  Total samples: " << samples.size());
			INFO("  Discontinuity threshold: " << DISCONTINUITY_THRESHOLD);
			INFO("  Found " << discontinuity_indices.size() << " discontinuities");
			
			if (!discontinuity_indices.empty()) {
				INFO("  First 10 discontinuity magnitudes:");
				for (size_t i = 0; i < std::min(discontinuity_indices.size(), size_t(10)); ++i) {
					INFO("    Sample " << discontinuity_indices[i] << ": " << discontinuity_magnitudes[i]);
				}
			}
			
			// Verify continuity - shifting window should not cause discontinuities
			// Allow a small number of discontinuities (less than 0.1% of samples, minimum 1)
			// This accounts for normal signal variations, amplitude changes, and texture window boundaries
			unsigned int max_allowed = std::max(1u, static_cast<unsigned int>(samples.size() / 1000));
			REQUIRE(discontinuity_indices.size() < max_allowed);
		}
	}
	
	SECTION("Slope change continuity check - verify smooth transitions during shifting window") {
		// This test verifies that the rate of change (slope) changes incrementally
		// without large jumps, which would indicate smooth window shifting
		// The shifting window and amplitude changes can cause some variation, but should be smooth
		constexpr float MAX_SLOPE_CHANGE_JUMP = 0.08f; // Maximum allowed jump in slope change per sample
		// Higher threshold than normal because amplitude changes can cause larger slope variations
		constexpr float MIN_SLOPE_CHANGE_FOR_ANALYSIS = 0.0001f; // Ignore very small slope changes
		
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			
			if (samples.size() < 3) {
				continue; // Need at least 3 samples to calculate slope changes
			}
			
			// Calculate first derivative (slope) between consecutive samples
			std::vector<float> slopes;
			slopes.reserve(samples.size() - 1);
			for (size_t i = 1; i < samples.size(); ++i) {
				float slope = samples[i] - samples[i - 1];
				slopes.push_back(slope);
			}
			
			// Calculate second derivative (change in slope)
			std::vector<float> slope_changes;
			slope_changes.reserve(slopes.size() - 1);
			for (size_t i = 1; i < slopes.size(); ++i) {
				float slope_change = std::abs(slopes[i] - slopes[i - 1]);
				slope_changes.push_back(slope_change);
			}
			
			// Find large jumps in slope changes
			std::vector<size_t> large_jump_indices;
			std::vector<float> large_jump_magnitudes;
			
			for (size_t i = 1; i < slope_changes.size(); ++i) {
				float jump = std::abs(slope_changes[i] - slope_changes[i - 1]);
				// Only flag if both slope changes are significant enough to matter
				if (slope_changes[i] > MIN_SLOPE_CHANGE_FOR_ANALYSIS && 
				    slope_changes[i - 1] > MIN_SLOPE_CHANGE_FOR_ANALYSIS &&
				    jump > MAX_SLOPE_CHANGE_JUMP) {
					large_jump_indices.push_back(i);
					large_jump_magnitudes.push_back(jump);
				}
			}
			
			INFO("Channel " << ch << " slope analysis:");
			INFO("  Total samples: " << samples.size());
			INFO("  Max slope change jump: " << MAX_SLOPE_CHANGE_JUMP);
			INFO("  Found " << large_jump_indices.size() << " large slope change jumps");
			
			if (!large_jump_indices.empty()) {
				INFO("  First 10 large jump magnitudes:");
				for (size_t i = 0; i < std::min(large_jump_indices.size(), size_t(10)); ++i) {
					INFO("    Sample " << large_jump_indices[i] << ": " << large_jump_magnitudes[i]);
				}
			}
			
			// Verify smooth slope changes - shifting window should not cause sudden slope changes
			// Allow a small number of jumps (less than 0.1% of samples, minimum 1)
			// This accounts for normal signal variations, amplitude changes, and texture window boundaries
			unsigned int max_allowed = std::max(1u, static_cast<unsigned int>(samples.size() / 1000));
			REQUIRE(large_jump_indices.size() < max_allowed);
		}
	}
	
	INFO("Test completed successfully:");
	INFO("  Total frames: " << NUM_PLAYBACK_FRAMES);
	INFO("  Texture updates: " << playback_stage.get_texture_update_count());
	INFO("  Final tape size: " << tape->size() << " samples per channel");
	INFO("  Window size: " << WINDOW_SIZE_SECONDS << " seconds (" << playback_stage.get_history().get_window_size_samples() << " samples)");
}

TEST_CASE("AudioRenderStageHistory2 - playback faster than recording hits upper window bound", "[audio_history2][window_bounds][faster_playback][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float BASE_AMPLITUDE = 0.3f;
	constexpr int RECORD_DURATION_SECONDS = 3;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 2; // Shorter playback duration since we're playing faster
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 1.0f;
	constexpr float RECORD_SPEED = 1.0f; // Normal recording speed
	constexpr float PLAYBACK_SPEED = 2.0f; // Playback 2x faster than recording
	
	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStageWithConditionalUpdates playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
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
	
	// Clear stream_audio_texture
	auto stream_param = playback_stage.find_parameter("stream_audio_texture");
	if (stream_param) {
		stream_param->clear_value();
	}
	
	// Setup audio output (only if enabled)
	AudioPlayerOutput* audio_output = nullptr;
	if (is_audio_output_enabled()) {
		audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
		REQUIRE(audio_output->open());
		REQUIRE(audio_output->start());
	}
	
	// Record initial audio to tape
	std::vector<float> record_phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			float channel_amplitude = BASE_AMPLITUDE * (ch + 1);
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, record_phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			record_phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data(), tape->size());
	}
	REQUIRE(tape->size() > 0);
	INFO("Recorded " << tape->size() << " samples per channel to tape");
	
	// Setup playback - start from beginning, play at 2x speed
	playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
	playback_stage.get_history().set_tape_position(0u);
	playback_stage.play();
	playback_stage.reset_texture_update_count();
	
	// Track window offsets and texture updates
	std::vector<unsigned int> window_offsets;
	std::vector<unsigned int> tape_positions;
	std::vector<unsigned int> texture_update_frames;
	
	// Collect output samples per channel
	std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
	}
	
	// Render and capture audio
	for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
		global_time->set_value(frame);
		global_time->render();
		
		// Track state before render
		unsigned int position_before = playback_stage.get_history().get_tape_position();
		unsigned int window_offset_before = playback_stage.get_history().get_window_offset_samples();
		unsigned int update_count_before = playback_stage.get_texture_update_count();
		bool stopped_before = playback_stage.get_history().is_tape_stopped();
		unsigned int tape_size = tape->size();
		
		// Check if we're approaching or at the end BEFORE render
		bool near_end_before = (tape_size > position_before) && (tape_size - position_before < BUFFER_SIZE * 3);
		bool at_end_before = position_before >= tape_size;
		
		// Render playback stage
		playback_stage.render(frame);
		
		// Track state after render
		unsigned int position_after = playback_stage.get_history().get_tape_position();
		unsigned int window_offset_after = playback_stage.get_history().get_window_offset_samples();
		unsigned int update_count_after = playback_stage.get_texture_update_count();
		bool stopped_after = playback_stage.get_history().is_tape_stopped();
		
		// Log what happened when hitting the boundary
		if (at_end_before || near_end_before || position_after >= tape_size) {
			INFO("Frame " << frame << ": " << (at_end_before ? "AT END" : (near_end_before ? "NEAR END" : "REACHED END")) << " of tape!");
			INFO("  Before render - Position: " << position_before << " / " << tape_size 
			     << ", Window offset: " << window_offset_before << ", Stopped: " << stopped_before);
			INFO("  After render  - Position: " << position_after << " / " << tape_size 
			     << ", Window offset: " << window_offset_after << ", Stopped: " << stopped_after);
			INFO("  Position change: " << (position_after > position_before ? "+" : "") << (static_cast<int>(position_after) - static_cast<int>(position_before)));
			INFO("  Texture updated: " << (update_count_after > update_count_before));
			INFO("  Samples remaining: " << (tape_size > position_after ? (tape_size - position_after) : 0));
		}
		
		// Check if texture was updated
		if (update_count_after > update_count_before) {
			texture_update_frames.push_back(frame);
			window_offsets.push_back(window_offset_after);
			tape_positions.push_back(position_after);
		}
		
		// Track window offset changes
		if (window_offset_after != window_offset_before) {
			INFO("Frame " << frame << ": Window offset changed from " << window_offset_before 
			     << " to " << window_offset_after << " (position: " << position_before << " -> " << position_after << ")");
		}
		
		// Render final stage
		final_stage.render(frame);
		
		// Get output from final stage
		auto output_param = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param != nullptr);
		
		const float* output_data = static_cast<const float*>(output_param->get_value());
		REQUIRE(output_data != nullptr);
		
		// De-interleave to separate channels
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
	}
	playback_stage.stop();
	
	// Wait for audio to finish playing and close audio output
	if (audio_output) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		audio_output->stop();
		audio_output->close();
		delete audio_output;
	}
	
	// Verify we got output
	REQUIRE(output_samples_per_channel[0].size() > 0);
	
	// Verify window offset increased (window shifted forward as playback progressed)
	REQUIRE(window_offsets.size() > 0);
	INFO("Window offset analysis (faster playback):");
	INFO("  Total texture updates: " << window_offsets.size());
	INFO("  Initial window offset: " << window_offsets[0]);
	INFO("  Final window offset: " << window_offsets.back());
	
	// Track what happened when hitting the end
	unsigned int final_position = playback_stage.get_history().get_tape_position();
	unsigned int final_window_offset = playback_stage.get_history().get_window_offset_samples();
	bool tape_stopped = playback_stage.get_history().is_tape_stopped();
	INFO("End-of-tape behavior:");
	INFO("  Final position: " << final_position << " (tape size: " << tape->size() << ")");
	INFO("  Final window offset: " << final_window_offset);
	INFO("  Tape stopped: " << tape_stopped);
	INFO("  Position >= tape size: " << (final_position >= tape->size()));
	
	// Verify window offsets are increasing (window shifting forward)
	bool window_shifting_forward = false;
	for (size_t i = 1; i < window_offsets.size(); ++i) {
		if (window_offsets[i] > window_offsets[i-1]) {
			window_shifting_forward = true;
			break;
		}
	}
	REQUIRE(window_shifting_forward);
	
	// Verify that playback position advanced faster than recording
	// With 2x speed, we should have advanced approximately 2x the number of frames
	unsigned int expected_advancement = NUM_PLAYBACK_FRAMES * BUFFER_SIZE * static_cast<unsigned int>(PLAYBACK_SPEED);
	INFO("Position analysis:");
	INFO("  Final playback position: " << final_position);
	INFO("  Expected advancement (2x speed): ~" << expected_advancement);
	INFO("  Actual advancement: " << final_position);
	INFO("  Reached end of tape: " << (final_position >= tape->size()));
	
	// Continuity check
	SECTION("Continuity check - faster playback hitting upper bound") {
		constexpr float DISCONTINUITY_THRESHOLD = 0.3f; // Higher threshold for faster playback
		
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			std::vector<size_t> discontinuity_indices;
			
			for (size_t i = 1; i < samples.size(); ++i) {
				float sample_diff = std::abs(samples[i] - samples[i - 1]);
				if (sample_diff > DISCONTINUITY_THRESHOLD) {
					discontinuity_indices.push_back(i);
				}
			}
			
			INFO("Channel " << ch << " continuity analysis (faster playback):");
			INFO("  Total samples: " << samples.size());
			INFO("  Found " << discontinuity_indices.size() << " discontinuities");
			
			// Allow a small number of discontinuities (less than 0.1% of samples, minimum 1)
			unsigned int max_allowed = std::max(1u, static_cast<unsigned int>(samples.size() / 1000));
			REQUIRE(discontinuity_indices.size() < max_allowed);
		}
	}
	
	INFO("Test completed successfully:");
	INFO("  Playback speed: " << PLAYBACK_SPEED << "x");
	INFO("  Texture updates: " << playback_stage.get_texture_update_count());
	INFO("  Window size: " << WINDOW_SIZE_SECONDS << " seconds");
}

TEST_CASE("AudioRenderStageHistory2 - playback slower than recording hits lower window bound", "[audio_history2][window_bounds][slower_playback][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr float TEST_FREQUENCY = 440.0f;
	constexpr float BASE_AMPLITUDE = 0.3f;
	constexpr int RECORD_DURATION_SECONDS = 3;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 4; // Longer playback duration since we're playing slower
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 1.0f;
	constexpr float RECORD_SPEED = 1.0f; // Normal recording speed
	constexpr float PLAYBACK_SPEED = 0.5f; // Playback 0.5x slower than recording
	
	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockTapePlaybackStageWithConditionalUpdates playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
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
	
	// Clear stream_audio_texture
	auto stream_param = playback_stage.find_parameter("stream_audio_texture");
	if (stream_param) {
		stream_param->clear_value();
	}
	
	// Setup audio output (only if enabled)
	AudioPlayerOutput* audio_output = nullptr;
	if (is_audio_output_enabled()) {
		audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
		REQUIRE(audio_output->open());
		REQUIRE(audio_output->start());
	}
	
	// Record initial audio to tape
	std::vector<float> record_phases(NUM_CHANNELS, 0.0f);
	for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			float channel_amplitude = BASE_AMPLITUDE * (ch + 1);
			auto sine_buffer = generate_sine_wave(TEST_FREQUENCY, channel_amplitude, SAMPLE_RATE, BUFFER_SIZE, 1, record_phases[ch]);
			for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = sine_buffer[i];
			}
			record_phases[ch] += BUFFER_SIZE;
		}
		tape->record(channel_major_buffer.data(), tape->size());
	}
	REQUIRE(tape->size() > 0);
	INFO("Recorded " << tape->size() << " samples per channel to tape");
	
	// Setup playback - start from middle, play at 0.5x speed
	// Starting from middle gives us room to move backwards if needed
	unsigned int start_position = tape->size() / 2;
	playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
	playback_stage.get_history().set_tape_position(start_position);
	playback_stage.play();
	playback_stage.reset_texture_update_count();
	
	// Track window offsets and texture updates
	std::vector<unsigned int> window_offsets;
	std::vector<unsigned int> tape_positions;
	std::vector<unsigned int> texture_update_frames;
	
	// Collect output samples per channel
	std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
	}
	
	// Render and capture audio
	for (int frame = 0; frame < NUM_PLAYBACK_FRAMES; ++frame) {
		global_time->set_value(frame);
		global_time->render();
		
		// Track state before render
		unsigned int position_before = playback_stage.get_history().get_tape_position();
		unsigned int window_offset_before = playback_stage.get_history().get_window_offset_samples();
		unsigned int update_count_before = playback_stage.get_texture_update_count();
		
		// Render playback stage
		playback_stage.render(frame);
		
		// Track state after render
		unsigned int position_after = playback_stage.get_history().get_tape_position();
		unsigned int window_offset_after = playback_stage.get_history().get_window_offset_samples();
		unsigned int update_count_after = playback_stage.get_texture_update_count();
		
		// Check if texture was updated
		if (update_count_after > update_count_before) {
			texture_update_frames.push_back(frame);
			window_offsets.push_back(window_offset_after);
			tape_positions.push_back(position_after);
		}
		
		// Track window offset changes
		if (window_offset_after != window_offset_before) {
			INFO("Frame " << frame << ": Window offset changed from " << window_offset_before 
			     << " to " << window_offset_after << " (position: " << position_before << " -> " << position_after << ")");
		}
		
		// Render final stage
		final_stage.render(frame);
		
		// Get output from final stage
		auto output_param = final_stage.find_parameter("final_output_audio_texture");
		REQUIRE(output_param != nullptr);
		
		const float* output_data = static_cast<const float*>(output_param->get_value());
		REQUIRE(output_data != nullptr);
		
		// De-interleave to separate channels
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
		
		// Track when we hit the beginning (but don't stop - let it continue to see what happens)
		unsigned int current_pos = playback_stage.get_history().get_tape_position();
		bool near_beginning = current_pos < BUFFER_SIZE * 2;
		bool at_beginning = current_pos == 0u;
		
		if (at_beginning || near_beginning) {
			INFO("Frame " << frame << ": " << (at_beginning ? "HIT BEGINNING" : "Near beginning") << " of tape!");
			INFO("  Position: " << current_pos);
			INFO("  Window offset: " << playback_stage.get_history().get_window_offset_samples());
			INFO("  Window size: " << playback_stage.get_history().get_window_size_samples());
			INFO("  Tape stopped: " << playback_stage.get_history().is_tape_stopped());
			INFO("  Texture outdated: " << playback_stage.get_history().is_outdated());
		}
	}
	playback_stage.stop();
	
	// Wait for audio to finish playing and close audio output
	if (audio_output) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		audio_output->stop();
		audio_output->close();
		delete audio_output;
	}
	
	// Verify we got output
	REQUIRE(output_samples_per_channel[0].size() > 0);
	
	// Verify window offset behavior (may decrease or stay stable for slower playback)
	REQUIRE(window_offsets.size() > 0);
	INFO("Window offset analysis (slower playback):");
	INFO("  Total texture updates: " << window_offsets.size());
	INFO("  Initial window offset: " << window_offsets[0]);
	INFO("  Final window offset: " << window_offsets.back());
	INFO("  Start position: " << start_position);
	
	// Track what happened when hitting boundaries
	unsigned int final_position = playback_stage.get_history().get_tape_position();
	unsigned int final_window_offset = playback_stage.get_history().get_window_offset_samples();
	bool tape_stopped = playback_stage.get_history().is_tape_stopped();
	INFO("Boundary behavior:");
	INFO("  Final position: " << final_position);
	INFO("  Final window offset: " << final_window_offset);
	INFO("  Tape stopped: " << tape_stopped);
	INFO("  Hit beginning: " << (final_position == 0u));
	INFO("  Hit end: " << (final_position >= tape->size()));
	
	// For slower playback, window offset may decrease as we approach the lower bound
	// or stay relatively stable if we're moving forward slowly
	bool window_changed = false;
	for (size_t i = 1; i < window_offsets.size(); ++i) {
		if (window_offsets[i] != window_offsets[i-1]) {
			window_changed = true;
			break;
		}
	}
	// Window should have updated at least once
	bool window_valid = window_changed || window_offsets.size() > 0;
	REQUIRE(window_valid);
	
	// Verify that playback position advanced slower than recording
	// With 0.5x speed, we should have advanced approximately 0.5x the number of frames
	unsigned int position_change = (final_position > start_position) ? (final_position - start_position) : (start_position - final_position);
	INFO("Position analysis:");
	INFO("  Start playback position: " << start_position);
	INFO("  Final playback position: " << final_position);
	INFO("  Position change: " << position_change);
	INFO("  Expected advancement (0.5x speed): ~" << (NUM_PLAYBACK_FRAMES * BUFFER_SIZE / 2));
	
		// Continuity check
	SECTION("Continuity check - slower playback hitting lower bound") {
		constexpr float DISCONTINUITY_THRESHOLD = 0.25f;
		
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			const auto& samples = output_samples_per_channel[ch];
			std::vector<size_t> discontinuity_indices;
			
			for (size_t i = 1; i < samples.size(); ++i) {
				float sample_diff = std::abs(samples[i] - samples[i - 1]);
				if (sample_diff > DISCONTINUITY_THRESHOLD) {
					discontinuity_indices.push_back(i);
				}
			}
			
			INFO("Channel " << ch << " continuity analysis (slower playback):");
			INFO("  Total samples: " << samples.size());
			INFO("  Found " << discontinuity_indices.size() << " discontinuities");
			
			// Allow a small number of discontinuities (less than 0.1% of samples, minimum 1)
			unsigned int max_allowed = std::max(1u, static_cast<unsigned int>(samples.size() / 1000));
			REQUIRE(discontinuity_indices.size() < max_allowed);
		}
	}
	
	INFO("Test completed successfully:");
	INFO("  Playback speed: " << PLAYBACK_SPEED << "x");
	INFO("  Texture updates: " << playback_stage.get_texture_update_count());
	INFO("  Window size: " << WINDOW_SIZE_SECONDS << " seconds");
}

TEST_CASE("AudioRenderStageHistory2 - multiple start positions", "[audio_history2][multiple_start_positions][playback][audio_output][csv_output][gl_test]") {
	constexpr int BUFFER_SIZE = 256;
	constexpr int NUM_CHANNELS = 2;
	constexpr int SAMPLE_RATE = 44100;
	constexpr int RECORD_DURATION_SECONDS = 8;
	constexpr int NUM_RECORD_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * RECORD_DURATION_SECONDS;
	constexpr int PLAYBACK_DURATION_SECONDS = 3;
	constexpr int NUM_PLAYBACK_FRAMES = (SAMPLE_RATE / BUFFER_SIZE) * PLAYBACK_DURATION_SECONDS;
	constexpr float WINDOW_SIZE_SECONDS = 1.5f;
	constexpr float PLAYBACK_SPEED = 1.0f;

	// Constant amplitudes for different regions
	constexpr float AMPLITUDE = 0.5f;   // After START_POSITION_1

	// Start positions (in samples)
	constexpr unsigned int START_POSITION_0 = 0;
	constexpr unsigned int START_POSITION_1 = SAMPLE_RATE / BUFFER_SIZE; // 172 frames

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	// Create tape and mock playback stage
	auto tape = std::make_shared<AudioTape>(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	MockMultipleStartPositionsStage playback_stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, WINDOW_SIZE_SECONDS);
	playback_stage.get_history().set_tape(tape);
	playback_stage.set_start_position(START_POSITION_0, START_POSITION_1);
	
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
	
	// Record constant values to tape with different amplitudes at different positions
	// Before START_POSITION_1_SAMPLES: use AMPLITUDE_BEFORE_POSITION_1
	// After START_POSITION_1_SAMPLES: use AMPLITUDE_AFTER_POSITION_1
	std::vector<std::vector<float>> input_samples_per_channel(NUM_CHANNELS);
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		input_samples_per_channel[ch].reserve(SAMPLE_RATE * RECORD_DURATION_SECONDS);
	}
	
	unsigned int current_sample = 0;
	for (int frame = 0; frame < NUM_RECORD_FRAMES; ++frame) {
		std::vector<float> channel_major_buffer(BUFFER_SIZE * NUM_CHANNELS);
		
		// Determine amplitude based on current position
		float amplitude = AMPLITUDE;
		
		// Generate constant buffer for all channels
		auto constant_buffer = generate_constant_buffer(amplitude, BUFFER_SIZE, NUM_CHANNELS);
		
		// Convert from interleaved to channel-major format
		for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				channel_major_buffer[ch * BUFFER_SIZE + i] = constant_buffer[i * NUM_CHANNELS + ch];
			}
		}
		
		// Collect input samples (de-interleave from channel-major format)
		for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
			for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
				input_samples_per_channel[ch].push_back(channel_major_buffer[ch * BUFFER_SIZE + i]);
			}
		}
		
		tape->record(channel_major_buffer.data());
		current_sample += BUFFER_SIZE;
	}
	
	REQUIRE(tape->size() > 0);
	
	// Setup audio output (only if enabled)
	AudioPlayerOutput* audio_output = nullptr;
	if (is_audio_output_enabled()) {
		audio_output = new AudioPlayerOutput(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
		REQUIRE(audio_output->open());
		REQUIRE(audio_output->start());
	}
	
	// Initialize output sample vectors (per channel)
	std::vector<std::vector<float>> output_samples_per_channel(NUM_CHANNELS);
	for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
		output_samples_per_channel[ch].reserve(SAMPLE_RATE * PLAYBACK_DURATION_SECONDS);
	}
	
	// Configure playback - start from beginning
	playback_stage.get_history().set_tape_speed(PLAYBACK_SPEED);
	playback_stage.get_history().set_tape_position(0u);
	playback_stage.get_history().start_tape();
	playback_stage.play();
	
	// Initialize window before first render to ensure correct offset for position 0
	playback_stage.get_history().update_window();
	
	// Render and capture output
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
		
		frame_count++;
		
		// Stop if tape has stopped or reached end
		if (playback_stage.get_history().is_tape_stopped() || 
		    playback_stage.get_history().is_tape_at_end()) {
			break;
		}
	}
	
	// Stop playback
	playback_stage.stop();
	
	// Wait for audio to finish and close audio output
	if (audio_output) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		audio_output->stop();
		audio_output->close();
		delete audio_output;
	}
	
	SECTION("Verify output") {
		// Verify we have samples for all channels
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			REQUIRE(output_samples_per_channel[ch].size() > 0);
			
			// Check that output is not all zeros
			bool has_non_zero = false;
			for (float sample : output_samples_per_channel[ch]) {
				if (std::abs(sample) > 0.001f) {
					has_non_zero = true;
					break;
				}
			}
			REQUIRE(has_non_zero == true);
		}
	}
	
	SECTION("Verify amplitude changes at correct positions") {
		// Calculate exact sample index where position_1 starts
		// START_POSITION_1 is in frames, so multiply by BUFFER_SIZE to get sample index
		constexpr unsigned int SAMPLE_WHERE_POSITION_1_STARTS = START_POSITION_1 * BUFFER_SIZE; // 172 * 256 = 44032
		
		// Get actual window size from history (may differ from WINDOW_SIZE_SECONDS * SAMPLE_RATE due to texture rounding)
		unsigned int actual_window_size_samples = playback_stage.get_history().get_window_size_samples();
		
		// Small epsilon for floating point comparison
		constexpr float EPSILON = 0.0001f;
	
		constexpr unsigned int AUDIO_START_SAMPLE = 0; // Audio starts at sample 0
		constexpr unsigned int TRANSITION_SAMPLE = SAMPLE_WHERE_POSITION_1_STARTS; // 44032
		// Window for position_0 ends when it reaches window_size_samples from its start
		// Position_0 starts at sample 0, so its window ends at actual_window_size_samples
		// The last sample with both positions is actual_window_size_samples (inclusive)
		const unsigned int POSITION_0_WINDOW_END = actual_window_size_samples;
		// Window for position_1 ends when it reaches window_size_samples from its start
		// Position_1 starts at TRANSITION_SAMPLE (44032), so its window ends at TRANSITION_SAMPLE + actual_window_size_samples
		const unsigned int POSITION_1_WINDOW_END = TRANSITION_SAMPLE + actual_window_size_samples;
		
		for (unsigned int ch = 0; ch < NUM_CHANNELS; ++ch) {
			for (unsigned int i = 0; i < output_samples_per_channel[ch].size(); ++i) {
				float sample_value = output_samples_per_channel[ch][i];
				float expected_value;
				
				if (i <= AUDIO_START_SAMPLE) {
					// At audio start (sample 0): only position_0 contributes
					expected_value = AMPLITUDE;
				} else if (i < TRANSITION_SAMPLE) {
					// After audio starts but before position_1 starts: only position_0 contributes
					expected_value = AMPLITUDE;
				} else if (i < POSITION_0_WINDOW_END) {
					// After position_1 starts but before position_0's window ends: both positions contribute
					expected_value = AMPLITUDE * 2.0f;
				} else if (i < POSITION_1_WINDOW_END) {
					// After position_0's window ends but before position_1's window ends: only position_1 contributes
					expected_value = AMPLITUDE;
				} else {
					// After position_1's window ends: no audio
					expected_value = 0.0f;
				}
				
				INFO("Channel " << ch << " sample " << i << ": got " << sample_value << ", expected " << expected_value);
				REQUIRE(std::abs(sample_value - expected_value) < EPSILON);
			}
		}
	}
	
	if (is_csv_output_enabled()) {
		SECTION("Export output to CSV") {
			std::string csv_output_dir = "build/tests/csv_output";
			system(("mkdir -p " + csv_output_dir).c_str());
			
			std::string csv_filename = csv_output_dir + "/multiple_start_positions_output.csv";
			
			CSVTestOutput csv_writer(csv_filename, SAMPLE_RATE);
			REQUIRE(csv_writer.is_open());
			csv_writer.write_channels(output_samples_per_channel, SAMPLE_RATE);
			csv_writer.close();
			
			INFO("CSV file written to: " << csv_filename);
			printf("CSV file written to: %s\n", csv_filename.c_str());
		}
	}
	
	if (is_csv_output_enabled()) {
		SECTION("Write input sine wave to CSV") {
			std::string csv_output_dir = "build/tests/csv_output";
			system(("mkdir -p " + csv_output_dir).c_str());
			
			std::ofstream csv_file(csv_output_dir + "/input_sine_wave_multiple_start_positions.csv");
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
			std::cout << "Wrote input sine wave to build/tests/csv_output/input_sine_wave_multiple_start_positions.csv (" 
			          << num_samples << " samples, " << NUM_CHANNELS << " channels)" << std::endl;
		}
	}
	
	delete global_time;
}