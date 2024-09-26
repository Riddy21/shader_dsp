#include <iostream>
#include <cmath>
#include <jack/jack.h>

#define SAMPLE_RATE 44100
#define FREQUENCY 440.0 // Frequency of the sine wave (A4)
#define AMPLITUDE 0.5   // Amplitude of the sine wave

jack_port_t *output_port;
jack_client_t *client;
double phase = 0.0;
double phase_increment = 2.0 * M_PI * FREQUENCY / SAMPLE_RATE;

int process(jack_nframes_t nframes, void *arg) {
    jack_default_audio_sample_t *out = (jack_default_audio_sample_t *)jack_port_get_buffer(output_port, nframes);

    for (unsigned int i = 0; i < nframes; i++) {
        out[i] = AMPLITUDE * sin(phase);
        phase += phase_increment;
        if (phase >= 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
    }

    return 0;
}

int main() {
    const char *client_name = "sine_wave";
    jack_options_t options = JackNullOption;
    jack_status_t status;

    client = jack_client_open(client_name, options, &status);
    if (client == nullptr) {
        std::cerr << "jack_client_open() failed, status = 0x" << std::hex << status << std::endl;
        return 1;
    }

    jack_set_process_callback(client, process, 0);

    output_port = jack_port_register(client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (output_port == nullptr) {
        std::cerr << "no more JACK ports available" << std::endl;
        return 1;
    }

    if (jack_activate(client)) {
        std::cerr << "cannot activate client" << std::endl;
        return 1;
    }

    std::cout << "Press Enter to stop the client..." << std::endl;
    std::cin.get();

    jack_client_close(client);

    return 0;
}