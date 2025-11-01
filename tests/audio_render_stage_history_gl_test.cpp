// Mock-stage focused test for AudioRenderStageHistory without chaining to final output

#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#define private public
#include "audio_render_stage/audio_render_stage_history.h"
#undef private

#include "audio_core/audio_render_stage.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"

#include <vector>
#include <fstream>
#include <algorithm>
#include <cmath>

struct TestParams { int buffer_size; int num_channels; const char* name; };
using TestParam1 = std::integral_constant<int, 0>; // 256 x 2
using TestParam2 = std::integral_constant<int, 1>; // 512 x 2

constexpr TestParams get_test_params(int i) {
	constexpr TestParams P[] = { {256,2,"256x2"}, {512,2,"512x2"} };
	return P[i];
}

// Minimal fragment shader sampling from history via sample_index uniform
static const char* kHistorySampleFrag = "build/tests/mock_history_stage_frag.glsl";

static void write_history_shader_once() {
	static bool done = false;
	if (done) return;
	std::ofstream fs(kHistorySampleFrag);
	fs << R"(
uniform int sample_index;
void main(){
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    float v = get_audio_history_sample(sample_index, int(TexCoord.y * float(num_channels)));

    output_audio_texture = vec4(v);
    debug_audio_texture  = stream_audio;
}
)";
	done = true;
}

class MockHistoryStage : public AudioRenderStage {
public:
	MockHistoryStage(unsigned int frames_per_buffer,
				  unsigned int sample_rate,
				  unsigned int num_channels)
	: AudioRenderStage(frames_per_buffer, sample_rate, num_channels,
				   kHistorySampleFrag,
				   // include history imports explicitly
				   std::vector<std::string>{
					"build/shaders/global_settings.glsl",
					"build/shaders/frag_shader_settings.glsl",
					"build/shaders/history_settings.glsl"})
	{
		// Add sample_index uniform
		auto sample_index = new AudioIntParameter("sample_index", AudioParameter::ConnectionType::INPUT);
		sample_index->set_value(0);
		this->add_parameter(sample_index);

		// Create history and its texture parameter
		m_history = std::make_unique<AudioRenderStageHistory>(MAX_TEXTURE_SIZE, frames_per_buffer, sample_rate, num_channels);
		this->add_parameter(m_history->create_audio_history_texture(++m_active_texture_count));

		m_next_frame_data.resize(frames_per_buffer * num_channels, 0.0f);
	}

	void set_next_frame_data(const std::vector<float>& channel_major_data) {
		m_next_frame_data = channel_major_data;
	}

protected:
	void render(const unsigned int time) override {
		if (time != m_time) {
			m_history->shift_history_buffer();
		}
		m_history->save_stream_to_history(m_next_frame_data.data());
		m_history->update_audio_history_texture();
		AudioRenderStage::render(time);
	}

private:
	std::unique_ptr<AudioRenderStageHistory> m_history;
	std::vector<float> m_next_frame_data; // channel-major layout
};

TEMPLATE_TEST_CASE("AudioRenderStageHistory - mock stage sampling only", "[audio_history][gl_test][template]",
			   TestParam1, TestParam2) {
	write_history_shader_once();
	constexpr auto P = get_test_params(TestType::value);
	constexpr int BUFFER_SIZE = P.buffer_size;
	constexpr int NUM_CHANNELS = P.num_channels;
	constexpr int SAMPLE_RATE = 44100;

	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;

	// Global time buffer for compatibility with default imports
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());

	MockHistoryStage stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	REQUIRE(stage.initialize());

	context.prepare_draw();

	REQUIRE(stage.bind());

	// Create channel-major frame data with distinct constants per channel
	const float ch0 = 0.21f;
	const float ch1 = 0.37f;
	std::vector<float> frame(NUM_CHANNELS * BUFFER_SIZE);
	std::fill(frame.begin() + 0, frame.begin() + BUFFER_SIZE, ch0);
	std::fill(frame.begin() + BUFFER_SIZE, frame.begin() + 2 * BUFFER_SIZE, ch1);

	// Inject one frame to history tail
	stage.set_next_frame_data(frame);
	stage.render(0);

	// Sample the last history index (per channel) via uniform
	auto sample_idx_param = stage.find_parameter("sample_index");
	REQUIRE(sample_idx_param != nullptr);
	sample_idx_param->set_value(MAX_TEXTURE_SIZE - 1);

	stage.render(1);

	// Validate output texture contains expected constants per channel
	auto out_param = stage.find_parameter("output_audio_texture");
	REQUIRE(out_param != nullptr);
	const float* out = static_cast<const float*>(out_param->get_value());
	REQUIRE(out != nullptr);

	for (int i = 0; i < BUFFER_SIZE; ++i) {
		REQUIRE(out[i + 0 * BUFFER_SIZE] == Catch::Approx(ch0).margin(1e-6f));
		REQUIRE(out[i + 1 * BUFFER_SIZE] == Catch::Approx(ch1).margin(1e-6f));
	}

	delete global_time;
}

