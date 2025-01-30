// Uniforms for envelope parameters
uniform float attack_time;   // Time for the attack phase
uniform float decay_time;    // Time for the decay phase
uniform float sustain_level; // Level for the sustain phase
uniform float release_time;  // Time for the release phase

// Smooth ADSR envelope using smoothstep instead of a previous-envelope variable.
float adsr_envelope(float start_time, float end_time, float time) {
    float from_start = time - start_time;
    float from_end   = time - end_time;

    // If playback hasn't started yet, envelope is 0.
    if (from_start < 0.0) {
        return 0.0;
    }

    if (play) {
        // -----------------------
        // Attack: smoothly go 0 → 1
        // -----------------------
        if (from_start <= attack_time) {
            // Progress 0..1 over the attack_time range
            // smoothstep(x0, x1, x) transitions smoothly from 0..1 across [x0, x1]
            return smoothstep(0.0, attack_time, from_start);
        }

        // -----------------------
        // Decay: smoothly go 1 → sustain_level
        // -----------------------
        float after_attack = from_start - attack_time;
        if (after_attack <= decay_time) {
            float t = after_attack / decay_time; // normalized 0..1 in decay range
            // smoothly blend from 1.0 down to sustain_level
            return mix(1.0, sustain_level, smoothstep(0.0, 1.0, t));
        }

        // -----------------------
        // Sustain
        // -----------------------
        return sustain_level;

    } else {
        // -----------------------
        // Release: smoothly go from current level → 0
        // -----------------------
        // If release hasn’t started (time < end_time), just stay at sustain_level:
        if (from_end < 0.0) {
            return sustain_level;
        }

        // If we're still within release_time, do a smooth transition to 0.0
        if (from_end <= release_time) {
            float t = from_end / release_time; // 0..1 across release
            // smoothly blend from sustain_level to 0
            return mix(sustain_level, 0.0, smoothstep(0.0, 1.0, t));
        }

        // Once release_time is exceeded, envelope is fully at 0
        return 0.0;
    }
}