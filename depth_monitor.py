

# NAME-OJASVI CHERUKU ------------------------------------------------------------------------------------------------------------------
# ID NUMBER- 2025AAPS0226H -------------------------------------------------------------------------------------------------------------


"""
Task 1: Finding the Sea Floor

Making a simple to understand depth time graph, so that Odysseus can make it out safely from the Charybdis and Scylla. For this, we'll be using the 
following libraries-
1. pandas- CSV loading, numeric coercion, rolling median, interpolation
2. numpy- Array math(i don't need to explain that further, right), NaN(stands for not a number. Who knew!) handling, min/max/percentile
3. matplotlib- Dude the name of the library says what we're using it for. To PLOT the MATH. In this, we'll be using matplotlib.pyplot and 
matplotlib.animation
    (just cuz odysseus has the wisdom of athena doesn't mean the crew does too)
4. scipy.signal- 	Savitzky-Golay smoothing filter for noise reduction.

Data assumption: one sample per second (as stated in the task).

If you don't have the above libraries mentioned, just copy paste this thingie below-
    pip install pandas numpy matplotlib scipy

We'll also be making a GIF of the graph because, well, GIFs are fun! (And help you analyse the data much easier)
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from scipy.signal import savgol_filter

# --------------------------------------------------------------------------
# IMPORTANT INFO AND OTHER STUFF
# --------------------------------------------------------------------------
CSV_PATH = "Depth_Data.csv"          # Replace it with whatever address you have the csv file saved on. Helps when running on terminal
SAMPLE_PERIOD_S = 1.0               
FRAME_INTERVAL_MS = 60               # animation playback speed (ms/frame).
                                        # If you wanna play it at the true real-time speed, set to int(SAMPLE_PERIOD_S * 1000) = 1000
SPIKE_THRESHOLD_M = 150              # setting a local median deviation beyond which the data is considered a spike (Cuz ody's crewmates might
                                    #legit think a shark was under their ship for a fraction of a second if they see spikes out of nowhere)
SMOOTHING_WINDOW = 11                # Savitzky-Golay window (must be odd)
SMOOTHING_POLYORDER = 2
SAVE_GIF = True
GIF_PATH = "depth_animation.gif"

# --------------------------------------------------------------------------
# POSIEDEN'S BOUNTY: GATHER THE RAW SOUNDINGS FROM THE DEEP
# --------------------------------------------------------------------------
def load_data(path):
    df = pd.read_csv(path)
    df["Depth (m)"] = pd.to_numeric(df["Depth (m)"], errors="coerce")  # for eg. '#VALUE!' ---> NaN
    t = df["Point"].to_numpy(dtype=float) * SAMPLE_PERIOD_S            # values in seconds
    depth = df["Depth (m)"].to_numpy(dtype=float)                       # everyone knows (m) stands for metres 
    return t, depth


# --------------------------------------------------------------------------
# THEMIS'S JUDGEMENT: WEIGH EACH READING AND CAST OUT FALSE ONES
# --------------------------------------------------------------------------
def clean_data(depth):
    """
    Like Themis's judgement, we shall flag and repair the sensor faults by: 
      - non-numeric readings (already NaN after pd.to_numeric)
      - physically implausible readings (depth >= 0, i.e. above the surface)
      - statistical spikes (far from the local rolling median)
    Bad points are linearly interpolated from their neighbors(us in lab) so the series stays continuous for downstream alarm logic, while the
    point indices are returned so we can mark them transparently on the plot.
    """
    depth = pd.Series(depth)

    is_nan = depth.isna()
    is_implausible = (depth >= 0) & ~is_nan

    rolling_med = depth.rolling(window=9, center=True, min_periods=1).median()
    is_spike = ((depth - rolling_med).abs() > SPIKE_THRESHOLD_M) & ~is_nan

    bad_mask = (is_nan | is_implausible | is_spike).to_numpy()

    cleaned = depth.copy()
    cleaned[bad_mask] = np.nan
    cleaned = cleaned.interpolate(method="linear", limit_direction="both")

    return cleaned.to_numpy(), bad_mask


# --------------------------------------------------------------------------
# APOLLO'S CLARITY: REVEAL THE TRUE SIGNAL
# --------------------------------------------------------------------------
def smooth_data(cleaned_depth, window=SMOOTHING_WINDOW, polyorder=SMOOTHING_POLYORDER):
    """Apollo couldn't make it so he sent Savitzky-Golay filter: It smooths noise while preserving the shape/slope
    of real seafloor features better than a plain moving average."""
    w = min(window, len(cleaned_depth) - (1 - len(cleaned_depth) % 2))
    if w % 2 == 0:
        w -= 1
    w = max(w, polyorder + 2 if (polyorder + 2) % 2 == 1 else polyorder + 3)
    return savgol_filter(cleaned_depth, window_length=w, polyorder=polyorder)


# --------------------------------------------------------------------------
# HEPHAESTUS'S ANIMATION: BRINGING THE DATA TO LIFE
# --------------------------------------------------------------------------
def animate_depth(t, raw, cleaned, smoothed, bad_mask):
    fig, ax = plt.subplots(figsize=(11, 6))

    ax.set_xlim(t.min(), t.max())
    pad = 0.08 * (np.nanmax(cleaned) - np.nanmin(cleaned))
    ax.set_ylim(np.nanmin(cleaned) - pad, max(20, np.nanmax(cleaned) + pad))

    ax.set_title("Live Ship Depth Sounding", fontsize=14, fontweight="bold")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Depth below surface (m)")
    ax.grid(alpha=0.3)
    ax.axhline(0, color="black", linewidth=1)  # sea surface reference


# --------------------------------------------------------------------------
# ZEUS'S CALL: THE WARNING TO THE SAILORS
# --------------------------------------------------------------------------
    """A configurable "danger zone" near the seafloor's shallowest recorded point, illustrating how this feeds an intervention alarm."""
    danger_depth = np.nanpercentile(cleaned, 5)  # shallowest 5% band as an example
    ax.axhspan(danger_depth, ax.get_ylim()[1], color="red", alpha=0.08)
    ax.axhline(danger_depth, color="red", linestyle="--", linewidth=1, alpha=0.6,
               label=f"Caution depth ~ {danger_depth:.0f} m")

    (raw_line,) = ax.plot([], [], color="lightgray", linewidth=1, label="Cleaned reading")
    (smooth_line,) = ax.plot([], [], color="#1f77b4", linewidth=2, label="Smoothed (Savitzky-Golay)")
    corrected_scatter = ax.scatter([], [], color="green", s=40, zorder=5, label="Corrected sensor fault")
    current_point = ax.scatter([], [], color="orange", s=70, zorder=6, edgecolor="black")
    depth_text = ax.text(0.02, 0.95, "", transform=ax.transAxes, fontsize=11,
                          va="top", bbox=dict(boxstyle="round", fc="white", alpha=0.8))

    ax.legend(loc="lower left", fontsize=9)

    bad_idx = np.where(bad_mask)[0]

    def init():
        raw_line.set_data([], [])
        smooth_line.set_data([], [])
        corrected_scatter.set_offsets(np.empty((0, 2)))
        current_point.set_offsets(np.empty((0, 2)))
        depth_text.set_text("")
        return raw_line, smooth_line, corrected_scatter, current_point, depth_text

    def update(frame):
        i = frame + 1
        raw_line.set_data(t[:i], cleaned[:i])
        smooth_line.set_data(t[:i], smoothed[:i])

        shown_bad = bad_idx[bad_idx < i]
        corrected_scatter.set_offsets(np.column_stack([t[shown_bad], cleaned[shown_bad]]))

        current_point.set_offsets([[t[i - 1], cleaned[i - 1]]])

        status = "\u26a0 CAUTION: shallow reading" if cleaned[i - 1] > danger_depth else "Depth nominal"
        depth_text.set_text(f"t = {t[i-1]:.0f}s\nDepth = {cleaned[i-1]:.1f} m\n{status}")

        return raw_line, smooth_line, corrected_scatter, current_point, depth_text

    ani = animation.FuncAnimation(
        fig, update, frames=len(t), init_func=init,
        interval=FRAME_INTERVAL_MS, blit=False, repeat=False
    )

    if SAVE_GIF:
        fps = max(1, int(1000 / FRAME_INTERVAL_MS))
        ani.save(GIF_PATH, writer=animation.PillowWriter(fps=fps))
        print(f"Saved animation to {GIF_PATH}")

    plt.tight_layout()
    plt.show()
    return ani


# --------------------------------------------------------------------------
if __name__ == "__main__":
    t, raw = load_data(CSV_PATH)
    cleaned, bad_mask = clean_data(raw)
    smoothed = smooth_data(cleaned)

    print(f"Loaded {len(t)} samples. Corrected {bad_mask.sum()} corrupted reading(s) "
          f"at t={t[bad_mask].astype(int).tolist()} s.")

    animate_depth(t, raw, cleaned, smoothed, bad_mask)

"""-------------------------------the end---------------------------------------------"""