struct History2TestParams {
	int buffer_size;
	int num_channels;
	float window_seconds;
	const char* name;
};

using History2TestParam1 = std::integral_constant<int, 0>; // 256 x 2, 0.5s
using History2TestParam2 = std::integral_constant<int, 1>; // 256 x 2, 2.0s default
using History2TestParam3 = std::integral_constant<int, 2>; // 256 x 4, 1.0s

constexpr History2TestParams get_history2_test_params(int i) {
	constexpr History2TestParams P[] = {
		{256, 2, 0.5f, "256x2_0.5s"},
		{256, 2, 2.0f, "256x2_2.0s"},
		{256, 4, 1.0f, "256x4_1.0s"}
	};
	return P[i];
}

// Minimal fragment shader for testing tape history uniforms
static const char* kTapeHistoryUniformsFrag = "build/tests/mock_tape_history_uniforms_frag.glsl";

static void write_tape_history_uniforms_shader_once() {
	static bool done = false;
	if (done) return;
	std::ofstream fs(kTapeHistoryUniformsFrag);
	fs << R"(
uniform int test_mode; // 0=position, 1=speed, 2=window

void main(){
    vec4 stream_audio = texture(stream_audio_texture, TexCoord);
    
    // Sample the texture to ensure it's bound (even if we get zeros)
    // Use textureSize to ensure the texture is recognized by the shader
    ivec2 tex_size = textureSize(audio_history_texture, 0);
    vec4 tex_sample = texture(audio_history_texture, vec2(0.5, 0.5));
    
    // Calculate normalized values
    float pos_normalized = float(get_tape_position_samples()) / 100000.0; // Normalize to reasonable range
    float speed_normalized = get_tape_speed() / 2.0; // Normalize speed (0-2 range)
    float window_normalized = get_tape_window_size_seconds() / 5.0; // Normalize window (0-5 seconds)
    
    // Use tex_size to prevent optimization - add a tiny offset based on texture size
    float tex_offset = float(tex_size.x) * 0.0000001;
    
    // Output the value we're testing in the red channel (texture is RED only)
    float test_value = 0.0;
    if (test_mode == 0) {
        test_value = clamp(pos_normalized, 0.0, 1.0) + tex_offset;
    } else if (test_mode == 1) {
        test_value = clamp(speed_normalized, 0.0, 1.0) + tex_offset;
    } else if (test_mode == 2) {
        test_value = clamp(window_normalized, 0.0, 1.0) + tex_offset;
    }
    
    output_audio_texture = vec4(test_value, 0.0, 0.0, 1.0);
    debug_audio_texture = stream_audio;
}
)";
	done = true;
}

class MockTapeHistoryStage : public AudioRenderStage {
public:
	MockTapeHistoryStage(unsigned int frames_per_buffer,
	                    unsigned int sample_rate,
	                    unsigned int num_channels,
	                    float window_seconds = 2.0f)
	: AudioRenderStage(frames_per_buffer, sample_rate, num_channels,
	                   kTapeHistoryUniformsFrag,
	                   // include tape history imports explicitly
	                   std::vector<std::string>{
	                   	"build/shaders/global_settings.glsl",
	                   	"build/shaders/frag_shader_settings.glsl",
	                   	"build/shaders/tape_history_settings.glsl"})
	{
		// Create tape history and its texture parameter
		m_history2 = std::make_unique<AudioRenderStageHistory2>(frames_per_buffer, sample_rate, num_channels, window_seconds);
		this->add_parameter(m_history2->create_audio_history_texture(++m_active_texture_count));
		
		// Add uniform parameters
		auto uniform_params = m_history2->get_uniform_parameters();
		for (auto* param : uniform_params) {
			this->add_parameter(param);
		}
		
		// Add test_mode uniform for shader
		auto test_mode = new AudioIntParameter("test_mode", AudioParameter::ConnectionType::INPUT);
		test_mode->set_value(0);
		this->add_parameter(test_mode);
	}
	
