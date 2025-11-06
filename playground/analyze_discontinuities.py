#!/usr/bin/env python3
"""
Simple script to output every single audio sample value for discontinuity analysis.
This script reads the CSV file and prints every sample value so you can see
discontinuities directly in the raw data. Also provides graphical visualization.
"""

import pandas as pd
import numpy as np
import argparse
import os

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("Warning: matplotlib not available. Graphical features disabled.")

def print_all_samples(csv_file, channel='all', max_samples=None):
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
    # Support multi-channel by auto-detecting channel_0, channel_1, channel_2, etc.
    channels_to_print = []
    
    if channel == 'all' or channel == 'multi':
        # Auto-detect all channels (channel_0, channel_1, channel_2, ...)
        ch_idx = 0
        while f'channel_{ch_idx}' in df.columns:
            channels_to_print.append((f'channel_{ch_idx}', f'Ch{ch_idx}'))
            ch_idx += 1
        
        # Fallback to legacy formats if no channel_N columns found
        if not channels_to_print:
            if 'left_channel' in df.columns:
                channels_to_print.append(('left_channel', 'L'))
            if 'right_channel' in df.columns:
                channels_to_print.append(('right_channel', 'R'))
            if 'amplitude' in df.columns and not channels_to_print:
                channels_to_print.append(('amplitude', 'Amp'))
    elif channel in ['both', 'left', 'l']:
        if 'left_channel' in df.columns:
            channels_to_print.append(('left_channel', 'L'))
        elif 'channel_0' in df.columns:
            channels_to_print.append(('channel_0', 'Ch0'))
        elif 'amplitude' in df.columns:
            channels_to_print.append(('amplitude', 'Amp'))
    elif channel in ['both', 'right', 'r']:
        if 'right_channel' in df.columns:
            channels_to_print.append(('right_channel', 'R'))
        elif 'channel_1' in df.columns:
            channels_to_print.append(('channel_1', 'Ch1'))
    else:
        # Try to parse channel number (e.g., 'channel_2' or '2')
        try:
            ch_num = int(channel)
            ch_col = f'channel_{ch_num}'
            if ch_col in df.columns:
                channels_to_print.append((ch_col, f'Ch{ch_num}'))
        except ValueError:
            pass
    
    # Limit number of samples if specified
    total_samples = len(df)
    if max_samples and max_samples < total_samples:
        df = df.head(max_samples)
        print(f"Printing first {max_samples} samples (out of {total_samples} total)")
    else:
        print(f"Printing all {total_samples} samples")
    
    print()
    print("=" * 100)
    print("RAW AUDIO SAMPLE VALUES")
    print("=" * 100)
    
    # Build header dynamically for multi-channel
    header_parts = [f"{'Frame':>6}", f"{'Time(s)':>8}"]
    for _, label in channels_to_print:
        header_parts.append(f"{label:>12}")
    if len(channels_to_print) == 2:
        header_parts.append(f"{'Diff':>12}")
    header_line = " | ".join(header_parts)
    
    print(header_line)
    print("-" * len(header_line))
    
    # Check for frame or sample_index column (handle both formats)
    frame_col = 'frame' if 'frame' in df.columns else 'sample_index'
    if frame_col not in df.columns:
        print(f"Error: CSV file must contain either 'frame' or 'sample_index' column")
        print(f"Available columns: {list(df.columns)}")
        return
    
    # Print each sample
    for i, row in df.iterrows():
        frame = int(row[frame_col])
        time_sec = row['time_seconds'] if 'time_seconds' in df.columns else frame / 44100.0  # Use time_seconds if available
        
        row_parts = [f"{frame:>6}", f"{time_sec:>8.6f}"]
        values = []
        for channel_col, _ in channels_to_print:
            val = row[channel_col]
            values.append(val)
            row_parts.append(f"{val:>12.8f}")
        
        # Add difference for 2-channel case
        if len(channels_to_print) == 2:
            diff = abs(values[0] - values[1])
            row_parts.append(f"{diff:>12.8f}")
        
        print(" | ".join(row_parts))
    
    print("=" * len(header_line))
    print(f"Printed {len(df)} samples")

