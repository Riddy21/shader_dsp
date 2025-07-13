#!/usr/bin/env python3
"""
Audio Generator Test Visualization Script

This script reads the audio output from the audio generator test and creates
various visualizations including waveform plots, frequency analysis, and
envelope detection.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.gridspec import GridSpec
import os
import sys

def read_audio_data(filename):
    """Read audio data from the test output file."""
    if not os.path.exists(filename):
        print(f"Error: File {filename} not found!")
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
    
    samples = np.array(samples)
    time_axis = np.arange(len(samples)) / sample_rate
    
    return samples, time_axis, sample_rate, test_frequency, test_gain, buffer_size

def analyze_frequency(samples, sample_rate):
    """Analyze the frequency content of the audio."""
    # Perform FFT
    fft_result = np.fft.fft(samples)
    fft_freqs = np.fft.fftfreq(len(samples), 1/sample_rate)
    
    # Get positive frequencies only
    positive_freqs = fft_freqs[:len(fft_freqs)//2]
    positive_fft = np.abs(fft_result[:len(fft_result)//2])
    
    return positive_freqs, positive_fft

def detect_zero_crossings(samples):
    """Detect zero crossings in the waveform."""
    zero_crossings = []
    for i in range(1, len(samples)):
        if (samples[i-1] < 0 and samples[i] >= 0) or (samples[i-1] > 0 and samples[i] <= 0):
            zero_crossings.append(i)
    return zero_crossings

def analyze_envelope(samples, sample_rate, buffer_size):
    """Analyze the ADSR envelope of the audio."""
    # Calculate RMS envelope
    window_size = min(buffer_size // 4, 64)
    envelope = []
    for i in range(0, len(samples), window_size):
        window = samples[i:i+window_size]
        if len(window) > 0:
            rms = np.sqrt(np.mean(window**2))
            envelope.append(rms)
    
    envelope_time = np.arange(len(envelope)) * window_size / sample_rate
    return envelope, envelope_time

def create_visualization(samples, time_axis, sample_rate, test_frequency, test_gain, buffer_size):
    """Create comprehensive visualization of the audio data."""
    
    # Create figure with subplots
    fig = plt.figure(figsize=(15, 12))
    gs = GridSpec(4, 2, figure=fig, height_ratios=[2, 1, 1, 1])
    
    # 1. Main waveform plot
    ax1 = fig.add_subplot(gs[0, :])
    ax1.plot(time_axis, samples, 'b-', linewidth=0.5, alpha=0.8)
    ax1.set_title(f'Audio Generator Test Output\nFrequency: {test_frequency} Hz, Gain: {test_gain}', fontsize=14)
    ax1.set_xlabel('Time (seconds)')
    ax1.set_ylabel('Amplitude')
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(-test_gain*1.2, test_gain*1.2)
    
    # Mark frame boundaries
    for i in range(0, len(samples), buffer_size):
        if i < len(time_axis):
            ax1.axvline(x=time_axis[i], color='r', linestyle='--', alpha=0.5, linewidth=0.5)
    
    # 2. Frequency analysis
    freqs, fft_magnitude = analyze_frequency(samples, sample_rate)
    ax2 = fig.add_subplot(gs[1, 0])
    ax2.plot(freqs, fft_magnitude, 'g-', linewidth=1)
    ax2.set_title('Frequency Spectrum')
    ax2.set_xlabel('Frequency (Hz)')
    ax2.set_ylabel('Magnitude')
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim(0, test_frequency * 3)
    
    # Mark expected frequency
    ax2.axvline(x=test_frequency, color='r', linestyle='--', alpha=0.7, label=f'Expected: {test_frequency} Hz')
    ax2.legend()
    
    # 3. Zero crossings analysis
    zero_crossings = detect_zero_crossings(samples)
    ax3 = fig.add_subplot(gs[1, 1])
    
    # Plot waveform with zero crossings marked
    ax3.plot(time_axis, samples, 'b-', linewidth=0.5, alpha=0.8)
    if zero_crossings:
        zero_crossing_times = np.array(zero_crossings) / sample_rate
        ax3.plot(zero_crossing_times, np.zeros_like(zero_crossing_times), 'ro', markersize=3, alpha=0.7)
    
    ax3.set_title(f'Zero Crossings Analysis ({len(zero_crossings)} detected)')
    ax3.set_xlabel('Time (seconds)')
    ax3.set_ylabel('Amplitude')
    ax3.grid(True, alpha=0.3)
    ax3.axhline(y=0, color='k', linestyle='-', alpha=0.5)
    
    # 4. Envelope analysis
    envelope, envelope_time = analyze_envelope(samples, sample_rate, buffer_size)
    ax4 = fig.add_subplot(gs[2, :])
    ax4.plot(envelope_time, envelope, 'm-', linewidth=2)
    ax4.set_title('RMS Envelope Analysis')
    ax4.set_xlabel('Time (seconds)')
    ax4.set_ylabel('RMS Amplitude')
    ax4.grid(True, alpha=0.3)
    
    # 5. Statistical analysis
    ax5 = fig.add_subplot(gs[3, 0])
    
    # Histogram of amplitudes
    ax5.hist(samples, bins=50, alpha=0.7, color='skyblue', edgecolor='black')
    ax5.set_title('Amplitude Distribution')
    ax5.set_xlabel('Amplitude')
    ax5.set_ylabel('Frequency')
    ax5.grid(True, alpha=0.3)
    
    # 6. Quality metrics
    ax6 = fig.add_subplot(gs[3, 1])
    ax6.axis('off')
    
    # Calculate metrics
    dc_offset = np.mean(samples)
    rms_level = np.sqrt(np.mean(samples**2))
    peak_level = np.max(np.abs(samples))
    dynamic_range = 20 * np.log10(peak_level / (np.std(samples) + 1e-10))
    
    # Calculate measured frequency from zero crossings
    if len(zero_crossings) >= 2:
        total_time = len(samples) / sample_rate
        measured_freq = (len(zero_crossings) - 1) / (2.0 * total_time)
        freq_error = abs(measured_freq - test_frequency) / test_frequency * 100
    else:
        measured_freq = 0
        freq_error = 100
    
    # Display metrics
    metrics_text = f"""Quality Metrics:
    