	AudioRenderStageHistory2& get_history() { return *m_history2; }

protected:
	void render(const unsigned int time) override {
		// Update the history texture (even if empty, this tests the uniform updates)
		m_history2->update_audio_history_texture();
		AudioRenderStage::render(time);
	}

private:
	std::unique_ptr<AudioRenderStageHistory2> m_history2;
};

TEMPLATE_TEST_CASE("AudioRenderStageHistory2 - auxiliary functions", "[audio_history2][auxiliary][gl_test][template]",
			   TestParam1, TestParam2) {
	write_tape_history_uniforms_shader_once();
	constexpr auto P = get_test_params(TestType::value);
	constexpr int BUFFER_SIZE = P.buffer_size;
	constexpr int NUM_CHANNELS = P.num_channels;
	constexpr int SAMPLE_RATE = 44100;
	
	SDLWindow window(BUFFER_SIZE, NUM_CHANNELS);
	GLContext context;
	
	// Global time buffer for compatibility with default imports
	auto global_time = new AudioIntBufferParameter("global_time", AudioParameter::ConnectionType::INPUT);
	global_time->set_value(0);
	REQUIRE(global_time->initialize());
	
	MockTapeHistoryStage stage(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS);
	REQUIRE(stage.initialize());
	
	context.prepare_draw();
	REQUIRE(stage.bind());
	
	// Test 1: Set and get tape position (samples)
	{
		unsigned int test_position = 12345;
		stage.get_history().set_tape_position(test_position);
		REQUIRE(stage.get_history().get_tape_position() == test_position);
		
		// Verify the parameter was set correctly
		auto tape_pos_param = stage.find_parameter("tape_position");
		REQUIRE(tape_pos_param != nullptr);
		int param_value = *static_cast<const int*>(static_cast<AudioIntParameter*>(tape_pos_param)->get_value());
		REQUIRE(param_value == static_cast<int>(test_position));
		
		// Set test_mode to 0 (position)
		auto test_mode_param = stage.find_parameter("test_mode");
		REQUIRE(test_mode_param != nullptr);
		static_cast<AudioIntParameter*>(test_mode_param)->set_value(0);
		
		stage.render(0);
		
		// Verify position is reflected in shader output (red channel)
		auto out_param = stage.find_parameter("output_audio_texture");
		REQUIRE(out_param != nullptr);
		const float* out = static_cast<const float*>(out_param->get_value());
		REQUIRE(out != nullptr);
		
		float expected_pos_normalized = static_cast<float>(test_position) / 100000.0f;
		float clamped_pos = std::max(0.0f, std::min(1.0f, expected_pos_normalized));
		// Account for tiny texture offset
		float expected_with_offset = clamped_pos + (4096.0f * 0.0000001f);
		// Output texture is RED only (single channel), so access directly without stride
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			REQUIRE(out[i] == Catch::Approx(expected_with_offset).margin(0.001f));
		}
	}
	
	// Test 2: Set and get tape position (seconds)
	{
		float test_position_seconds = 0.5f;
		stage.get_history().set_tape_position(test_position_seconds);
		unsigned int expected_samples = static_cast<unsigned int>(test_position_seconds * SAMPLE_RATE);
		REQUIRE(stage.get_history().get_tape_position() == expected_samples);
		REQUIRE(stage.get_history().get_tape_position_in_seconds() == Catch::Approx(test_position_seconds).margin(0.001f));
		
		// Set test_mode to 0 (position)
		auto test_mode_param = stage.find_parameter("test_mode");
		REQUIRE(test_mode_param != nullptr);
		static_cast<AudioIntParameter*>(test_mode_param)->set_value(0);
		
		stage.render(1);
		
		// Verify position is reflected in shader output
		auto out_param = stage.find_parameter("output_audio_texture");
		REQUIRE(out_param != nullptr);
		const float* out = static_cast<const float*>(out_param->get_value());
		REQUIRE(out != nullptr);
		
		float expected_pos_normalized = static_cast<float>(expected_samples) / 100000.0f;
		float clamped_pos = std::max(0.0f, std::min(1.0f, expected_pos_normalized));
		// Account for tiny texture offset
		float expected_with_offset = clamped_pos + (4096.0f * 0.0000001f);
		// Output texture is RED only (single channel)
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			REQUIRE(out[i] == Catch::Approx(expected_with_offset).margin(0.001f));
		}
	}
	
	// Test 3: Set and get tape speed
	{
		float test_speed = 1.5f;
		stage.get_history().set_tape_speed(test_speed);
		REQUIRE(stage.get_history().get_tape_speed() == Catch::Approx(test_speed).margin(0.001f));
		
		// Set test_mode to 1 (speed)
		auto test_mode_param = stage.find_parameter("test_mode");
		REQUIRE(test_mode_param != nullptr);
		static_cast<AudioIntParameter*>(test_mode_param)->set_value(1);
		
		stage.render(2);
		
		// Verify speed is reflected in shader output (red channel)
		auto out_param = stage.find_parameter("output_audio_texture");
		REQUIRE(out_param != nullptr);
		const float* out = static_cast<const float*>(out_param->get_value());
		REQUIRE(out != nullptr);
		
		float expected_speed_normalized = test_speed / 2.0f;
		float clamped_speed = std::max(0.0f, std::min(1.0f, expected_speed_normalized));
		// Account for tiny texture offset
		float expected_with_offset = clamped_speed + (4096.0f * 0.0000001f);
		// Output texture is RED only (single channel)
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			REQUIRE(out[i] == Catch::Approx(expected_with_offset).margin(0.001f));
		}
	}
	
	// Test 4: Verify window size seconds getter
	{
		float window_size = stage.get_history().get_window_size_seconds();
		REQUIRE(window_size > 0.0f);
		
		// Set test_mode to 2 (window)
		auto test_mode_param = stage.find_parameter("test_mode");
		REQUIRE(test_mode_param != nullptr);
		static_cast<AudioIntParameter*>(test_mode_param)->set_value(2);
		
		stage.render(3);
		
		// Verify window size is reflected in shader output (red channel)
		auto out_param = stage.find_parameter("output_audio_texture");
		REQUIRE(out_param != nullptr);
		const float* out = static_cast<const float*>(out_param->get_value());
		REQUIRE(out != nullptr);
		
		float expected_window_normalized = window_size / 5.0f;
		float clamped_window = std::max(0.0f, std::min(1.0f, expected_window_normalized));
		// Account for tiny texture offset
		float expected_with_offset = clamped_window + (4096.0f * 0.0000001f);
		// Output texture is RED only (single channel)
		for (int i = 0; i < BUFFER_SIZE * NUM_CHANNELS; ++i) {
			REQUIRE(out[i] == Catch::Approx(expected_with_offset).margin(0.001f));
		}
	}
	
	delete global_time;
}