def detect_large_jumps(csv_file, threshold=0.1, channel='all'):
    """Detect and highlight large jumps in the audio signal."""
    if not os.path.exists(csv_file):
        print(f"Error: CSV file '{csv_file}' not found.")
        return
    
    df = pd.read_csv(csv_file)
    print(f"\nAnalyzing {len(df)} samples for discontinuities...")
    print(f"Jump threshold: {threshold}")
    print()
    
    # Determine which channels to analyze
    # Support multi-channel by auto-detecting channel_0, channel_1, channel_2, etc.
    channels_to_analyze = []
    
    if channel == 'all' or channel == 'multi':
        # Auto-detect all channels (channel_0, channel_1, channel_2, ...)
        ch_idx = 0
        while f'channel_{ch_idx}' in df.columns:
            channels_to_analyze.append((f'channel_{ch_idx}', f'Channel {ch_idx}'))
            ch_idx += 1
        
        # Fallback to legacy formats if no channel_N columns found
        if not channels_to_analyze:
            if 'left_channel' in df.columns:
                channels_to_analyze.append(('left_channel', 'Left'))
            if 'right_channel' in df.columns:
                channels_to_analyze.append(('right_channel', 'Right'))
            if 'amplitude' in df.columns and not channels_to_analyze:
                channels_to_analyze.append(('amplitude', 'Amplitude'))
    elif channel in ['both', 'left', 'l']:
        if 'left_channel' in df.columns:
            channels_to_analyze.append(('left_channel', 'Left'))
        elif 'channel_0' in df.columns:
            channels_to_analyze.append(('channel_0', 'Channel 0'))
        elif 'amplitude' in df.columns:
            channels_to_analyze.append(('amplitude', 'Amplitude'))
    elif channel in ['both', 'right', 'r']:
        if 'right_channel' in df.columns:
            channels_to_analyze.append(('right_channel', 'Right'))
        elif 'channel_1' in df.columns:
            channels_to_analyze.append(('channel_1', 'Channel 1'))
    else:
        # Try to parse channel number (e.g., 'channel_2' or '2')
        try:
            ch_num = int(channel)
            ch_col = f'channel_{ch_num}'
            if ch_col in df.columns:
                channels_to_analyze.append((ch_col, f'Channel {ch_num}'))
        except ValueError:
            pass
    
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
                # Check for frame or sample_index column (handle both formats)
                frame_col = 'frame' if 'frame' in df.columns else 'sample_index'
                frame = int(df.iloc[i][frame_col])
                time_sec = df.iloc[i]['time_seconds'] if 'time_seconds' in df.columns else frame / 44100.0
                
                print(f"{frame:>6} | {time_sec:>8.6f} | {channel_name:>8} | {prev_val:>12.8f} | {curr_val:>12.8f} | {jump:>12.8f}")
                discontinuity_count += 1
    
    print("=" * 100)
    print(f"Found {discontinuity_count} discontinuities above threshold {threshold}")
    
    return discontinuity_count, channels_to_analyze, df