DC Offset: {dc_offset:.6f}
RMS Level: {rms_level:.6f}
Peak Level: {peak_level:.6f}
Dynamic Range: {dynamic_range:.1f} dB

Frequency Analysis:
Expected: {test_frequency:.1f} Hz
Measured: {measured_freq:.1f} Hz
Error: {freq_error:.1f}%

Zero Crossings: {len(zero_crossings)}
Total Samples: {len(samples)}
Duration: {time_axis[-1]:.3f}s"""
    
    ax6.text(0.1, 0.9, metrics_text, transform=ax6.transAxes, fontsize=10,
             verticalalignment='top', fontfamily='monospace',
             bbox=dict(boxstyle="round,pad=0.3", facecolor="lightgray", alpha=0.8))
    
    plt.tight_layout()
    return fig

def main():
    """Main function to run the visualization."""
    audio_file = "playground/audio_output.txt"
    
    print("Audio Generator Test Visualization")
    print("=" * 40)
    
    # Read audio data
    result = read_audio_data(audio_file)
    if result[0] is None:
        print("Failed to read audio data. Make sure to run the test first.")
        return
    
    samples, time_axis, sample_rate, test_frequency, test_gain, buffer_size = result
    
    print(f"Loaded {len(samples)} samples")
    print(f"Sample rate: {sample_rate} Hz")
    print(f"Duration: {time_axis[-1]:.3f} seconds")
    print(f"Test frequency: {test_frequency} Hz")
    print(f"Test gain: {test_gain}")
    print()
    
    # Create visualization
    fig = create_visualization(samples, time_axis, sample_rate, test_frequency, test_gain, buffer_size)
    
    # Save the plot
    output_file = "playground/audio_visualization.png"
    fig.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Visualization saved to: {output_file}")
    
    # Show the plot
    plt.show()

if __name__ == "__main__":
    main() 