#!/usr/bin/env python3
"""
Simple script to output every single audio sample value for discontinuity analysis.
This script reads the CSV file and prints every sample value so you can see
discontinuities directly in the raw data.
"""

import pandas as pd
import numpy as np
import argparse
import os

def print_all_samples(csv_file, channel='both', max_samples=None):
    """Print every single audio sample value."""
    if not os.path.exists(csv_file):
        print(f"Error: CSV file '{csv_file}' not found.")
        return
    
    # Load the CSV data
    df = pd.read_csv(csv_file)
    print(f"Loaded {len(df)} audio frames from {csv_file}")
    print(f"Columns: {list(df.columns)}")
    print()
    
    # Determine which channels to print
    channels_to_print = []
    if channel in ['both', 'left', 'l']:
        channels_to_print.append(('left_channel', 'L'))
    if channel in ['both', 'right', 'r']:
        channels_to_print.append(('right_channel', 'R'))
    
    # Limit number of samples if specified
    total_samples = len(df)
    if max_samples and max_samples < total_samples:
        df = df.head(max_samples)
        print(f"Printing first {max_samples} samples (out of {total_samples} total)")
    else:
        print(f"Printing all {total_samples} samples")
    
    print()
    print("=" * 80)
    print("RAW AUDIO SAMPLE VALUES")
    print("=" * 80)
    print("Format: Frame | Time(s) | Left_Channel | Right_Channel | Difference")
    print("=" * 80)
    
    # Print header
    if len(channels_to_print) == 2:
        print(f"{'Frame':>6} | {'Time(s)':>8} | {'Left':>12} | {'Right':>12} | {'Diff':>12}")
    else:
        channel_name = channels_to_print[0][1]
        print(f"{'Frame':>6} | {'Time(s)':>8} | {channel_name:>12}")
    
    print("-" * 80)
    
    # Print each sample
    for i, row in df.iterrows():
        frame = int(row['frame'])
        time_sec = frame / 44100.0  # Assuming 44.1kHz sample rate
        
        if len(channels_to_print) == 2:
            left_val = row['left_channel']
            right_val = row['right_channel']
            diff = abs(left_val - right_val)
            print(f"{frame:>6} | {time_sec:>8.6f} | {left_val:>12.8f} | {right_val:>12.8f} | {diff:>12.8f}")
        else:
            channel_col, channel_label = channels_to_print[0]
            val = row[channel_col]
            print(f"{frame:>6} | {time_sec:>8.6f} | {val:>12.8f}")
    
    print("=" * 80)
    print(f"Printed {len(df)} samples")

def detect_large_jumps(csv_file, threshold=0.1, channel='both'):
    """Detect and highlight large jumps in the audio signal."""
    if not os.path.exists(csv_file):
        print(f"Error: CSV file '{csv_file}' not found.")
        return
    
    df = pd.read_csv(csv_file)
    print(f"\nAnalyzing {len(df)} samples for discontinuities...")
    print(f"Jump threshold: {threshold}")
    print()
    
    # Determine which channels to analyze
    channels_to_analyze = []
    if channel in ['both', 'left', 'l']:
        channels_to_analyze.append(('left_channel', 'Left'))
    if channel in ['both', 'right', 'r']:
        channels_to_analyze.append(('right_channel', 'Right'))
    
    print("=" * 100)
    print("DISCONTINUITY DETECTION")
    print("=" * 100)
    print(f"{'Frame':>6} | {'Time(s)':>8} | {'Channel':>8} | {'Before':>12} | {'After':>12} | {'Jump':>12}")
    print("-" * 100)
    
    discontinuity_count = 0
    
    for channel_col, channel_name in channels_to_analyze:
        values = df[channel_col].values
        
        for i in range(1, len(values)):
            prev_val = values[i-1]
            curr_val = values[i]
            jump = abs(curr_val - prev_val)
            
            if jump > threshold:
                frame = int(df.iloc[i]['frame'])
                time_sec = frame / 44100.0
                
                print(f"{frame:>6} | {time_sec:>8.6f} | {channel_name:>8} | {prev_val:>12.8f} | {curr_val:>12.8f} | {jump:>12.8f}")
                discontinuity_count += 1
    
    print("=" * 100)
    print(f"Found {discontinuity_count} discontinuities above threshold {threshold}")

def main():
    parser = argparse.ArgumentParser(description='Analyze audio discontinuities by printing all sample values')
    parser.add_argument('csv_file', help='Path to the CSV file containing audio data')
    parser.add_argument('--channel', choices=['left', 'right', 'both', 'l', 'r'], default='both',
                       help='Which channel to analyze (default: both)')
    parser.add_argument('--max-samples', type=int, 
                       help='Maximum number of samples to print (default: all)')
    parser.add_argument('--threshold', type=float, default=0.1,
                       help='Threshold for discontinuity detection (default: 0.1)')
    parser.add_argument('--detect-only', action='store_true',
                       help='Only detect discontinuities, do not print all samples')
    
    args = parser.parse_args()
    
    if args.detect_only:
        detect_large_jumps(args.csv_file, args.threshold, args.channel)
    else:
        print_all_samples(args.csv_file, args.channel, args.max_samples)
        print()
        detect_large_jumps(args.csv_file, args.threshold, args.channel)

if __name__ == "__main__":
    main()
