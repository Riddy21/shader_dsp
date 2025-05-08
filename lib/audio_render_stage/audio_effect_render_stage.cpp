#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <numeric>  // for std::accumulate

#include "audio_parameter/audio_uniform_parameter.h"
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
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    // TODO: Change balance to be an array of channels
    // Add new parameter objects to the parameter list
    auto gain_parameter =
        new AudioFloatParameter("gain",
                                AudioParameter::ConnectionType::INPUT);
    gain_parameter->set_value(1.0f);

    auto balance_parameter =
        new AudioFloatParameter("balance",
                                AudioParameter::ConnectionType::INPUT);
    balance_parameter->set_value(0.5f);

    if (!this->add_parameter(gain_parameter)) {
        std::cerr << "Failed to add gain_parameter" << std::endl;
    }
    if (!this->add_parameter(balance_parameter)) {
        std::cerr << "Failed to add balance_parameter" << std::endl;
    }
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
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports) {

    // Add new parameter objects to the parameter list
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
                                ++m_active_texture_count,
                                0, GL_NEAREST);

    // Set the echo buffer to the size of the audio data and set to 0
    m_echo_buffer.resize(frames_per_buffer * num_channels * M_MAX_ECHO_BUFFER_SIZE, 0.0f);

    // Set to 0 to start
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

}

void AudioEchoEffectRenderStage::render(unsigned int time) {
    unsigned int last_time = m_time;
    AudioRenderStage::render(time);

    // Get the audio data
    auto * data = (float *)this->find_parameter("stream_audio_texture")->get_value();

    // Update the echo buffer with the new data
    std::copy(data, data + m_frames_per_buffer * m_num_channels, m_echo_buffer.begin());

    if (time != last_time) {
        last_time = time;

        // Shift the echo buffer
        std::copy(m_echo_buffer.begin(), m_echo_buffer.end() - m_frames_per_buffer * m_num_channels, m_echo_buffer.begin() + m_frames_per_buffer * m_num_channels);
    }

    // Always set echo_audio_texture to the current m_echo_buffer
    this->find_parameter("echo_audio_texture")->set_value(m_echo_buffer.data());

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
    : AudioRenderStage(frames_per_buffer, sample_rate, num_channels, fragment_shader_path, frag_shader_imports),
      NYQUIST(sample_rate / 2.0f),
      m_low_pass(1.0f),
      m_high_pass(230.0f),
      m_filter_follower(0.0f),
      m_resonance(1.0f) {

    m_audio_history = std::make_unique<AudioRenderStageHistory>(MAX_TEXTURE_SIZE, frames_per_buffer, sample_rate, num_channels);

    // Valid range is 1 - buffer_size * num_channels
    auto num_taps_parameter =
        new AudioIntParameter("num_taps",
                                AudioParameter::ConnectionType::INPUT);
    num_taps_parameter->set_value(1001); // Strange restriction, this cannot be larger than the buffer size

    auto b_coeff_texture =
        new AudioTexture2DParameter("b_coeff_texture",
                                AudioParameter::ConnectionType::INPUT,
                                MAX_TEXTURE_SIZE, 1, // Due to restriction of the shader only can be as big as the buffer size
                                ++m_active_texture_count,
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
    unsigned int last_time = m_time;
    AudioRenderStage::render(time);


    auto * data = (float *)this->find_parameter("stream_audio_texture")->get_value();

    if (m_filter_follower != 0.0f) {
        float current_amplitude = std::accumulate(data, data + m_frames_per_buffer * m_num_channels, 0.0f, [](float sum, float value) {
            return sum + std::fabs(value);
        }) / (m_frames_per_buffer * m_num_channels);
        update_b_coefficients(current_amplitude);
    }

    if (time != last_time) {
        m_audio_history->shift_history_buffer();
    }

    m_audio_history->save_stream_to_history(data);
    m_audio_history->update_audio_history_texture();
}

void AudioFrequencyFilterEffectRenderStage::update_b_coefficients(const float current_amplitude) {
    float low_pass = m_low_pass + m_filter_follower * current_amplitude;
    float high_pass = m_high_pass + m_filter_follower * current_amplitude;
    auto b_coeff = calculate_firwin_b_coefficients(low_pass/NYQUIST, high_pass/NYQUIST, *(int *)this->find_parameter("num_taps")->get_value(), m_resonance);
    b_coeff.resize(MAX_TEXTURE_SIZE, 0.0);
    this->find_parameter("b_coeff_texture")->set_value(b_coeff.data());
}