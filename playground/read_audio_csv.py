#!/usr/bin/env python3
import csv
import math
import argparse
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except Exception as exc:  # matplotlib is optional; plot features will error if missing
    plt = None


def read_interleaved_csv(csv_path: Path):
    with csv_path.open("r", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if header is None:
            return []
        # Expect header: sample_index,ch0,ch1,...
        num_channels = len(header) - 1
        interleaved = []
        for row in reader:
            if not row:
                continue
            # row[0] is sample index, rest are channel values
            vals = [float(x) for x in row[1:1 + num_channels]]
            interleaved.extend(vals)
        return interleaved, num_channels


def read_input_output_csv(csv_path: Path):
    """Read CSV with columns: sample_index,input,output (and possibly extra trailing empty column).
    Returns (input_list, output_list) or ([], []) if not detectable.
    """
    with csv_path.open("r", newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if not header:
            return [], []

        # Find columns by name (case-sensitive as produced by the test)
        try:
            input_idx = header.index("input")
            output_idx = header.index("output")
        except ValueError:
            return [], []

        input_vals = []
        output_vals = []

        for row in reader:
            if not row:
                continue

            # Wide-row case: multiple samples concatenated on one line
            # Pattern is repeating groups of: sample_index, input, output, [optional empty]
            # We'll scan the row sequentially and greedily parse groups first.
            p = 0
            parsed_any = False
            while p + 2 < len(row):
                # Skip empty tokens
                if row[p] == "":
                    p += 1
                    continue
                # Try parse index, input, output
                try:
                    _ = int(float(row[p]))  # index (accept numeric but may be formatted)
                    inp = float(row[p + 1])
                    out = float(row[p + 2])
                except Exception:
                    p += 1
                    continue

                input_vals.append(inp)
                output_vals.append(out)
                parsed_any = True

                # Advance by 4 if there is a trailing empty token, else by 3
                if (p + 3) < len(row) and row[p + 3] == "":
                    p += 4
                else:
                    p += 3

            # If no wide-row groups parsed, try normal column-based single-sample row
            if not parsed_any and len(row) > max(input_idx, output_idx):
                try:
                    input_vals.append(float(row[input_idx]))
                    output_vals.append(float(row[output_idx]))
                except ValueError:
                    # Skip malformed row
                    pass

        return input_vals, output_vals


def compute_rms(values):
    if not values:
        return 0.0
    s = sum(v * v for v in values)
    return math.sqrt(s / len(values))


def main():
    parser = argparse.ArgumentParser(description="Read audio CSVs, compute error metrics, and optionally plot.")
    parser.add_argument("base", help="Base name prefix, e.g., audio_simple_record_playback_256_buffer_2_channels")
    parser.add_argument("--dir", default=str(Path.cwd()), help="Directory containing CSV files (default: cwd)")
    parser.add_argument("--write-diff", action="store_true", help="Write per-sample diff CSV")
    # Plotting options
    parser.add_argument("--plot", action="store_true", help="Show plots comparing tape vs output and their difference")
    parser.add_argument("--channel", type=int, default=0, help="Channel index to plot (default: 0)")
    parser.add_argument("--start", type=int, default=0, help="Start sample index (per-channel indexing)")
    parser.add_argument("--end", type=int, default=None, help="End sample index (per-channel indexing, exclusive)")
    parser.add_argument("--downsample", type=int, default=1, help="Plot every Nth sample to speed up rendering")
    parser.add_argument("--save", type=str, default=None, help="If set, save figure to this path instead of/as well as showing")
    parser.add_argument("--no-show", action="store_true", help="Do not display interactive window (use with --save)")
    args = parser.parse_args()

    base = args.base
    dir_path = Path(args.dir)
    out_csv = dir_path / f"{base}_output.csv"
    if not out_csv.exists():
        print(f"Missing CSV: {out_csv}")
        return 1

    # Try new combined format first: sample_index,input,output
    input_vals, output_vals = read_input_output_csv(out_csv)

    combined_mode = len(input_vals) > 0 and len(output_vals) > 0

    if combined_mode:
        n = min(len(input_vals), len(output_vals))
        input_vals = input_vals[:n]
        output_vals = output_vals[:n]

        diff_vals = [o - t for o, t in zip(output_vals, input_vals)]
        rms_err = compute_rms(diff_vals)
        sig_rms = compute_rms(input_vals)
        snr = float('inf') if rms_err == 0 else 20.0 * math.log10(sig_rms / rms_err) if sig_rms > 0 else -float('inf')

        print(f"Base: {base} (combined input/output)")
        print(f"Parsed samples: {n}")
        print(f"RMS error = {rms_err:.8f}, SNR = {snr:.2f} dB")
    else:
        # Fallback to legacy format: separate CSVs with interleaved channels
        tape_csv = dir_path / f"{base}_record_tape.csv"
        if not tape_csv.exists():
            print(f"Could not parse combined CSV and legacy tape CSV missing: {tape_csv}")
            return 1

        output_interleaved, out_channels = read_interleaved_csv(out_csv)
        tape_interleaved, tape_channels = read_interleaved_csv(tape_csv)

        if out_channels != tape_channels:
            print(f"Channel mismatch: output has {out_channels}, tape has {tape_channels}")
            return 1

        num_channels = out_channels
        n = min(len(output_interleaved), len(tape_interleaved))
        if n == 0:
            print("No samples to compare.")
            return 1

        # Compute per-channel RMS error and SNR
        per_channel_err = []
        per_channel_snr = []
        for ch in range(num_channels):
            out_ch = output_interleaved[ch:n: num_channels]
            tape_ch = tape_interleaved[ch:n: num_channels]
            diff = [o - t for o, t in zip(out_ch, tape_ch)]
            rms_err = compute_rms(diff)
            sig_rms = compute_rms(tape_ch)
            snr = float('inf') if rms_err == 0 else 20.0 * math.log10(sig_rms / rms_err) if sig_rms > 0 else -float('inf')
            per_channel_err.append(rms_err)
            per_channel_snr.append(snr)

        # Print summary
        print(f"Base: {base}")
        for ch in range(num_channels):
            print(f"ch{ch}: RMS error = {per_channel_err[ch]:.8f}, SNR = {per_channel_snr[ch]:.2f} dB")

    if args.write_diff:
        diff_csv = dir_path / f"{base}_diff.csv"
        with diff_csv.open("w", newline="") as f:
            writer = csv.writer(f)
            if combined_mode:
                writer.writerow(["sample_index", "diff"])
                for i, (t, o) in enumerate(zip(input_vals, output_vals)):
                    writer.writerow([i, o - t])
            else:
                header = ["sample_index"] + [f"ch{ch}_diff" for ch in range(num_channels)]
                writer.writerow(header)
                # write up to n samples (per channel interleaved)
                frames = n // num_channels
                for i in range(frames):
                    row = [i]
                    for ch in range(num_channels):
                        o = output_interleaved[i * num_channels + ch]
                        t = tape_interleaved[i * num_channels + ch]
                        row.append(o - t)
                    writer.writerow(row)
        print(f"Wrote diff to {diff_csv}")

    # Plot if requested
    if args.plot:
        if plt is None:
            print("matplotlib is not installed. Install it with: pip install matplotlib")
            return 1

        start = max(0, args.start)
        end = args.end

        if combined_mode:
            ref = input_vals
            out = output_vals
        else:
            ch = max(0, min(args.channel, num_channels - 1))
            ref = tape_interleaved[ch:n: num_channels]
            out = output_interleaved[ch:n: num_channels]

        if end is None or end > len(out):
            end = len(out)
        if start >= end:
            print("Invalid range: start must be < end")
            return 1

        # Slice the requested window
        ref_win = ref[start:end]
        out_win = out[start:end]
        diff_win = [o - t for o, t in zip(out_win, ref_win)]

        # Optional downsampling for speed
        ds = max(1, args.downsample)
        xs = list(range(start, end, ds))
        ref_ds = ref_win[0:len(ref_win):ds]
        out_ds = out_win[0:len(out_win):ds]
        diff_ds = diff_win[0:len(diff_win):ds]

        # Metrics over the window
        window_rms = compute_rms(diff_win)
        window_sig_rms = compute_rms(ref_win)
        window_snr = float('inf') if window_rms == 0 else 20.0 * math.log10(window_sig_rms / window_rms) if window_sig_rms > 0 else -float('inf')

        fig, axes = plt.subplots(3, 1, figsize=(12, 8), sharex=True, constrained_layout=True)
        axes[0].plot(xs, ref_ds, label="input" if combined_mode else "tape", color="C0", linewidth=0.9)
        axes[0].set_ylabel("Amplitude")
        axes[0].set_title("Input" if combined_mode else (f"Tape (ch{args.channel})"))
        axes[0].grid(True, alpha=0.3)

        axes[1].plot(xs, out_ds, label="output", color="C1", linewidth=0.9)
        axes[1].set_ylabel("Amplitude")
        axes[1].set_title("Output")
        axes[1].grid(True, alpha=0.3)

        axes[2].plot(xs, diff_ds, label="diff", color="C3", linewidth=0.9)
        axes[2].set_xlabel("Sample index")
        axes[2].set_ylabel("Error")
        axes[2].set_title(f"Diff (output - input), RMS={window_rms:.6g}, SNR={window_snr:.2f} dB")
        axes[2].grid(True, alpha=0.3)

        if args.save:
            out_path = Path(args.save)
            out_path.parent.mkdir(parents=True, exist_ok=True)
            fig.savefig(out_path, dpi=150)
            print(f"Saved figure to {out_path}")

        if not args.no_show:
            plt.show()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())


