#!/usr/bin/env python3
"""
Interpolated Audio Analysis Script

This script analyzes the interpolated 2-channel audio data from the second test case.
The data is stored as interleaved samples: [L0, R0, L1, R1, L2, R2, ...]
"""

import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
import os
import sys

def load_interpolated_audio(filename, sample_rate=44100, num_channels=2, max_duration=1.0):
    """Load interpolated audio data from binary file."""
    try:
        # Load binary data
        data = np.fromfile(filename, dtype=np.float32)
        print(f"Loaded {len(data)} samples from {filename}")
        
        # Limit to max_duration seconds
        max_samples = int(sample_rate * max_duration * num_channels)
        if len(data) > max_samples:
            data = data[:max_samples]
            print(f"Limited to {max_samples} samples ({max_duration} seconds)")
        
        # Separate channels (interleaved format: L0, R0, L1, R1, ...)
        left_channel = data[0::num_channels]
        right_channel = data[1::num_channels]
        
        print(f"Extracted {len(left_channel)} left channel samples")
        print(f"Extracted {len(right_channel)} right channel samples")
        
        return left_channel, right_channel
        
    except Exception as e:
        print(f"Error loading {filename}: {e}")
        return None, None

def detect_glitches(audio_data, sample_rate=44100, threshold=0.1, buffer_size=512):
    """Detect glitches by looking for sudden jumps in amplitude."""
    glitches = []
    
    for i in range(1, len(audio_data)):
        diff = abs(audio_data[i] - audio_data[i-1])
        
        # Skip frame boundaries (every buffer_size samples) as they might have legitimate discontinuities
        if i % buffer_size == 0:
            continue
            
        if diff > threshold:
            glitches.append({
                'index': i,
                'time': i / sample_rate,
                'amplitude_jump': diff,
                'sample_before': audio_data[i-1],
                'sample_after': audio_data[i]
            })
    
    return glitches

def detect_glitches_enhanced(audio_data, sample_rate=44100, threshold=0.1, buffer_size=512):
    """Enhanced glitch detection with detailed analysis."""
    glitches = []
    frame_boundaries = []
    
    # Mark frame boundaries
    for i in range(0, len(audio_data), buffer_size):
        frame_boundaries.append(i)
    
    for i in range(1, len(audio_data)):
        diff = abs(audio_data[i] - audio_data[i-1])
        
        # Check if this is a frame boundary
        is_frame_boundary = i in frame_boundaries
        
        if diff > threshold:
            # Calculate frame number and position within frame
            frame_number = i // buffer_size
            position_in_frame = i % buffer_size
            
            glitches.append({
                'index': i,
                'time': i / sample_rate,
                'amplitude_jump': diff,
                'sample_before': audio_data[i-1],
                'sample_after': audio_data[i],
                'is_frame_boundary': is_frame_boundary,
                'frame_number': frame_number,
                'position_in_frame': position_in_frame,
                'frame_time': frame_number * buffer_size / sample_rate
            })
    
    return glitches, frame_boundaries

