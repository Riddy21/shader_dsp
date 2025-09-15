#!/usr/bin/env python3
"""
Simple Audio Visualization Script

This script reads the audio_output.txt file and creates visualizations using matplotlib.
No make dependencies - just pure Python plotting.
"""

import numpy as np
import matplotlib.pyplot as plt
import os
import sys

def read_audio_data(filename):
    """Read audio data from the test output file."""
    if not os.path.exists(filename):
        print(f"Error: File {filename} not found!")
        print("Please run the audio generator test first to generate audio_output.txt")
        return None, None, None, None, None, None
    
    samples = []
    sample_rate = 44100
    buffer_size = 256
    num_frames = 10
    test_frequency = 440.0
    test_gain = 0.5
    
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('#'):
                # Parse metadata
                if 'Sample Rate:' in line:
                    sample_rate = int(line.split(':')[1].strip())
                elif 'Buffer Size:' in line:
                    buffer_size = int(line.split(':')[1].strip())
                elif 'Num Frames:' in line:
                    num_frames = int(line.split(':')[1].strip())
                elif 'Test Frequency:' in line:
                    test_frequency = float(line.split(':')[1].strip())
                elif 'Test Gain:' in line:
                    test_gain = float(line.split(':')[1].strip())
                continue
            
            if ',' in line:
                parts = line.split(',')
                if len(parts) == 2:
                    try:
                        sample_index = int(parts[0])
                        amplitude = float(parts[1])
                        samples.append(amplitude)
                    except ValueError:
                        continue
    
    if not samples:
        print("Error: No audio samples found in the file!")
        return None, None, None, None, None, None
    
    samples = np.array(samples)
    time_axis = np.arange(len(samples)) / sample_rate
    
    return samples, time_axis, sample_rate, test_frequency, test_gain, buffer_size

def create_simple_visualization(samples, time_axis, sample_rate, test_frequency, test_gain):
    """Create a simple but comprehensive visualization."""
    
    # Create figure with subplots
    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    fig.suptitle(f'Audio Generator Test Results\nFrequency: {test_frequency} Hz, Gain: {test_gain}', fontsize=14)
    
    # 1. Waveform plot
    axes[0, 0].plot(time_axis, samples, 'b-', linewidth=0.5, alpha=0.8)
    axes[0, 0].set_title('Audio Waveform')
    axes[0, 0].set_xlabel('Time (seconds)')
    axes[0, 0].set_ylabel('Amplitude')
    axes[0, 0].grid(True, alpha=0.3)
    axes[0, 0].set_ylim(-test_gain*1.2, test_gain*1.2)
    
    # 2. Frequency spectrum
    fft_result = np.fft.fft(samples)
    fft_freqs = np.fft.fftfreq(len(samples), 1/sample_rate)
    positive_freqs = fft_freqs[:len(fft_freqs)//2]
    positive_fft = np.abs(fft_result[:len(fft_result)//2])
    
    axes[0, 1].plot(positive_freqs, positive_fft, 'g-', linewidth=1)
    axes[0, 1].set_title('Frequency Spectrum')
    axes[0, 1].set_xlabel('Frequency (Hz)')
    axes[0, 1].set_ylabel('Magnitude')
    axes[0, 1].grid(True, alpha=0.3)
    axes[0, 1].set_xlim(0, test_frequency * 3)
    axes[0, 1].axvline(x=test_frequency, color='r', linestyle='--', alpha=0.7, label=f'Expected: {test_frequency} Hz')
    axes[0, 1].legend()
    
    # 3. Zero crossings
    zero_crossings = []
    for i in range(1, len(samples)):
        if (samples[i-1] < 0 and samples[i] >= 0) or (samples[i-1] > 0 and samples[i] <= 0):
            zero_crossings.append(i)
    
    axes[1, 0].plot(time_axis, samples, 'b-', linewidth=0.5, alpha=0.8)
    if zero_crossings:
        zero_crossing_times = np.array(zero_crossings) / sample_rate
        axes[1, 0].plot(zero_crossing_times, np.zeros_like(zero_crossing_times), 'ro', markersize=3, alpha=0.7)
    axes[1, 0].set_title(f'Zero Crossings ({len(zero_crossings)} detected)')
    axes[1, 0].set_xlabel('Time (seconds)')
    axes[1, 0].set_ylabel('Amplitude')
    axes[1, 0].grid(True, alpha=0.3)
    axes[1, 0].axhline(y=0, color='k', linestyle='-', alpha=0.5)
    
    # 4. Statistics and metrics
    axes[1, 1].axis('off')
    
    # Calculate metrics
    dc_offset = np.mean(samples)
    rms_level = np.sqrt(np.mean(samples**2))
    peak_level = np.max(np.abs(samples))
    
    # Calculate measured frequency from zero crossings
    if len(zero_crossings) >= 2:
        total_time = len(samples) / sample_rate
        measured_freq = (len(zero_crossings) - 1) / (2.0 * total_time)
        freq_error = abs(measured_freq - test_frequency) / test_frequency * 100
    else:
        measured_freq = 0
        freq_error = 100
    
    metrics_text = f"""Audio Analysis Results:

Basic Metrics:
• DC Offset: {dc_offset:.6f}
• RMS Level: {rms_level:.6f}
• Peak Level: {peak_level:.6f}

Frequency Analysis:
• Expected: {test_frequency:.1f} Hz
• Measured: {measured_freq:.1f} Hz
• Error: {freq_error:.1f}%

Waveform Info:
• Total Samples: {len(samples)}
• Duration: {time_axis[-1]:.3f}s
• Zero Crossings: {len(zero_crossings)}"""
    
    axes[1, 1].text(0.1, 0.9, metrics_text, transform=axes[1, 1].transAxes, fontsize=10,
                    verticalalignment='top', fontfamily='monospace',
                    bbox=dict(boxstyle="round,pad=0.3", facecolor="lightgray", alpha=0.8))
    
    plt.tight_layout()
    return fig

def main():
    """Main function to read audio data and create visualization."""
    audio_file = "audio_output.txt"
    
    print("Audio Generator Test Visualization")
    print("=" * 40)
    
    # Read audio data
    result = read_audio_data(audio_file)
    if result[0] is None:
        print("Failed to read audio data.")
        print("Make sure audio_output.txt exists in the current directory.")
        return
    
    samples, time_axis, sample_rate, test_frequency, test_gain, buffer_size = result
    
    print(f"✅ Loaded {len(samples)} samples")
    print(f"✅ Sample rate: {sample_rate} Hz")
    print(f"✅ Duration: {time_axis[-1]:.3f} seconds")
    print(f"✅ Test frequency: {test_frequency} Hz")
    print(f"✅ Test gain: {test_gain}")
    print()
    
    # Create visualization
    fig = create_simple_visualization(samples, time_axis, sample_rate, test_frequency, test_gain)
    
    # Save the plot
    output_file = "audio_visualization.png"
    fig.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✅ Visualization saved to: {output_file}")
    
    # Show the plot
    plt.show()

if __name__ == "__main__":
    main() 