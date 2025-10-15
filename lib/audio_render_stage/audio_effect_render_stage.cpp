#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <numeric>  // for std::accumulate

#include "audio_parameter/audio_uniform_parameter.h"
#include "audio_parameter/audio_uniform_array_parameter.h"
#include "audio_parameter/audio_texture2d_parameter.h"
#include "audio_render_stage/audio_effect_render_stage.h"

const std::vector<std::string> AudioGainEffectRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioGainEffectRenderStage::AudioGainEffectRenderStage(const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioGainEffectRenderStage("GainEffect-" + std::to_string(generate_id()),
                                 frames_per_buffer, sample_rate, num_channels,
                                 fragment_shader_path, frag_shader_imports) {}

AudioGainEffectRenderStage::AudioGainEffectRenderStage(const std::string & stage_name,
                                               const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioEffectRenderStage(stage_name, frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    constexpr size_t MAX_CHANNELS = 8;
    auto gains_parameter =
        new AudioFloatArrayParameter("gains",
                                     AudioParameter::ConnectionType::INPUT,
                                     MAX_CHANNELS);
    float* gains = new float[MAX_CHANNELS];
    for (size_t i = 0; i < MAX_CHANNELS; i++) {
        gains[i] = 1.0f;
    }
    gains_parameter->set_value(gains);

    if (!this->add_parameter(gains_parameter)) {
        std::cerr << "Failed to add gains_parameter" << std::endl;
    }

    m_controls.clear();
    auto gain_control = std::make_shared<AudioControl<std::vector<float>>>(
        "gain",
        std::vector<float>(num_channels, 1.0f),
        [this](const std::vector<float>& gains) { this->set_channel_gains(gains); }
    );
    m_controls.push_back(gain_control);
}

void AudioGainEffectRenderStage::set_channel_gains(const std::vector<float>& channel_gains) {
    constexpr size_t MAX_CHANNELS = 8;
    auto gains_param = find_parameter("gains");
    if (!gains_param) {
        std::cerr << "Error: gains parameter not found" << std::endl;
        return;
    }
    
    // Error checking: vector size cannot exceed num_channels or MAX_CHANNELS
    if (channel_gains.size() > num_channels) {
        std::cerr << "Error: provided " << channel_gains.size() << " channel gains, but render stage only has " 
                  << num_channels << " channels" << std::endl;
        return;
    }
    
    if (channel_gains.size() > MAX_CHANNELS) {
        std::cerr << "Error: provided " << channel_gains.size() << " channel gains, but maximum supported is " 
                  << MAX_CHANNELS << " channels" << std::endl;
        return;
    }
    
    // Create a new full-size array and fill it
    float* full_gains = new float[MAX_CHANNELS];
    
    // Copy the provided channel gains (up to num_channels)
    for (size_t i = 0; i < channel_gains.size(); i++) {
        full_gains[i] = channel_gains[i];
    }
    
    // Fill remaining slots up to num_channels with 1.0f (unity gain)
    for (size_t i = channel_gains.size(); i < num_channels; i++) {
        full_gains[i] = 1.0f;
    }
    
    // Fill remaining slots beyond num_channels with 1.0f (for unused channels)
    for (size_t i = num_channels; i < MAX_CHANNELS; i++) {
        full_gains[i] = 1.0f;
    }
    
    gains_param->set_value(full_gains);

    delete[] full_gains;
}

const std::vector<std::string> AudioEchoEffectRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl"
};

AudioEchoEffectRenderStage::AudioEchoEffectRenderStage(const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioEchoEffectRenderStage("EchoEffect-" + std::to_string(generate_id()),
                                 frames_per_buffer, sample_rate, num_channels,
                                 fragment_shader_path, frag_shader_imports) {}

AudioEchoEffectRenderStage::AudioEchoEffectRenderStage(const std::string & stage_name,
                                               const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioEffectRenderStage(stage_name, frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    auto feedback_parameter =
        new AudioIntParameter("num_echos",
                                AudioParameter::ConnectionType::INPUT);
    feedback_parameter->set_value(5);

    auto delay_parameter =
        new AudioFloatParameter("delay",
                                AudioParameter::ConnectionType::INPUT);
    delay_parameter->set_value(0.1f);

    auto decay_parameter =
        new AudioFloatParameter("decay",
                                AudioParameter::ConnectionType::INPUT);
    decay_parameter->set_value(.4f);

    auto echo_audio_texture =
        new AudioTexture2DParameter("echo_audio_texture",
                                AudioParameter::ConnectionType::INPUT,
                                frames_per_buffer, M_MAX_ECHO_BUFFER_SIZE * num_channels, // Around 2s of audio data
                                m_active_texture_count++,
                                0, GL_NEAREST);

    m_echo_buffer.resize(frames_per_buffer * num_channels * M_MAX_ECHO_BUFFER_SIZE, 0.0f);
    echo_audio_texture->set_value(m_echo_buffer.data());

    if (!this->add_parameter(feedback_parameter)) {
        std::cerr << "Failed to add feedback_parameter" << std::endl;
    }
    if (!this->add_parameter(delay_parameter)) {
        std::cerr << "Failed to add delay_parameter" << std::endl;
    }
    if (!this->add_parameter(decay_parameter)) {
        std::cerr << "Failed to add decay_parameter" << std::endl;
    }
    if (!this->add_parameter(echo_audio_texture)) {
        std::cerr << "Failed to add echo_audio_texture" << std::endl;
    }

    m_controls.clear();
    auto num_echos_control = std::make_shared<AudioControl<int>>(
        "num_echos",
        5,
        [feedback_parameter](const int& v) { feedback_parameter->set_value(v); }
    );
    m_controls.push_back(num_echos_control);

    auto delay_control = std::make_shared<AudioControl<float>>(
        "delay",
        0.1f,
        [delay_parameter](const float& v) { delay_parameter->set_value(v); }
    );
    m_controls.push_back(delay_control);

    auto decay_control = std::make_shared<AudioControl<float>>(
        "decay",
        0.4f,
        [decay_parameter](const float& v) { decay_parameter->set_value(v); }
    );
    m_controls.push_back(decay_control);
}

void AudioEchoEffectRenderStage::render(unsigned int time) {
    // Always set echo_audio_texture to the current m_echo_buffer
    this->find_parameter("echo_audio_texture")->set_value(m_echo_buffer.data());

    if (time != m_time) {
        // Shift the echo buffer
        std::copy(m_echo_buffer.begin(), m_echo_buffer.end() - frames_per_buffer * num_channels, m_echo_buffer.begin() + frames_per_buffer * num_channels);
    }

    AudioRenderStage::render(time);

    // Get the audio data
    auto * data = (float *)this->find_parameter("output_audio_texture")->get_value();

    // Update the echo buffer with the new data
    std::copy(data, data + frames_per_buffer * num_channels, m_echo_buffer.begin());
}

bool AudioEchoEffectRenderStage::disconnect_render_stage(AudioRenderStage * render_stage) {
    // Disconnect the render stage
    if (!AudioEffectRenderStage::disconnect_render_stage(render_stage)) {
        std::cerr << "Failed to disconnect render stage" << std::endl;
        return false;
    }

    // Clear the echo buffer
    std::fill(m_echo_buffer.begin(), m_echo_buffer.end(), 0.0f);
    return true;
}

const std::vector<std::string> AudioFrequencyFilterEffectRenderStage::default_frag_shader_imports = {
    "build/shaders/global_settings.glsl",
    "build/shaders/frag_shader_settings.glsl",
    "build/shaders/history_settings.glsl"
};

AudioFrequencyFilterEffectRenderStage::AudioFrequencyFilterEffectRenderStage(const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioFrequencyFilterEffectRenderStage("FrequencyFilterEffect-" + std::to_string(generate_id()),
                                            frames_per_buffer, sample_rate, num_channels,
                                            fragment_shader_path, frag_shader_imports) {}

AudioFrequencyFilterEffectRenderStage::AudioFrequencyFilterEffectRenderStage(const std::string & stage_name,
                                               const unsigned int frames_per_buffer,
                                               const unsigned int sample_rate,
                                               const unsigned int num_channels,
                                               const std::string & fragment_shader_path,
                                               const std::vector<std::string> & frag_shader_imports)
    : AudioEffectRenderStage(stage_name, frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports),
      NYQUIST(sample_rate / 2.0f),
      m_low_pass(1.0f),
      m_high_pass(230.0f),
      m_filter_follower(0.0f),
      m_resonance(1.0f) {

    m_audio_history = std::make_unique<AudioRenderStageHistory>(MAX_TEXTURE_SIZE, frames_per_buffer, sample_rate, num_channels);

    auto num_taps_parameter =
        new AudioIntParameter("num_taps",
                                AudioParameter::ConnectionType::INPUT);
    num_taps_parameter->set_value(frames_per_buffer - 1);

    auto b_coeff_texture =
        new AudioTexture2DParameter("b_coeff_texture",
                                AudioParameter::ConnectionType::INPUT,
                                MAX_TEXTURE_SIZE, 1,
                                m_active_texture_count++,
                                0, GL_NEAREST);
    
    if (!this->add_parameter(num_taps_parameter)) {
        std::cerr << "Failed to add num_taps_parameter" << std::endl;
    }
    if (!this->add_parameter(b_coeff_texture)) {
        std::cerr << "Failed to add b_coeff_texture" << std::endl;
    }
    if (!this->add_parameter(m_audio_history->create_audio_history_texture(++m_active_texture_count))) {
        std::cerr << "Failed to add audio_history_texture" << std::endl;
    }

    m_controls.clear();
    auto low_pass_control = std::make_shared<AudioControl<float>>(
        "low_pass",
        m_low_pass,
        [this](const float& v) { this->set_low_pass(v); }
    );
    m_controls.push_back(low_pass_control);

    auto high_pass_control = std::make_shared<AudioControl<float>>(
        "high_pass",
        m_high_pass,
        [this](const float& v) { this->set_high_pass(v); }
    );
    m_controls.push_back(high_pass_control);

    auto filter_follower_control = std::make_shared<AudioControl<float>>(
        "filter_follower",
        m_filter_follower,
        [this](const float& v) { this->set_filter_follower(v); }
    );
    m_controls.push_back(filter_follower_control);

    auto resonance_control = std::make_shared<AudioControl<float>>(
        "resonance",
        m_resonance,
        [this](const float& v) { this->set_resonance(v); }
    );
    m_controls.push_back(resonance_control);

    update_b_coefficients();
}

// Assume this function is a member of AudioFrequencyFilterEffectRenderStage.
const std::vector<float> AudioFrequencyFilterEffectRenderStage::calculate_firwin_b_coefficients(
    const float low_pass_param,
    const float high_pass_param,
    const unsigned int num_taps,
    const float resonance)
{
    float low_pass = low_pass_param;
    float high_pass = high_pass_param;

    // Check that we have at least one positive cutoff.
    if (low_pass <= 0.0f) {
        low_pass = 0.0f;
    }
    if (high_pass <= 0.0f) {
        high_pass = 0.0f;
    }

    // Create a vector for the filter coefficients.
    std::vector<float> h(num_taps, 0.0f);
    const double M = (num_taps - 1) / 2.0;  // center index

    // Helper lambda: normalized sinc function: sin(pi*x)/(pi*x) (with sinc(0)=1).
    auto sinc = [](double x) -> double {
        const double pi = M_PI;
        if (std::fabs(x) < 1e-8)
            return 1.0;
        return std::sin(pi * x) / (pi * x);
    };

    // Helper lambda: lowpass impulse response for cutoff f.
    auto lowpass_ir = [=](double f, double n) -> double {
        // f * sinc(f*(n-M))
        return f * sinc(f * (n - M));
    };

    // Determine filter type:
    // We'll define our filter type by an enum.
    enum FilterType { Lowpass, Highpass, Bandpass, Bandstop };
    FilterType filter_type;
    double cutoff = 0.0; // used for single-cutoff filters
    double f1 = 0.0, f2 = 0.0;  // used for two-cutoff filters

    if (low_pass <= 0.0f && high_pass > 0.0f)
    {
        // Only high_pass is positive: design a lowpass filter with cutoff = high_pass.
        filter_type = Lowpass;
        cutoff = high_pass;
    }
    else if (high_pass <= 0.0f && low_pass > 0.0f)
    {
        // Only low_pass is positive: design a highpass filter with cutoff = low_pass.
        filter_type = Highpass;
        cutoff = low_pass;
    }
    else // both cutoffs are positive
    {
        if (low_pass < high_pass)
        {
            // Bandpass filter: pass frequencies between low_pass and high_pass.
            filter_type = Bandpass;
            f1 = low_pass;
            f2 = high_pass;
        }
        else // low_pass > high_pass
        {
            // Bandstop (band reject) filter: reject frequencies between high_pass and low_pass.
            filter_type = Bandstop;
            // Here we sort the frequencies so that f1 < f2.
            f1 = high_pass;
            f2 = low_pass;
        }
    }

    // Compute the ideal impulse response.
    for (unsigned int n = 0; n < num_taps; n++)
    {
        switch (filter_type)
        {
            case Lowpass:
                // Lowpass filter: use the lowpass impulse response at cutoff.
                h[n] = lowpass_ir(cutoff, n);
                break;
            case Highpass:
                // Highpass filter: spectral inversion.
                // h[n] = δ[n-M] - lowpass_ir(cutoff, n)
                h[n] = ((std::fabs(n - M) < 1e-8) ? 1.0 : 0.0) - lowpass_ir(cutoff, n);
                break;
            case Bandpass:
                // Bandpass filter: difference between two lowpass responses.
                // h[n] = lowpass_ir(f2, n) - lowpass_ir(f1, n)
                h[n] = lowpass_ir(f2, n) - lowpass_ir(f1, n);
                break;
            case Bandstop:
                // Bandstop filter: spectral inversion of the bandpass.
                // h[n] = δ[n-M] - (lowpass_ir(f2, n) - lowpass_ir(f1, n))
                h[n] = ((std::fabs(n - M) < 1e-8) ? 1.0 : 0.0) - (lowpass_ir(f2, n) - lowpass_ir(f1, n));
                break;
        }
    }

    // Apply Hamming window to reduce ripple in the frequency response.
    // Hamming window: w[n] = 0.54 - 0.46*cos(2*pi*n/(N-1))
    for (unsigned int n = 0; n < num_taps; n++)
    {
        double w = 0.54 - 0.46 * std::cos(2 * M_PI * n / (num_taps - 1));
        h[n] *= w;
    }

    // Apply resonance by scaling the coefficients.
    for (unsigned int n = 0; n < num_taps; n++)
    {
        h[n] *= std::pow(resonance, std::fabs(n - M) / M);
    }

    // For filters that pass DC (lowpass and bandstop), normalize the coefficients so that their sum equals one.
    // (Highpass and bandpass filters naturally have zero DC gain.)
    if (filter_type == Lowpass || filter_type == Bandstop)
    {
        double sum = std::accumulate(h.begin(), h.end(), 0.0);
        if (std::fabs(sum) > 1e-8)
        {
            for (unsigned int n = 0; n < num_taps; n++)
                h[n] = static_cast<float>(h[n] / sum);
        }
    }

    return h;
}



void AudioFrequencyFilterEffectRenderStage::render(const unsigned int time) {
    auto * data = (float *)this->find_parameter("stream_audio_texture")->get_value();

    if (m_b_coefficients_dirty) {
        float current_amplitude = std::accumulate(data, data + frames_per_buffer * num_channels, 0.0f, [](float sum, float value) {
            return sum + std::fabs(value);
        }) / (frames_per_buffer * num_channels);
        update_b_coefficients(current_amplitude);
    }
    
    if (time != m_time) {
        m_audio_history->shift_history_buffer();
    }

    m_audio_history->save_stream_to_history(data);
    m_audio_history->update_audio_history_texture();

    AudioRenderStage::render(time);
}

void AudioFrequencyFilterEffectRenderStage::update_b_coefficients(const float current_amplitude) {
    // Current amplitude is used to follow the filter and adjust the cutoff frequency
    float low_pass = m_low_pass + m_filter_follower * current_amplitude;
    float high_pass = m_high_pass + m_filter_follower * current_amplitude;
    auto b_coeff = calculate_firwin_b_coefficients(low_pass/NYQUIST, high_pass/NYQUIST, *(int *)this->find_parameter("num_taps")->get_value(), m_resonance);
    b_coeff.resize(MAX_TEXTURE_SIZE, 0.0);
    this->find_parameter("b_coeff_texture")->set_value(b_coeff.data());
    m_b_coefficients_dirty = false;
}

bool AudioFrequencyFilterEffectRenderStage::disconnect_render_stage(AudioRenderStage * render_stage) {
    // Disconnect the render stage
    if (!AudioEffectRenderStage::disconnect_render_stage(render_stage)) {
        std::cerr << "Failed to disconnect render stage" << std::endl;
        return false;
    }

    // Clear history buffer
    m_audio_history->clear_history_buffer();
    return true;
}