#!/usr/bin/env python3
"""
Ultra-simple script to dump raw audio values for discontinuity analysis.
This outputs every single sample value in a compact format.
"""

import pandas as pd
import sys

def main():
    if len(sys.argv) < 2:
        print("Usage: python raw_audio_dump.py <csv_file> [channel] [max_samples]")
        print("  channel: 'left', 'right', or 'both' (default: both)")
        print("  max_samples: maximum number of samples to print (default: all)")
        return 1
    
    csv_file = sys.argv[1]
    channel = sys.argv[2] if len(sys.argv) > 2 else 'both'
    max_samples = int(sys.argv[3]) if len(sys.argv) > 3 else None
    
    try:
        df = pd.read_csv(csv_file)
        
        if max_samples:
            df = df.head(max_samples)
        
        print(f"# Raw audio data from {csv_file}")
        print(f"# Total samples: {len(df)}")
        print(f"# Channel: {channel}")
        print("# Format: frame,time_sec,left_channel,right_channel")
        print()
        
        for i, row in df.iterrows():
            frame = int(row['frame'])
            time_sec = frame / 44100.0
            left = row['left_channel']
            right = row['right_channel']
            
            if channel in ['left', 'l']:
                print(f"{frame},{time_sec:.6f},{left:.8f}")
            elif channel in ['right', 'r']:
                print(f"{frame},{time_sec:.6f},{right:.8f}")
            else:  # both
                print(f"{frame},{time_sec:.6f},{left:.8f},{right:.8f}")
        
        return 0
        
    except Exception as e:
        print(f"Error: {e}")
        return 1

if __name__ == "__main__":
    exit(main())
