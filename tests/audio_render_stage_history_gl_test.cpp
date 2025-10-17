// Mock-stage focused test for AudioRenderStageHistory without chaining to final output

#include "catch2/catch_all.hpp"
#include "framework/test_gl.h"

#include "audio_core/audio_render_stage.h"
#include "audio_render_stage/audio_render_stage_history.h"
#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_buffer_parameter.h"

#include <vector>
#include <fstream>

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