def analyze_buffer_boundaries(audio_data, sample_rate=44100, buffer_size=512):
    """Analyze discontinuities at buffer boundaries."""
    boundary_issues = []
    
    for frame in range(1, len(audio_data) // buffer_size):
        frame_start = frame * buffer_size
        frame_end = frame_start + buffer_size
        
        if frame_end < len(audio_data):
            # Check for discontinuity between frames
            last_sample_prev_frame = audio_data[frame_start - 1]
            first_sample_curr_frame = audio_data[frame_start]
            
            diff = abs(first_sample_curr_frame - last_sample_prev_frame)
            
            if diff > 0.01:  # Lower threshold for boundary analysis
                boundary_issues.append({
                    'frame': frame,
                    'time': frame_start / sample_rate,
                    'discontinuity': diff,
                    'last_sample_prev': last_sample_prev_frame,
                    'first_sample_curr': first_sample_curr_frame
                })
    
    return boundary_issues

def analyze_phase_continuity(audio_data, sample_rate=44100, expected_frequency=440.0):
    """Analyze phase continuity to detect phase jumps."""
    phase_jumps = []
    
    # Calculate expected phase increment per sample
    phase_increment = 2.0 * np.pi * expected_frequency / sample_rate
    
    # Track phase
    current_phase = 0.0
    for i in range(1, len(audio_data)):
        # Estimate phase from amplitude (assuming sine wave)
        if audio_data[i-1] != 0:
            estimated_phase = np.arcsin(audio_data[i-1] / np.max(np.abs(audio_data)))
        else:
            estimated_phase = 0.0
            
        # Check for phase discontinuity
        phase_diff = abs(estimated_phase - current_phase)
        if phase_diff > np.pi:  # Significant phase jump
            phase_jumps.append({
                'index': i,
                'time': i / sample_rate,
                'phase_jump': phase_diff,
                'sample_value': audio_data[i]
            })
        
        current_phase = estimated_phase
    
    return phase_jumps

def analyze_frequency(audio_data, sample_rate=44100):
    """Analyze the frequency content of the audio."""
    # Calculate FFT
    fft = np.fft.fft(audio_data)
    freqs = np.fft.fftfreq(len(audio_data), 1/sample_rate)
    
    # Get positive frequencies only
    positive_freqs = freqs[:len(freqs)//2]
    positive_fft = np.abs(fft[:len(fft)//2])
    
    # Find peak frequency
    peak_idx = np.argmax(positive_fft)
    peak_freq = positive_freqs[peak_idx]
    peak_amplitude = positive_fft[peak_idx]
    
    return positive_freqs, positive_fft, peak_freq, peak_amplitude

def calculate_rms(audio_data):
    """Calculate RMS (Root Mean Square) of the audio."""
    return np.sqrt(np.mean(audio_data**2))

def calculate_crest_factor(audio_data):
    """Calculate crest factor (peak to RMS ratio)."""
    peak = np.max(np.abs(audio_data))
    rms = calculate_rms(audio_data)
    return peak / rms if rms > 0 else 0

def plot_interpolated_audio_analysis(left_data, right_data, sample_rate=44100, buffer_size=512):
    """Create comprehensive analysis plots for interpolated audio."""
    
    # Create figure with subplots
    fig = plt.figure(figsize=(20, 16))
    
    # 1. Time domain waveform
    ax1 = plt.subplot(4, 2, 1)
    time = np.arange(len(left_data)) / sample_rate
    ax1.plot(time, left_data, 'b-', alpha=0.7, label='Left Channel')
    ax1.plot(time, right_data, 'r-', alpha=0.7, label='Right Channel')
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Amplitude')
    ax1.set_title('Interpolated Audio Waveform')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # 2. Zoomed in view of first second
    ax2 = plt.subplot(4, 2, 2)
    samples_per_second = sample_rate
    ax2.plot(time[:samples_per_second], left_data[:samples_per_second], 'b-', label='Left')
    ax2.plot(time[:samples_per_second], right_data[:samples_per_second], 'r-', label='Right')
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Amplitude')
    ax2.set_title('First Second (Zoomed)')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    
    # 3. Sample difference analysis
    ax3 = plt.subplot(4, 2, 3)
    left_diff = np.diff(left_data)
    right_diff = np.diff(right_data)
    time_diff = time[1:]
    
    ax3.plot(time_diff, left_diff, 'b-', alpha=0.7, label='Left Channel')
    ax3.plot(time_diff, right_diff, 'r-', alpha=0.7, label='Right Channel')
    ax3.axhline(y=0.1, color='g', linestyle='--', alpha=0.5, label='Glitch Threshold')
    ax3.axhline(y=-0.1, color='g', linestyle='--', alpha=0.5)
    ax3.set_xlabel('Time (s)')
    ax3.set_ylabel('Sample Difference')
    ax3.set_title('Sample-to-Sample Differences')
    ax3.legend()
    ax3.grid(True, alpha=0.3)
    
    # 4. Frequency spectrum
    ax4 = plt.subplot(4, 2, 4)
    freqs_left, fft_left, peak_freq_left, peak_amp_left = analyze_frequency(left_data, sample_rate)
    freqs_right, fft_right, peak_freq_right, peak_amp_right = analyze_frequency(right_data, sample_rate)
    
    ax4.semilogy(freqs_left, fft_left, 'b-', alpha=0.7, label=f'Left (Peak: {peak_freq_left:.1f} Hz)')
    ax4.semilogy(freqs_right, fft_right, 'r-', alpha=0.7, label=f'Right (Peak: {peak_freq_right:.1f} Hz)')
    ax4.set_xlabel('Frequency (Hz)')
    ax4.set_ylabel('Magnitude')
    ax4.set_title('Frequency Spectrum')
    ax4.legend()
    ax4.grid(True, alpha=0.3)
    ax4.set_xlim(0, 2000)  # Focus on lower frequencies
    
    # 5. Glitch detection visualization
    ax5 = plt.subplot(4, 2, 5)
    left_glitches, _ = detect_glitches_enhanced(left_data, sample_rate, threshold=0.1, buffer_size=buffer_size)
    right_glitches, _ = detect_glitches_enhanced(right_data, sample_rate, threshold=0.1, buffer_size=buffer_size)
    
    # Plot glitches
    for glitch in left_glitches:
        ax5.axvline(x=glitch['time'], color='red', alpha=0.5, linewidth=2)
    for glitch in right_glitches:
        ax5.axvline(x=glitch['time'], color='orange', alpha=0.5, linewidth=2)
    
    ax5.plot(time, left_data, 'b-', alpha=0.7, label='Left Channel')
    ax5.plot(time, right_data, 'r-', alpha=0.7, label='Right Channel')
    ax5.set_xlabel('Time (s)')
    ax5.set_ylabel('Amplitude')
    ax5.set_title(f'Glitch Detection (Red: Left, Orange: Right)\nLeft: {len(left_glitches)} glitches, Right: {len(right_glitches)} glitches')
    ax5.legend()
    ax5.grid(True, alpha=0.3)
    
    # 6. Histogram of sample values
    ax6 = plt.subplot(4, 2, 6)
    ax6.hist(left_data, bins=100, alpha=0.7, label='Left Channel', color='blue')
    ax6.hist(right_data, bins=100, alpha=0.7, label='Right Channel', color='red')
    ax6.set_xlabel('Sample Value')
    ax6.set_ylabel('Frequency')
    ax6.set_title('Sample Value Distribution')
    ax6.legend()
    ax6.grid(True, alpha=0.3)
    
    # 7. RMS over time
    ax7 = plt.subplot(4, 2, 7)
    window_size = sample_rate // 10  # 100ms windows
    left_rms = []
    right_rms = []
    rms_times = []
    
    for i in range(0, len(left_data) - window_size, window_size // 2):
        left_rms.append(calculate_rms(left_data[i:i+window_size]))
        right_rms.append(calculate_rms(right_data[i:i+window_size]))
        rms_times.append(i / sample_rate)
    
    ax7.plot(rms_times, left_rms, 'b-', label='Left Channel')
    ax7.plot(rms_times, right_rms, 'r-', label='Right Channel')
    ax7.set_xlabel('Time (s)')
    ax7.set_ylabel('RMS')
    ax7.set_title('RMS Over Time')
    ax7.legend()
    ax7.grid(True, alpha=0.3)
    
    # 8. Correlation between channels
    ax8 = plt.subplot(4, 2, 8)
    correlation = np.corrcoef(left_data, right_data)[0, 1]
    ax8.scatter(left_data[::100], right_data[::100], alpha=0.5, s=1)
    ax8.set_xlabel('Left Channel')
    ax8.set_ylabel('Right Channel')
    ax8.set_title(f'Channel Correlation: {correlation:.4f}')
    ax8.grid(True, alpha=0.3)
    
    # Add a diagonal line for perfect correlation
    min_val = min(np.min(left_data), np.min(right_data))
    max_val = max(np.max(left_data), np.max(right_data))
    ax8.plot([min_val, max_val], [min_val, max_val], 'k--', alpha=0.5)
    
    plt.tight_layout()
    return fig, left_glitches, right_glitches

def print_analysis_summary(left_data, right_data, left_glitches, right_glitches, sample_rate=44100):
    """Print a summary of the interpolated audio analysis."""
    print("\n" + "="*60)
    print("INTERPOLATED AUDIO ANALYSIS SUMMARY")
    print("="*60)
    
    # Basic statistics
    print(f"Duration: {len(left_data) / sample_rate:.2f} seconds")
    print(f"Sample rate: {sample_rate} Hz")
    print(f"Total samples per channel: {len(left_data)}")
    print(f"Interleaved format: L0, R0, L1, R1, L2, R2, ...")
    
    # Amplitude analysis
    left_max = np.max(np.abs(left_data))
    right_max = np.max(np.abs(right_data))
    left_rms = calculate_rms(left_data)
    right_rms = calculate_rms(right_data)
    left_crest = calculate_crest_factor(left_data)
    right_crest = calculate_crest_factor(right_data)
    
    print(f"\nAmplitude Analysis:")
    print(f"  Left Channel - Max: {left_max:.4f}, RMS: {left_rms:.4f}, Crest Factor: {left_crest:.2f}")
    print(f"  Right Channel - Max: {right_max:.4f}, RMS: {right_rms:.4f}, Crest Factor: {right_crest:.2f}")
    
    # Frequency analysis
    _, _, peak_freq_left, _ = analyze_frequency(left_data, sample_rate)
    _, _, peak_freq_right, _ = analyze_frequency(right_data, sample_rate)
    
    print(f"\nFrequency Analysis:")
    print(f"  Left Channel - Peak Frequency: {peak_freq_left:.1f} Hz")
    print(f"  Right Channel - Peak Frequency: {peak_freq_right:.1f} Hz")
    
    # Glitch analysis
    print(f"\nGlitch Analysis:")
    print(f"  Left Channel - {len(left_glitches)} glitches detected")
    print(f"  Right Channel - {len(right_glitches)} glitches detected")
    
    if left_glitches:
        max_jump_left = max(g['amplitude_jump'] for g in left_glitches)
        print(f"  Left Channel - Max amplitude jump: {max_jump_left:.4f}")
    
    if right_glitches:
        max_jump_right = max(g['amplitude_jump'] for g in right_glitches)
        print(f"  Right Channel - Max amplitude jump: {max_jump_right:.4f}")
    
    # Channel correlation
    correlation = np.corrcoef(left_data, right_data)[0, 1]
    print(f"\nChannel Correlation: {correlation:.4f}")
    
    # DC offset
    left_dc = np.mean(left_data)
    right_dc = np.mean(right_data)
    print(f"\nDC Offset:")
    print(f"  Left Channel: {left_dc:.6f}")
    print(f"  Right Channel: {right_dc:.6f}")
    
    print("="*60)

def main():
    """Main function to run the interpolated audio analysis."""
    
    # Check if file exists
    audio_file = "playground/recorded_audio.raw"
    
    if not os.path.exists(audio_file):
        print("Error: Interpolated audio file not found!")
        print("Please run the second C++ test case first to generate the interpolated audio file.")
        print("Expected file:")
        print(f"  {audio_file}")
        sys.exit(1)
    
    # Load interpolated audio data
    print("Loading interpolated audio data...")
    left_data, right_data = load_interpolated_audio(audio_file, num_channels=2, max_duration=1.0)
    
    if left_data is None or right_data is None:
        print("Error: Failed to load interpolated audio data!")
        sys.exit(1)
    
    # Ensure both channels have the same length
    min_length = min(len(left_data), len(right_data))
    left_data = left_data[:min_length]
    right_data = right_data[:min_length]
    
    print(f"Loaded {min_length} samples from both channels")
    
    # Enhanced glitch analysis
    print("\n" + "="*60)
    print("ENHANCED GLITCH ANALYSIS")
    print("="*60)
    
    # Use enhanced glitch detection
    left_glitches, left_boundaries = detect_glitches_enhanced(left_data, buffer_size=512)
    right_glitches, right_boundaries = detect_glitches_enhanced(right_data, buffer_size=512)
    
    # Analyze buffer boundaries
    left_boundary_issues = analyze_buffer_boundaries(left_data, buffer_size=512)
    right_boundary_issues = analyze_buffer_boundaries(right_data, buffer_size=512)
    
    # Analyze phase continuity
    left_phase_jumps = analyze_phase_continuity(left_data, expected_frequency=440.0)
    right_phase_jumps = analyze_phase_continuity(right_data, expected_frequency=440.0)
    
    # Detailed glitch analysis
    print(f"\nGlitch Summary:")
    print(f"  Left Channel: {len(left_glitches)} glitches detected")
    print(f"  Right Channel: {len(right_glitches)} glitches detected")
    
    if right_glitches:
        print(f"\nRight Channel Glitch Details:")
        for i, glitch in enumerate(right_glitches):
            print(f"  Glitch {i+1}:")
            print(f"    Time: {glitch['time']:.3f}s")
            print(f"    Amplitude Jump: {glitch['amplitude_jump']:.4f}")
            print(f"    Sample Before: {glitch['sample_before']:.4f}")
            print(f"    Sample After: {glitch['sample_after']:.4f}")
            print(f"    Frame Number: {glitch['frame_number']}")
            print(f"    Position in Frame: {glitch['position_in_frame']}")
            print(f"    Is Frame Boundary: {glitch['is_frame_boundary']}")
            print(f"    Frame Time: {glitch['frame_time']:.3f}s")
    
    # Buffer boundary analysis
    print(f"\nBuffer Boundary Analysis:")
    print(f"  Left Channel: {len(left_boundary_issues)} boundary issues")
    print(f"  Right Channel: {len(right_boundary_issues)} boundary issues")
    
    if right_boundary_issues:
        print(f"\nRight Channel Boundary Issues:")
        for issue in right_boundary_issues[:5]:  # Show first 5
            print(f"  Frame {issue['frame']} at {issue['time']:.3f}s: discontinuity = {issue['discontinuity']:.4f}")
    
    # Phase continuity analysis
    print(f"\nPhase Continuity Analysis:")
    print(f"  Left Channel: {len(left_phase_jumps)} phase jumps")
    print(f"  Right Channel: {len(right_phase_jumps)} phase jumps")
    
    if right_phase_jumps:
        print(f"\nRight Channel Phase Jumps:")
        for jump in right_phase_jumps[:3]:  # Show first 3
            print(f"  Time {jump['time']:.3f}s: phase jump = {jump['phase_jump']:.2f} radians")
    
    # Create analysis plots
    print("\nCreating analysis plots...")
    fig, left_glitches_old, right_glitches_old = plot_interpolated_audio_analysis(left_data, right_data)
    
    # Print analysis summary
    print_analysis_summary(left_data, right_data, left_glitches_old, right_glitches_old)
    
    # Save the plot
    output_file = "playground/interpolated_audio_analysis.png"
    fig.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\nAnalysis plot saved to: {output_file}")
    
    # Show the plot
    plt.show()
    
    # Save detailed glitch information
    if left_glitches or right_glitches:
        with open("playground/enhanced_glitch_report.txt", "w") as f:
            f.write("ENHANCED GLITCH DETECTION REPORT\n")
            f.write("="*50 + "\n\n")
            
            if left_glitches:
                f.write(f"LEFT CHANNEL GLITCHES ({len(left_glitches)} found):\n")
                f.write("-" * 30 + "\n")
                for i, glitch in enumerate(left_glitches):
                    f.write(f"Glitch {i+1}: Time={glitch['time']:.3f}s, "
                           f"Jump={glitch['amplitude_jump']:.4f}, "
                           f"Frame={glitch['frame_number']}, "
                           f"Pos={glitch['position_in_frame']}, "
                           f"Boundary={glitch['is_frame_boundary']}\n")
                f.write("\n")
            
            if right_glitches:
                f.write(f"RIGHT CHANNEL GLITCHES ({len(right_glitches)} found):\n")
                f.write("-" * 30 + "\n")
                for i, glitch in enumerate(right_glitches):
                    f.write(f"Glitch {i+1}: Time={glitch['time']:.3f}s, "
                           f"Jump={glitch['amplitude_jump']:.4f}, "
                           f"Frame={glitch['frame_number']}, "
                           f"Pos={glitch['position_in_frame']}, "
                           f"Boundary={glitch['is_frame_boundary']}\n")
                f.write("\n")
            
            # Add buffer boundary analysis
            if left_boundary_issues or right_boundary_issues:
                f.write("BUFFER BOUNDARY ANALYSIS:\n")
                f.write("-" * 30 + "\n")
                if left_boundary_issues:
                    f.write(f"Left Channel: {len(left_boundary_issues)} boundary issues\n")
                if right_boundary_issues:
                    f.write(f"Right Channel: {len(right_boundary_issues)} boundary issues\n")
                f.write("\n")
        
        print(f"Enhanced glitch report saved to: playground/enhanced_glitch_report.txt")
    
    # Provide recommendations
    print("\n" + "="*60)
    print("RECOMMENDATIONS")
    print("="*60)
    
    if right_glitches:
        print("1. **Major Glitch Detected**: There's a significant glitch in the right channel")
        print("   - This could be caused by:")
        print("     * Phase discontinuity in the sine wave generation")
        print("     * Buffer boundary issues in the render pipeline")
        print("     * Interpolation artifacts in the final render stage")
        print("     * Frame synchronization issues")
        
        print("\n2. **Investigation Steps**:")
        print("   - Check the sine wave phase calculation in the shader")
        print("   - Verify frame-to-frame continuity in the render stages")
        print("   - Examine the interpolation logic in AudioFinalRenderStage")
        print("   - Check if global_time parameter is being updated correctly")
        
        print("\n3. **Potential Fixes**:")
        print("   - Ensure phase continuity across frame boundaries")
        print("   - Add proper interpolation between frames")
        print("   - Check for integer overflow in phase calculations")
        print("   - Verify buffer alignment and memory access patterns")
    
    print("="*60)

if __name__ == "__main__":
    main() 