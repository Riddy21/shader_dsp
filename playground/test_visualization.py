#!/usr/bin/env python3
"""
Test script to verify the audio visualization setup.
This script creates dummy audio data to test the visualization functions.
"""

import numpy as np
import matplotlib.pyplot as plt
import os

def create_test_audio_data():
    """Create test audio data to verify the visualization works."""
    
    # Test parameters
    sample_rate = 44100
    duration = 2.0  # 2 seconds
    frequency = 441.0  # Hz
    amplitude = 0.5
    
    # Generate time array
    t = np.linspace(0, duration, int(sample_rate * duration), endpoint=False)
    
    # Generate clean sine wave
    left_channel = amplitude * np.sin(2 * np.pi * frequency * t)
    right_channel = amplitude * np.sin(2 * np.pi * frequency * t)  # Same as left for test
    
    # Add some test glitches
    glitch_positions = [int(sample_rate * 0.5), int(sample_rate * 1.0), int(sample_rate * 1.5)]
    for pos in glitch_positions:
        if pos < len(left_channel):
            left_channel[pos] += 0.2  # Add a glitch
            right_channel[pos] += 0.15  # Add a smaller glitch
    
    return left_channel, right_channel, sample_rate

def save_test_data(left_data, right_data):
    """Save test data in the same format as the C++ test."""
    
    # Create playground directory if it doesn't exist
    os.makedirs("playground", exist_ok=True)
    
    # Save as binary files
    left_data.astype(np.float32).tofile("playground/left_channel.raw")
    right_data.astype(np.float32).tofile("playground/right_channel.raw")
    
    # Save as text files
    np.savetxt("playground/left_channel.txt", left_data)
    np.savetxt("playground/right_channel.txt", right_data)
    
    print("Test audio data saved to playground/")

def main():
    """Main test function."""
    print("Creating test audio data...")
    
    # Generate test data
    left_data, right_data, sample_rate = create_test_audio_data()
    
    # Save test data
    save_test_data(left_data, right_data)
    
    print(f"Generated {len(left_data)} samples at {sample_rate} Hz")
    print("Test data includes:")
    print("- Clean 441 Hz sine wave")
    print("- Test glitches at 0.5s, 1.0s, and 1.5s")
    print("- Amplitude: 0.5")
    
    print("\nYou can now run: python visualize_audio.py")
    print("This should detect the test glitches and show the sine wave analysis.")

if __name__ == "__main__":
    main() 