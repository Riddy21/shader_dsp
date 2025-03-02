// Uniforms for envelope parameters
uniform float attack_time;   // Time for the attack phase
uniform float decay_time;    // Time for the decay phase
uniform float sustain_level; // Level for the sustain phase
uniform float release_time;  // Time for the release phase

/**
 * Returns a smoothed ADSR value at "time", starting from "start_time"
 * and ending at "end_time". When "play" goes false, we release from
 * the *current* envelope level rather than jumping to sustain.
 */
float custom_smoothstep(float edge0, float edge1, float x) {
    // Scale, bias and saturate x to 0..1 range
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    // Evaluate polynomial
    return x * x * (3.0 - 2.0 * x);
}

float adsr_envelope(float start_time, float end_time, float time) {
    float from_start = time - start_time;  // Time since note-on
    float from_end   = time - end_time;    // Time since note-off

    // If playback hasn't started yet, envelope is 0
    if (from_start < 0.0) {
        return 0.0;
    }

    //--------------------------------------------------------------------------
    // 1) Compute "currentLevel" as if the note were still playing
    //--------------------------------------------------------------------------
    float currentLevel;
    if (from_start <= attack_time) {
        // Attack: smoothly go 0 → 1
        currentLevel = custom_smoothstep(0.0, attack_time, from_start);
    }
    else {
        float after_attack = from_start - attack_time;
        if (after_attack <= decay_time) {
            // Decay: smoothly go 1 → sustain_level
            float t = after_attack / decay_time; 
            currentLevel = mix(1.0, sustain_level, custom_smoothstep(0.0, 1.0, t));
        }
        else {
            // Sustain
            currentLevel = sustain_level;
        }
    }

    //--------------------------------------------------------------------------
    // 2) If play = false, apply release from "currentLevel" → 0
    //--------------------------------------------------------------------------
    if (!play) {
        // If release hasn’t started yet (time < end_time), stay at currentLevel
        if (from_end < 0.0) {
            return currentLevel;
        }
        // If in the middle of release_time, smoothly go currentLevel → 0
        else if (from_end <= release_time) {
            float t = from_end / release_time; 
            return mix(currentLevel, 0.0, custom_smoothstep(0.0, 1.0, t));
        }
        // If release fully completed
        else {
            return 0.0;
        }
    }

    // If still playing, just return the currentLevel
    return currentLevel;
}