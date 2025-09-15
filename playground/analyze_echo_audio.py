#!/usr/bin/env python3
"""
Audio Echo Test Analysis and Visualization

This script reads the CSV data exported from the C++ echo test and provides
comprehensive analysis and visualization of the audio data, including:
- Waveform visualization with note boundaries
- Derivative analysis for smoothness detection  
- Echo decay analysis
- Discontinuity detection and reporting
- Interactive plots for detailed examination

Usage: python analyze_echo_audio.py
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from scipy import signal
import os
import sys

def load_audio_data(csv_filename="echo_test_audio_data.csv"):
    """Load audio data from CSV file"""
    if not os.path.exists(csv_filename):
        print(f"Error: {csv_filename} not found!")
        print("Please run the C++ test first to generate the audio data.")
        return None
    
    try:
        df = pd.read_csv(csv_filename)
        print(f"Loaded {len(df)} samples from {csv_filename}")
        print(f"Duration: {df['time'].max():.3f} seconds")
        print(f"Sample rate: {len(df) / df['time'].max():.0f} Hz")
        return df
    except Exception as e:
        print(f"Error loading {csv_filename}: {e}")
        return None

def load_smoothness_data(csv_filename="echo_smoothness_analysis.csv"):
    """Load derivative analysis data from CSV file"""
    if not os.path.exists(csv_filename):
        print(f"Warning: {csv_filename} not found. Skipping derivative analysis.")
        return None
    
    try:
        df = pd.read_csv(csv_filename)
        print(f"Loaded smoothness analysis data: {len(df)} samples")
        return df
    except Exception as e:
        print(f"Error loading {csv_filename}: {e}")
        return None

def analyze_note_transitions(df):
    """Analyze note transitions and identify potential issues"""
    print("\n=== Note Transition Analysis ===")
    
    # Find note transitions
    df['note_changed'] = df['note_index'].diff() != 0
    transitions = df[df['note_changed'] & (df['note_index'] != -1)]
    
    print(f"Found {len(transitions)} note transitions")
    
    for _, transition in transitions.iterrows():
        prev_idx = transition.name - 1
        if prev_idx >= 0:
            prev_note = df.iloc[prev_idx]['note_index']
            curr_note = transition['note_index']
            time = transition['time']
            print(f"  t={time:.3f}s: Note {prev_note} -> {curr_note} ({transition['note_frequency']:.1f} Hz)")
    
    return transitions

def detect_discontinuities(df, channel='left_channel', threshold=0.05):
    """Detect amplitude discontinuities in the audio"""
    print(f"\n=== Discontinuity Detection ({channel}) ===")
    
    if channel not in df.columns:
        channel = 'amplitude'  # Fallback for mono audio
    
    audio_data = df[channel].values
    sample_rate = len(df) / df['time'].max()
    
    # Calculate sample-to-sample differences
    diffs = np.abs(np.diff(audio_data))
    discontinuities = np.where(diffs > threshold)[0]
    
    print(f"Discontinuity threshold: {threshold}")
    print(f"Found {len(discontinuities)} discontinuities")
    
    if len(discontinuities) > 0:
        print("Largest discontinuities:")
        # Sort by magnitude and show top 10
        sorted_indices = np.argsort(diffs[discontinuities])[::-1][:10]
        for i, idx in enumerate(sorted_indices):
            disc_idx = discontinuities[idx]
            time = df.iloc[disc_idx]['time']
            magnitude = diffs[disc_idx]
            note_info = f"(Note {df.iloc[disc_idx]['note_index']})" if df.iloc[disc_idx]['note_index'] != -1 else "(No note)"
            print(f"  {i+1}. t={time:.3f}s: {magnitude:.4f} {note_info}")
    
    return discontinuities, diffs

def analyze_echo_decay(df, echo_delay=0.1, num_echoes=5):
    """Analyze echo decay characteristics"""
    print(f"\n=== Echo Decay Analysis ===")
    
    channel = 'left_channel' if 'left_channel' in df.columns else 'amplitude'
    audio_data = df[channel].values
    time_data = df['time'].values
    
    # Find note end times
    note_changes = df['note_index'].diff()
    note_ends = df[(note_changes < 0) | ((df['note_index'] == -1) & (df['note_index'].shift(1) != -1))]
    
    echo_levels = []
    
    for _, note_end in note_ends.iterrows():
        end_time = note_end['time']
        note_idx = df.iloc[note_end.name - 1]['note_index'] if note_end.name > 0 else -1
        
        print(f"Analyzing echoes after note {note_idx} ending at t={end_time:.3f}s:")
        
        for echo_num in range(1, num_echoes + 1):
            echo_time = end_time + echo_num * echo_delay
            
            # Find closest sample to echo time
            closest_idx = np.argmin(np.abs(time_data - echo_time))
            if closest_idx < len(audio_data):
                echo_level = np.abs(audio_data[closest_idx])
                echo_levels.append(echo_level)
                print(f"  Echo {echo_num} (t={echo_time:.3f}s): {echo_level:.4f}")
    
    if echo_levels:
        print(f"Average echo level: {np.mean(echo_levels):.4f}")
        print(f"Max echo level: {np.max(echo_levels):.4f}")
    
    return echo_levels

def create_comprehensive_plot(df, smoothness_df=None, discontinuities=None, diffs=None):
    """Create comprehensive visualization of the audio data"""
    
    # Determine audio channel to plot
    if 'left_channel' in df.columns:
        audio_channel = 'left_channel'
        plot_title = "Echo Test Audio Analysis (Left Channel)"
    else:
        audio_channel = 'amplitude'
        plot_title = "Echo Test Audio Analysis"
    
    # Create subplots
    fig, axes = plt.subplots(4, 1, figsize=(15, 12))
    fig.suptitle(plot_title, fontsize=16)
    
    # Plot 1: Waveform with note boundaries
    ax1 = axes[0]
    ax1.plot(df['time'], df[audio_channel], 'b-', linewidth=0.5, alpha=0.7)
    ax1.set_ylabel('Amplitude')
    ax1.set_title('Audio Waveform with Note Boundaries')
    ax1.grid(True, alpha=0.3)
    
    # Add note boundaries and labels
    note_colors = ['red', 'green', 'blue', 'orange', 'purple']
    current_note = -1
    for i, row in df.iterrows():
        if row['note_index'] != current_note and row['note_index'] != -1:
            current_note = row['note_index']
            color = note_colors[current_note % len(note_colors)]
            ax1.axvline(x=row['time'], color=color, linestyle='--', alpha=0.7)
            ax1.text(row['time'], ax1.get_ylim()[1] * 0.9, f'Note {current_note+1}\n{row["note_frequency"]:.0f}Hz', 
                    rotation=90, verticalalignment='top', color=color, fontsize=8)
    
    # Plot 2: Discontinuities
    ax2 = axes[1]
    if discontinuities is not None and diffs is not None:
        ax2.plot(df['time'][1:], diffs, 'r-', linewidth=0.5)
        if len(discontinuities) > 0:
            ax2.scatter(df['time'].iloc[discontinuities], diffs[discontinuities], 
                       color='red', s=20, zorder=5, label=f'{len(discontinuities)} discontinuities')
        ax2.set_ylabel('Sample Difference')
        ax2.set_title('Amplitude Discontinuities')
        ax2.legend()
    else:
        ax2.text(0.5, 0.5, 'No discontinuity data available', transform=ax2.transAxes, 
                ha='center', va='center')
        ax2.set_title('Amplitude Discontinuities (No Data)')
    ax2.grid(True, alpha=0.3)
    
    # Plot 3: Derivative Analysis (if available)
    ax3 = axes[2]
    if smoothness_df is not None:
        ax3.plot(smoothness_df['time'], smoothness_df['derivative'], 'g-', linewidth=0.5, label='1st Derivative')
        ax3.set_ylabel('Derivative')
        ax3.set_title('Signal Derivatives (Smoothness Analysis)')
        ax3.legend()
    else:
        ax3.text(0.5, 0.5, 'No derivative data available', transform=ax3.transAxes, 
                ha='center', va='center')
        ax3.set_title('Signal Derivatives (No Data)')
    ax3.grid(True, alpha=0.3)
    
    # Plot 4: Second Derivative (if available)
    ax4 = axes[3]
    if smoothness_df is not None:
        ax4.plot(smoothness_df['time'], smoothness_df['second_derivative'], 'purple', linewidth=0.5, label='2nd Derivative')
        
        # Highlight sharp edges
        sharp_threshold = 5000.0
        sharp_edges = np.abs(smoothness_df['second_derivative']) > sharp_threshold
        if sharp_edges.any():
            ax4.scatter(smoothness_df.loc[sharp_edges, 'time'], 
                       smoothness_df.loc[sharp_edges, 'second_derivative'],
                       color='red', s=20, zorder=5, label=f'{sharp_edges.sum()} sharp edges')
        
        ax4.set_ylabel('Second Derivative')
        ax4.set_title('Curvature Analysis (Sharp Edge Detection)')
        ax4.legend()
    else:
        ax4.text(0.5, 0.5, 'No second derivative data available', transform=ax4.transAxes, 
                ha='center', va='center')
        ax4.set_title('Curvature Analysis (No Data)')
    ax4.grid(True, alpha=0.3)
    ax4.set_xlabel('Time (seconds)')
    
    plt.tight_layout()
    return fig

def create_note_detail_plots(df):
    """Create detailed plots for each note transition"""
    channel = 'left_channel' if 'left_channel' in df.columns else 'amplitude'
    
    # Find note transitions
    note_changes = df['note_index'].diff()
    note_starts = df[note_changes > 0]
    note_ends = df[(note_changes < 0) | ((df['note_index'] == -1) & (df['note_index'].shift(1) != -1))]
    
    if len(note_starts) == 0:
        print("No note transitions found for detailed analysis")
        return None
    
    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    fig.suptitle('Detailed Note Transition Analysis', fontsize=16)
    axes = axes.flatten()
    
    for i, (_, start_row) in enumerate(note_starts.iterrows()):
        if i >= 6:  # Limit to 6 plots
            break
            
        ax = axes[i]
        
        # Find corresponding end
        note_idx = start_row['note_index']
        start_time = start_row['time']
        
        # Find end time for this note
        end_candidates = note_ends[note_ends.index > start_row.name]
        if len(end_candidates) > 0:
            end_time = end_candidates.iloc[0]['time']
        else:
            end_time = df['time'].max()
        
        # Plot a window around the note
        window_start = max(0, start_time - 0.1)
        window_end = min(df['time'].max(), end_time + 0.3)  # Include echo time
        
        mask = (df['time'] >= window_start) & (df['time'] <= window_end)
        plot_data = df[mask]
        
        ax.plot(plot_data['time'], plot_data[channel], 'b-', linewidth=1)
        ax.axvline(x=start_time, color='green', linestyle='--', label='Note Start')
        ax.axvline(x=end_time, color='red', linestyle='--', label='Note End')
        
        # Mark echo times
        echo_delay = 0.1
        for echo_num in range(1, 4):
            echo_time = end_time + echo_num * echo_delay
            if echo_time <= window_end:
                ax.axvline(x=echo_time, color='orange', linestyle=':', alpha=0.7, 
                          label=f'Echo {echo_num}' if echo_num == 1 else '')
        
        ax.set_title(f'Note {note_idx + 1} ({start_row["note_frequency"]:.0f} Hz)')
        ax.set_xlabel('Time (s)')
        ax.set_ylabel('Amplitude')
        ax.grid(True, alpha=0.3)
        if i == 0:
            ax.legend()
    
    # Hide unused subplots
    for j in range(i + 1, 6):
        axes[j].set_visible(False)
    
    plt.tight_layout()
    return fig

def main():
    """Main analysis function"""
    print("=== Echo Audio Test Analysis ===")
    
    # Load data
    df = load_audio_data()
    if df is None:
        return
    
    smoothness_df = load_smoothness_data()
    
    # Perform analyses
    transitions = analyze_note_transitions(df)
    discontinuities, diffs = detect_discontinuities(df)
    echo_levels = analyze_echo_decay(df)
    
    # Create visualizations
    print("\n=== Creating Visualizations ===")
    
    # Main comprehensive plot
    main_fig = create_comprehensive_plot(df, smoothness_df, discontinuities, diffs)
    main_fig.savefig('echo_audio_analysis.png', dpi=300, bbox_inches='tight')
    print("Saved main analysis plot: echo_audio_analysis.png")
    
    # Detailed note transition plots
    detail_fig = create_note_detail_plots(df)
    if detail_fig:
        detail_fig.savefig('echo_note_details.png', dpi=300, bbox_inches='tight')
        print("Saved detailed note analysis: echo_note_details.png")
    
    # Summary statistics
    print("\n=== Summary Statistics ===")
    channel = 'left_channel' if 'left_channel' in df.columns else 'amplitude'
    audio_data = df[channel].values
    
    print(f"Audio Statistics:")
    print(f"  Peak amplitude: {np.max(np.abs(audio_data)):.4f}")
    print(f"  RMS level: {np.sqrt(np.mean(audio_data**2)):.4f}")
    print(f"  Dynamic range: {20 * np.log10(np.max(np.abs(audio_data)) / (np.std(audio_data) + 1e-10)):.1f} dB")
    
    if discontinuities is not None:
        print(f"  Discontinuities: {len(discontinuities)} ({len(discontinuities)/len(df)*100:.2f}% of samples)")
    
    if echo_levels:
        print(f"  Echo analysis: {len(echo_levels)} echoes detected")
        print(f"  Average echo level: {np.mean(echo_levels):.4f}")
    
    print("\nAnalysis complete! Check the generated PNG files for visualizations.")
    plt.show()

if __name__ == "__main__":
    main()