TEMPLATE_TEST_CASE("AudioRenderStageHistory2 - texture dimensions", "[audio_history2][texture_dimensions][template]",
			   History2TestParam1, History2TestParam2, History2TestParam3) {
	constexpr auto P = get_history2_test_params(TestType::value);
	constexpr int BUFFER_SIZE = P.buffer_size;
	constexpr int NUM_CHANNELS = P.num_channels;
	constexpr int SAMPLE_RATE = 44100;
	
	AudioRenderStageHistory2 history(BUFFER_SIZE, SAMPLE_RATE, NUM_CHANNELS, P.window_seconds);
	
	// Texture width should always be MAX_TEXTURE_SIZE
	REQUIRE(history.m_texture_width == MAX_TEXTURE_SIZE);
	
	// Texture height should be MAX_TEXTURE_SIZE (square texture)
	unsigned int texture_height = history.m_texture_rows * NUM_CHANNELS;
	REQUIRE(texture_height == MAX_TEXTURE_SIZE);
	
	// Texture height must be a multiple of num_channels
	REQUIRE(texture_height % NUM_CHANNELS == 0);
	
	// Verify window_size_seconds matches the adjusted texture size
	unsigned int expected_samples = history.m_texture_rows * MAX_TEXTURE_SIZE;
	float expected_window_seconds = static_cast<float>(expected_samples) / static_cast<float>(SAMPLE_RATE);
	REQUIRE(history.m_window_size_seconds == Catch::Approx(expected_window_seconds).margin(0.001f));
}


