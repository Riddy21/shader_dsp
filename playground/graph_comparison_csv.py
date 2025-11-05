#!/usr/bin/env python3
"""
Interactive graph viewer for comparison CSV files.
Usage: python3 graph_comparison_csv.py [filename]
"""

import csv
import sys
import os
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.widgets import CheckButtons

def graph_csv(filename):
    """Create interactive graph from CSV file."""
    if not os.path.exists(filename):
        print(f"Error: File '{filename}' not found.")
        return
    
    # Read CSV data
    samples = []
    inputs = []
    outputs = []
    
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            samples.append(int(row['sample_index']))
            inputs.append(float(row['input']))
            outputs.append(float(row['output']))
    
    if not samples:
        print("File is empty.")
        return
    
    samples = np.array(samples)
    inputs = np.array(inputs)
    outputs = np.array(outputs)
    differences = np.abs(inputs - outputs)
    
    # Create figure with subplots
    fig, axes = plt.subplots(3, 1, figsize=(14, 10))
    fig.suptitle(f'Audio Comparison: {os.path.basename(filename)}', fontsize=14, fontweight='bold')
    
    # Plot 1: Input vs Output overlay
    ax1 = axes[0]
    line1_input, = ax1.plot(samples, inputs, 'b-', label='Input', linewidth=1.5, alpha=0.7)
    line1_output, = ax1.plot(samples, outputs, 'r-', label='Output', linewidth=1.5, alpha=0.7)
    ax1.set_xlabel('Sample Index')
    ax1.set_ylabel('Amplitude')
    ax1.set_title('Input vs Output (Overlay)')
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim([-0.35, 0.35])
    
    # Plot 2: Input vs Output side by side
    ax2 = axes[1]
    line2_input, = ax2.plot(samples, inputs, 'b-', label='Input', linewidth=1.5, alpha=0.7)
    line2_output, = ax2.plot(samples, outputs, 'r-', label='Output', linewidth=1.5, alpha=0.7)
    ax2.set_xlabel('Sample Index')
    ax2.set_ylabel('Amplitude')
    ax2.set_title('Input vs Output (Side by Side)')
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim([-0.35, 0.35])
    
    # Plot 3: Difference
    ax3 = axes[2]
    line3_diff, = ax3.plot(samples, differences, 'g-', label='|Input - Output|', linewidth=1.5, alpha=0.7)
    ax3.set_xlabel('Sample Index')
    ax3.set_ylabel('Absolute Difference')
    ax3.set_title('Absolute Difference Between Input and Output')
    ax3.legend(loc='upper right')
    ax3.grid(True, alpha=0.3)
    ax3.set_ylim([0, max(differences) * 1.1])
    
    # Add checkboxes to toggle visibility
    ax_check = plt.axes([0.02, 0.5, 0.15, 0.15])
    check = CheckButtons(ax_check, ('Show Input', 'Show Output', 'Show Difference'), 
                        (True, True, True))
    
    def toggle_lines(label):
        if label == 'Show Input':
            line1_input.set_visible(not line1_input.get_visible())
            line2_input.set_visible(not line2_input.get_visible())
        elif label == 'Show Output':
            line1_output.set_visible(not line1_output.get_visible())
            line2_output.set_visible(not line2_output.get_visible())
        elif label == 'Show Difference':
            line3_diff.set_visible(not line3_diff.get_visible())
        plt.draw()
    
    check.on_clicked(toggle_lines)
    
    # Add statistics text box
    stats_text = f"""Statistics:
    Samples: {len(samples)}
    Input range: [{inputs.min():.6f}, {inputs.max():.6f}]
    Output range: [{outputs.min():.6f}, {outputs.max():.6f}]
    Avg difference: {differences.mean():.6f}
    Max difference: {differences.max():.6f}
    Min difference: {differences.min():.6f}
    RMS difference: {np.sqrt(np.mean(differences**2)):.6f}"""
    
    fig.text(0.02, 0.02, stats_text, fontsize=9, verticalalignment='bottom',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    plt.subplots_adjust(left=0.15, bottom=0.15)
    
    print(f"\n{'='*70}")
    print(f"Displaying graph for: {filename}")
    print(f"{'='*70}")
    print(f"Total samples: {len(samples)}")
    print(f"Input range: [{inputs.min():.9f}, {inputs.max():.9f}]")
    print(f"Output range: [{outputs.min():.9f}, {outputs.max():.9f}]")
    print(f"Average difference: {differences.mean():.9f}")
    print(f"Max difference: {differences.max():.9f}")
    print(f"\nClose the graph window to exit.")
    print(f"{'='*70}\n")
    
    plt.show()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        filename = sys.argv[1]
    else:
        # Default to the most recent comparison file
        import glob
        files = glob.glob("*comparison_output.csv")
        if files:
            filename = sorted(files)[-1]  # Most recent
            print(f"No filename provided, using: {filename}")
        else:
            print("No comparison CSV files found.")
            print("\nAvailable CSV files:")
            for f in glob.glob("*.csv"):
                print(f"  - {f}")
            sys.exit(1)
    
    graph_csv(filename)

