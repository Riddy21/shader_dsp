// Uniforms for envelope parameters
uniform float attack_time;   // Time for the attack phase
uniform float decay_time;    // Time for the decay phase
uniform float sustain_level; // Level for the sustain phase
uniform float release_time;  // Time for the release phase

// ADSR envelope
float adsr_envelope(float start_time, float end_time, float time) {
    // If play is on, calculate attack and decay
    float from_start_time = time - start_time;
    float from_end_time = time - end_time;
    float current_level;

    if (from_start_time < attack_time) {
        current_level = from_start_time / attack_time;
    } else if (from_start_time < attack_time + decay_time) {
        current_level = 1.0 - (1.0 - sustain_level) * (from_start_time - attack_time) / decay_time;
    } else {
        current_level = sustain_level;
    }

    // If play is on, return current level
    if (play) {
        return current_level;
    // If play is released before the sustain phase is complete, release from the current level
    } else {
        if (from_end_time < release_time) {
            return current_level - current_level * from_end_time / release_time;
        } else {
            return 0.0;
        }
    }
}