def visualize_discontinuities(csv_file, threshold=0.1, channel='all', save_path=None):
    """Create graphical visualization of discontinuities."""
    if not MATPLOTLIB_AVAILABLE:
        print("Error: matplotlib is required for graphical visualization.")
        print("Install it with: pip install matplotlib")
        return
    
    if not os.path.exists(csv_file):
        print(f"Error: CSV file '{csv_file}' not found.")
        return
    
    df = pd.read_csv(csv_file)
    
    # Determine which channels to analyze (same logic as detect_large_jumps)
    channels_to_analyze = []
    
    if channel == 'all' or channel == 'multi':
        # Auto-detect all channels (channel_0, channel_1, channel_2, ...)
        ch_idx = 0
        while f'channel_{ch_idx}' in df.columns:
            channels_to_analyze.append((f'channel_{ch_idx}', f'Channel {ch_idx}'))
            ch_idx += 1
        
        # Fallback to legacy formats if no channel_N columns found
        if not channels_to_analyze:
            if 'left_channel' in df.columns:
                channels_to_analyze.append(('left_channel', 'Left'))
            if 'right_channel' in df.columns:
                channels_to_analyze.append(('right_channel', 'Right'))
            if 'amplitude' in df.columns and not channels_to_analyze:
                channels_to_analyze.append(('amplitude', 'Amplitude'))
    elif channel in ['both', 'left', 'l']:
        if 'left_channel' in df.columns:
            channels_to_analyze.append(('left_channel', 'Left'))
        elif 'channel_0' in df.columns:
            channels_to_analyze.append(('channel_0', 'Channel 0'))
        elif 'amplitude' in df.columns:
            channels_to_analyze.append(('amplitude', 'Amplitude'))
    elif channel in ['both', 'right', 'r']:
        if 'right_channel' in df.columns:
            channels_to_analyze.append(('right_channel', 'Right'))
        elif 'channel_1' in df.columns:
            channels_to_analyze.append(('channel_1', 'Channel 1'))
    else:
        # Try to parse channel number (e.g., 'channel_2' or '2')
        try:
            ch_num = int(channel)
            ch_col = f'channel_{ch_num}'
            if ch_col in df.columns:
                channels_to_analyze.append((ch_col, f'Channel {ch_num}'))
        except ValueError:
            pass
    
    if not channels_to_analyze:
        print("Error: No valid channels found for visualization.")
        return
    
    # Get time axis
    frame_col = 'frame' if 'frame' in df.columns else 'sample_index'
    if frame_col not in df.columns:
        print("Error: CSV file must contain either 'frame' or 'sample_index' column")
        return
    
    if 'time_seconds' in df.columns:
        time_axis = df['time_seconds'].values
    else:
        time_axis = df[frame_col].values / 44100.0
    
    num_channels = len(channels_to_analyze)
    fig = plt.figure(figsize=(15, 5 * num_channels + 3))
    gs = fig.add_gridspec(num_channels * 2 + 1, 3, hspace=0.3, wspace=0.3)
    
    all_discontinuities = []
    
    # Plot each channel
    for ch_idx, (channel_col, channel_name) in enumerate(channels_to_analyze):
        values = df[channel_col].values
        
        # Find discontinuities
        discontinuities = []
        for i in range(1, len(values)):
            jump = abs(values[i] - values[i-1])
            if jump > threshold:
                discontinuities.append((i, values[i-1], values[i], jump, time_axis[i]))
        
        all_discontinuities.extend([(ch_idx, d) for d in discontinuities])
        
        # Full waveform plot
        ax1 = fig.add_subplot(gs[ch_idx * 2, :])
        ax1.plot(time_axis, values, linewidth=0.5, alpha=0.7, label=f'{channel_name} Channel')
        
        # Highlight discontinuities
        if discontinuities:
            disc_indices = [d[0] for d in discontinuities]
            disc_values = [values[i] for i in disc_indices]
            disc_times = [d[4] for d in discontinuities]
            ax1.scatter(disc_times, disc_values, color='red', s=30, zorder=5, 
                       label=f'Discontinuities (>{threshold})', marker='x')
        
        ax1.set_xlabel('Time (seconds)')
        ax1.set_ylabel('Amplitude')
        ax1.set_title(f'{channel_name} Channel - Full Waveform ({len(discontinuities)} discontinuities)')
        ax1.grid(True, alpha=0.3)
        ax1.legend()
        
        # Sample-to-sample differences plot
        ax2 = fig.add_subplot(gs[ch_idx * 2 + 1, :])
        sample_diffs = np.abs(np.diff(values))
        ax2.plot(time_axis[1:], sample_diffs, linewidth=0.5, alpha=0.7, color='orange')
        ax2.axhline(y=threshold, color='r', linestyle='--', linewidth=2, label=f'Threshold ({threshold})')
        
        # Highlight discontinuities in difference plot
        if discontinuities:
            disc_times_diff = [d[4] for d in discontinuities]
            disc_jumps = [d[3] for d in discontinuities]
            ax2.scatter(disc_times_diff, disc_jumps, color='red', s=30, zorder=5, 
                       marker='x', label='Discontinuities')
        
        ax2.set_xlabel('Time (seconds)')
        ax2.set_ylabel('Sample-to-Sample Difference')
        ax2.set_title(f'{channel_name} Channel - Sample Differences')
        ax2.grid(True, alpha=0.3)
        ax2.legend()
        ax2.set_yscale('log')  # Use log scale to better see small differences
    
    # Statistics summary
    ax_stats = fig.add_subplot(gs[-1, :])
    ax_stats.axis('off')
    
    total_discontinuities = sum(len([d for d in all_discontinuities if d[0] == ch]) 
                                for ch in range(num_channels))
    
    stats_text = f"""
    Statistics Summary:
    - Total samples: {len(df)}
    - Duration: {time_axis[-1]:.3f} seconds
    - Sample rate: {len(df) / time_axis[-1]:.0f} Hz (estimated)
    - Discontinuity threshold: {threshold}
    - Total discontinuities found: {total_discontinuities}
    """
    
    for ch_idx, (channel_col, channel_name) in enumerate(channels_to_analyze):
        ch_disc = [d[1] for d in all_discontinuities if d[0] == ch_idx]  # Get discontinuity tuples
        if ch_disc:
            max_jump = max(d[3] for d in ch_disc)  # d[3] is the jump magnitude
            stats_text += f"  - {channel_name}: {len(ch_disc)} discontinuities (max jump: {max_jump:.6f})\n"
    
    ax_stats.text(0.05, 0.95, stats_text, transform=ax_stats.transAxes,
                 fontsize=10, verticalalignment='top', family='monospace',
                 bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.suptitle(f'Audio Discontinuity Analysis: {os.path.basename(csv_file)}', fontsize=14, fontweight='bold')
    
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
        print(f"Saved visualization to {save_path}")
    
    plt.show()

def main():
    parser = argparse.ArgumentParser(description='Analyze audio discontinuities by printing all sample values')
    parser.add_argument('csv_file', help='Path to the CSV file containing audio data')
    parser.add_argument('--channel', type=str, default='all',
                       help='Which channel(s) to analyze: "all" (auto-detect all), "left"/"l", "right"/"r", "both", or channel number like "0", "1", "2", etc. (default: all)')
    parser.add_argument('--max-samples', type=int, 
                       help='Maximum number of samples to print (default: all)')
    parser.add_argument('--threshold', type=float, default=0.2,
                       help='Threshold for discontinuity detection (default: 0.2)')
    parser.add_argument('--detect-only', action='store_true',
                       help='Only detect discontinuities, do not print all samples')
    parser.add_argument('--plot', action='store_true',
                       help='Show graphical visualization (requires matplotlib)')
    parser.add_argument('--plot-only', action='store_true',
                       help='Only show graphical visualization, skip text output')
    parser.add_argument('--save-plot', type=str, metavar='PATH',
                       help='Save the plot to a file instead of displaying (e.g., --save-plot=output.png)')
    
    args = parser.parse_args()
    
    # Handle plot-only mode
    if args.plot_only:
        if not MATPLOTLIB_AVAILABLE:
            print("Error: matplotlib is required for --plot-only option.")
            print("Install it with: pip install matplotlib")
            return
        visualize_discontinuities(args.csv_file, args.threshold, args.channel, args.save_plot)
        return
    
    # Normal text output
    if args.detect_only:
        detect_large_jumps(args.csv_file, args.threshold, args.channel)
    else:
        print_all_samples(args.csv_file, args.channel, args.max_samples)
        print()
        detect_large_jumps(args.csv_file, args.threshold, args.channel)
    
    # Show plot if requested
    if args.plot or args.save_plot:
        visualize_discontinuities(args.csv_file, args.threshold, args.channel, args.save_plot)

if __name__ == "__main__":
    main()
