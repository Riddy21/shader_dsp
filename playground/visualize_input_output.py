#!/usr/bin/env python3
"""
Visualize input sine wave and output audio from CSV files.

This script reads input_sine_wave.csv and output_audio_speed_*.csv files and creates
visualizations comparing the input and output signals for each speed, showing both channels.
"""

import csv
import argparse
import glob
from pathlib import Path
import sys
import re

try:
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError as e:
    print(f"Error: Required packages not installed. Please install with: pip install matplotlib numpy")
    sys.exit(1)


def read_audio_csv(csv_path: Path):
    """Read audio CSV file with format: sample_index,time_seconds,channel_0,channel_1,...
    
    Returns:
        tuple: (time_seconds_list, channels_dict) where channels_dict maps channel_index -> list of samples
    """
    if not csv_path.exists():
        raise FileNotFoundError(f"CSV file not found: {csv_path}")
    
    time_seconds = []
    channels = {}
    
    with csv_path.open("r", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        
        if header is None:
            raise ValueError(f"Empty CSV file: {csv_path}")
        
        # Parse header to find channel columns
        channel_indices = []
        for i, col_name in enumerate(header):
            if col_name.startswith("channel_"):
                ch_num = int(col_name.split("_")[1])
                channel_indices.append((i, ch_num))
                channels[ch_num] = []
        
        # Read data rows
        for row in reader:
            if not row or len(row) < 3:
                continue
            
            try:
                # sample_index is column 0, time_seconds is column 1
                time_val = float(row[1])
                time_seconds.append(time_val)
                
                # Read channel data
                for col_idx, ch_num in channel_indices:
                    if col_idx < len(row):
                        channels[ch_num].append(float(row[col_idx]))
                    else:
                        channels[ch_num].append(0.0)
            except (ValueError, IndexError) as e:
                print(f"Warning: Skipping malformed row: {row}")
                continue
    
    return time_seconds, channels


def extract_speed_from_filename(filename: str):
    """Extract speed value from filename like 'output_audio_speed_0.500000.csv'"""
    match = re.search(r'speed_([\d.]+)\.csv', filename)
    if match:
        return float(match.group(1))
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Visualize input sine wave and output audio from CSV files for multiple speeds"
    )
    parser.add_argument(
        "--input-csv",
        type=str,
        default="input_sine_wave.csv",
        help="Path to input sine wave CSV file (default: input_sine_wave.csv)"
    )
    parser.add_argument(
        "--output-csv-pattern",
        type=str,
        default="output_audio_speed_*.csv",
        help="Glob pattern for output CSV files (default: output_audio_speed_*.csv)"
    )
    parser.add_argument(
        "--start",
        type=float,
        default=0.0,
        help="Start time in seconds (default: 0.0)"
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=None,
        help="Duration in seconds to visualize (default: all data)"
    )
    parser.add_argument(
        "--downsample",
        type=int,
        default=1,
        help="Plot every Nth sample to speed up rendering (default: 1)"
    )
    parser.add_argument(
        "--save",
        type=str,
        default=None,
        help="Save figure to this path instead of displaying (will append speed suffix)"
    )
    parser.add_argument(
        "--no-show",
        action="store_true",
        help="Do not display interactive window (use with --save)"
    )
    parser.add_argument(
        "--dir",
        type=str,
        default=".",
        help="Directory containing CSV files (default: current directory)"
    )
    
    args = parser.parse_args()
    
    dir_path = Path(args.dir)
    
    # Read input CSV
    input_path = dir_path / args.input_csv
    try:
        input_time, input_channels = read_audio_csv(input_path)
    except Exception as e:
        print(f"Error reading input CSV file: {e}")
        return 1
    
    # Find all output CSV files matching the pattern
    output_pattern = dir_path / args.output_csv_pattern
    output_files = sorted(glob.glob(str(output_pattern)))
    
    if not output_files:
        print(f"No output CSV files found matching pattern: {output_pattern}")
        return 1
    
    print(f"Found {len(output_files)} output CSV file(s)")
    
    # Read all output CSV files and extract speeds
    output_data_by_speed = {}
    for output_file in output_files:
        speed = extract_speed_from_filename(output_file)
        if speed is None:
            print(f"Warning: Could not extract speed from filename: {output_file}")
            continue
        
        try:
            output_time, output_channels = read_audio_csv(Path(output_file))
            output_data_by_speed[speed] = (output_time, output_channels)
            print(f"Loaded output CSV for speed {speed}x: {output_file}")
        except Exception as e:
            print(f"Error reading output CSV {output_file}: {e}")
            continue
    
    if not output_data_by_speed:
        print("No valid output CSV files found")
        return 1
    
    # Get available channels from input
    available_channels = sorted(input_channels.keys())
    if not available_channels:
        print("No channels found in input CSV")
        return 1
    
    print(f"Available channels: {available_channels}")
    
    # Convert input to numpy arrays
    input_time_arr = np.array(input_time)
    input_channels_arr = {ch: np.array(input_channels[ch]) for ch in available_channels}
    
    # Create visualization for each speed
    for speed in sorted(output_data_by_speed.keys()):
        output_time, output_channels = output_data_by_speed[speed]
        
        # Verify channels match
        output_available_channels = sorted(output_channels.keys())
        if set(available_channels) != set(output_available_channels):
            print(f"Warning: Channel mismatch for speed {speed}x. Input: {available_channels}, Output: {output_available_channels}")
        
        # Convert output to numpy arrays
        output_time_arr = np.array(output_time)
        output_channels_arr = {ch: np.array(output_channels[ch]) for ch in output_available_channels}
        
        # Apply time window if specified
        start_idx_input = 0
        end_idx_input = len(input_time_arr)
        start_idx_output = 0
        end_idx_output = len(output_time_arr)
        
        if args.start > 0:
            start_idx_input = np.searchsorted(input_time_arr, args.start)
            start_idx_output = np.searchsorted(output_time_arr, args.start)
        
        if args.duration is not None:
            end_time = args.start + args.duration
            end_idx_input = np.searchsorted(input_time_arr, end_time)
            end_idx_output = np.searchsorted(output_time_arr, end_time)
        
        input_time_window = input_time_arr[start_idx_input:end_idx_input]
        output_time_window = output_time_arr[start_idx_output:end_idx_output]
        
        # Create figure with subplots: 3 rows (Input, Output, Difference) x N columns (one per channel)
        num_channels = len(available_channels)
        fig, axes = plt.subplots(3, num_channels, figsize=(6 * num_channels, 10), sharex=True)
        
        # Handle single channel case (axes becomes 1D instead of 2D)
        if num_channels == 1:
            axes = axes.reshape(-1, 1)
        
        for ch_idx, ch in enumerate(available_channels):
            input_samples_window = input_channels_arr[ch][start_idx_input:end_idx_input]
            output_samples_window = output_channels_arr[ch][start_idx_output:end_idx_output]
            
            # Downsample if requested
            ds = max(1, args.downsample)
            input_time_plot = input_time_window[::ds]
            input_samples_plot = input_samples_window[::ds]
            output_time_plot = output_time_window[::ds]
            output_samples_plot = output_samples_window[::ds]
            
            # Calculate statistics
            input_rms = np.sqrt(np.mean(input_samples_window**2))
            output_rms = np.sqrt(np.mean(output_samples_window**2))
            
            # Calculate difference if lengths match
            if len(input_samples_window) == len(output_samples_window):
                diff_samples = output_samples_window - input_samples_window
                diff_rms = np.sqrt(np.mean(diff_samples**2))
                snr_db = 20 * np.log10(input_rms / diff_rms) if diff_rms > 0 else float('inf')
                diff_samples_plot = diff_samples[::ds]
            else:
                diff_samples = None
                diff_rms = None
                snr_db = None
                diff_samples_plot = None
            
            # Plot 1: Input sine wave
            axes[0, ch_idx].plot(input_time_plot, input_samples_plot, 'b-', linewidth=0.8, label=f'Input ch{ch}')
            axes[0, ch_idx].set_ylabel('Amplitude')
            axes[0, ch_idx].set_title(f'Input Sine Wave (ch{ch}) - RMS: {input_rms:.6f}')
            axes[0, ch_idx].grid(True, alpha=0.3)
            axes[0, ch_idx].legend()
            
            # Plot 2: Output audio
            axes[1, ch_idx].plot(output_time_plot, output_samples_plot, 'r-', linewidth=0.8, label=f'Output ch{ch}')
            axes[1, ch_idx].set_ylabel('Amplitude')
            axes[1, ch_idx].set_title(f'Output Audio (ch{ch}, speed={speed}x) - RMS: {output_rms:.6f}')
            axes[1, ch_idx].grid(True, alpha=0.3)
            axes[1, ch_idx].legend()
            
            # Plot 3: Difference
            if diff_samples_plot is not None:
                diff_time_plot = input_time_window[::ds]
                axes[2, ch_idx].plot(diff_time_plot, diff_samples_plot, 'g-', linewidth=0.8, label='Diff (Out-In)')
                axes[2, ch_idx].set_xlabel('Time (seconds)')
                axes[2, ch_idx].set_ylabel('Amplitude')
                title = f'Difference (ch{ch}) - RMS: {diff_rms:.6f}'
                if snr_db is not None:
                    title += f', SNR: {snr_db:.2f} dB'
                axes[2, ch_idx].set_title(title)
                axes[2, ch_idx].grid(True, alpha=0.3)
                axes[2, ch_idx].legend()
            else:
                axes[2, ch_idx].text(0.5, 0.5, 'Difference unavailable\n(lengths differ)', 
                            ha='center', va='center', transform=axes[2, ch_idx].transAxes)
                axes[2, ch_idx].set_xlabel('Time (seconds)')
                axes[2, ch_idx].set_title(f'Difference (ch{ch}) - unavailable')
        
        plt.suptitle(f'Input vs Output Comparison (Speed: {speed}x)', fontsize=14, y=0.995)
        plt.tight_layout()
        
        # Save or show
        if args.save:
            save_path = Path(args.save)
            # Append speed to filename
            save_stem = save_path.stem
            save_suffix = save_path.suffix or '.png'
            save_dir = save_path.parent
            speed_filename = save_dir / f"{save_stem}_speed_{speed:.6f}{save_suffix}"
            speed_filename.parent.mkdir(parents=True, exist_ok=True)
            fig.savefig(speed_filename, dpi=150, bbox_inches='tight')
            print(f"Saved figure to {speed_filename}")
        
        if not args.no_show:
            plt.show()
        
        # Print summary statistics
        print(f"\n=== Summary Statistics for Speed {speed}x ===")
        for ch in available_channels:
            input_samples_window = input_channels_arr[ch][start_idx_input:end_idx_input]
            output_samples_window = output_channels_arr[ch][start_idx_output:end_idx_output]
            input_rms = np.sqrt(np.mean(input_samples_window**2))
            output_rms = np.sqrt(np.mean(output_samples_window**2))
            
            if len(input_samples_window) == len(output_samples_window):
                diff_samples = output_samples_window - input_samples_window
                diff_rms = np.sqrt(np.mean(diff_samples**2))
                snr_db = 20 * np.log10(input_rms / diff_rms) if diff_rms > 0 else float('inf')
                print(f"  Channel {ch}: Input RMS={input_rms:.8f}, Output RMS={output_rms:.8f}, Diff RMS={diff_rms:.8f}, SNR={snr_db:.2f} dB")
            else:
                print(f"  Channel {ch}: Input RMS={input_rms:.8f}, Output RMS={output_rms:.8f} (lengths differ)")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
