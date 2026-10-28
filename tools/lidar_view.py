#!/usr/bin/env python3
"""Live top-down plot of RPLIDAR scans.

Reads the output of the Slamtec SDK's ultra_simple program on stdin. From the
repo root:

    ./third_party/rplidar_sdk/output/Darwin/Release/ultra_simple \\
        --channel --serial /dev/cu.usbserial-110 460800 | python3 tools/lidar_view.py

Press r in the window to start recording, and r again to save it as a GIF in
media/. Stop with Ctrl-C in the terminal, or close the window.
"""

import argparse
import contextlib
import math
import os
import re
import select
import sys
import threading
import time
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_agg import FigureCanvasAgg
from matplotlib.figure import Figure
from PIL import Image

# One line per sample, e.g. "S  theta: 0.00 Dist: 00352.00 Q: 47".
# A leading S marks the first sample of a new rotation.
SAMPLE = re.compile(rb"(S)?\s+theta:\s*([\d.]+)\s+Dist:\s*([\d.]+)\s+Q:\s*(\d+)")

FIGSIZE = (8, 8)  # inches, 100 points each on screen; taller doesn't fit a laptop screen
GIF_DPI = 80      # GIF frames are FIGSIZE at this dpi, 640 x 640 px
MEDIA = Path(__file__).resolve().parent.parent / "media"

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
MUTED = "#52514e"
POINTS = "#2a78d6"
RECORDING = "#d03b3b"


class ScanParser:
    """Turns chunks of ultra_simple output into rotations of (x, y) points in meters.

    x is to the lidar's right and y is its front.
    """

    def __init__(self):
        self.partial = b""  # an unfinished line from the last chunk
        self.points = []    # the rotation being collected

    def feed(self, data):
        """Parses a chunk and returns the last rotation it completed, or None."""
        lines = (self.partial + data).split(b"\n")
        self.partial = lines.pop()
        done = None
        for line in lines:
            m = SAMPLE.match(line)
            if not m:
                continue  # startup banner, serial number, health status
            if m.group(1) and self.points:
                done, self.points = self.points, []
            theta, dist, quality = float(m.group(2)), float(m.group(3)), int(m.group(4))
            if quality == 0 or dist == 0:
                continue
            # theta is degrees clockwise from the front, so 90 is to the right.
            a = math.radians(theta)
            self.points.append((dist / 1000 * math.sin(a), dist / 1000 * math.cos(a)))
        return done

    def finish(self):
        """Returns the rotation still being collected when the input ends, or None."""
        return self.points or None


def make_plot(fig, view_range):
    """Draws the empty view on fig and returns (axes, scatter of the scan points)."""
    ax = fig.subplots()
    ax.set_facecolor(SURFACE)
    ax.set_xlim(-view_range, view_range)
    ax.set_ylim(-view_range, view_range)
    ax.set_aspect("equal")
    ax.set_xlabel("right (m)", color=MUTED)
    ax.set_ylabel("forward (m)", color=MUTED)
    ax.tick_params(colors=MUTED, length=0)
    ax.grid(color=MUTED, alpha=0.15, linewidth=0.5)
    for spine in ax.spines.values():
        spine.set_visible(False)
    ax.set_title("waiting for a full rotation...", color=TEXT)
    ax.plot(0, 0, marker="^", markersize=9, color=TEXT, zorder=3)  # the lidar, front up
    points = ax.scatter([], [], s=4, color=POINTS, linewidths=0)
    return ax, points


def save_gif(frames, view_range):
    """Renders recorded rotations offscreen and saves them as a GIF in media/.

    frames is a list of (time, points). Each frame lasts until the next rotation
    arrived, so the GIF plays back in real time.
    """
    if not frames:
        print("nothing recorded", file=sys.stderr)
        return
    MEDIA.mkdir(exist_ok=True)
    path = MEDIA / time.strftime("%Y-%m-%d-lidar-%H%M%S.gif")
    print(f"saving {len(frames)} frames to {path} ...", file=sys.stderr)
    fig = Figure(figsize=FIGSIZE, dpi=GIF_DPI, facecolor=SURFACE, layout="constrained")
    canvas = FigureCanvasAgg(fig)
    ax, points = make_plot(fig, view_range)
    canvas.draw()
    fig.set_layout_engine("none")  # the layout is the same every frame, so solve it once
    images = []
    for _, scan in frames:
        points.set_offsets(scan)
        ax.set_title(f"{len(scan)} points", color=TEXT)
        canvas.draw()
        rgb = Image.fromarray(np.asarray(canvas.buffer_rgba())).convert("RGB")
        images.append(rgb.convert("P", palette=Image.Palette.ADAPTIVE))
    times = [t for t, _ in frames]
    durations = [round(1000 * (b - a)) for a, b in zip(times, times[1:])] + [100]
    images[0].save(path, save_all=True, append_images=images[1:], duration=durations, loop=0)
    print(f"saved {path} ({path.stat().st_size / 1e6:.1f} MB)", file=sys.stderr)


def drain(fd, timeout):
    """Reads and discards stdin until it closes or timeout seconds pass."""
    deadline = time.monotonic() + timeout
    while (left := deadline - time.monotonic()) > 0:
        if select.select([fd], [], [], left)[0] and not os.read(fd, 1 << 16):
            return


def main():
    parser = argparse.ArgumentParser(
        description="Live top-down plot of RPLIDAR scans from ultra_simple on stdin.")
    parser.add_argument("--range", type=float, default=3.0, metavar="M",
                        help="show M meters around the lidar in each direction (default: 3)")
    args = parser.parse_args()
    if args.range <= 0:
        parser.error("--range must be positive")
    if sys.stdin.isatty():
        parser.error("no input; pipe ultra_simple's output into this script")

    plt.rcParams["keymap.home"] = [k for k in plt.rcParams["keymap.home"] if k != "r"]  # r records
    fd = sys.stdin.fileno()
    scans = ScanParser()
    fig = plt.figure(figsize=FIGSIZE, facecolor=SURFACE, layout="constrained")
    fig.canvas.manager.set_window_title("lidar_view")
    ax, points = make_plot(fig, args.range)
    rec = ax.text(0.02, 0.98, "", transform=ax.transAxes, ha="left", va="top",
                  color=RECORDING, fontweight="bold")
    recording = None  # (time, points) for each rotation shown since r was pressed

    # Reads whatever stdin has ready and redraws once per completed rotation.
    # If more than one rotation arrived since the last tick, only the newest is drawn.
    def update():
        scan, ended = None, False
        for _ in range(16):  # bounded so a burst of input can't freeze the window
            if not select.select([fd], [], [], 0)[0]:
                break
            data = os.read(fd, 1 << 16)
            if not data:
                ended = True
                scan = scans.finish() or scan
                break
            scan = scans.feed(data) or scan
        if scan:
            points.set_offsets(scan)
            ax.set_title(f"{len(scan)} points", color=TEXT)
            if recording is not None:
                recording.append((time.monotonic(), scan))
                rec.set_text(f"● REC {recording[-1][0] - recording[0][0]:.0f} s")
        if ended:
            timer.stop()
            ax.set_title(f"{ax.get_title()} (input ended)", color=TEXT)
        if scan or ended:
            fig.canvas.draw_idle()

    # r starts recording, and r again saves the recording as a GIF.
    def on_key(event):
        nonlocal recording
        if event.key != "r":
            return
        if recording is None:
            recording = []
            rec.set_text("● REC")
        else:
            # Rendering takes a few seconds, so save in the background and keep the view live.
            # Not a daemon thread, so quitting waits for the GIF to finish.
            threading.Thread(target=save_gif, args=(recording, args.range)).start()
            recording = None
            rec.set_text("")
        fig.canvas.draw_idle()

    fig.canvas.mpl_connect("key_press_event", on_key)
    timer = fig.canvas.new_timer(interval=20)
    timer.add_callback(update)
    timer.start()
    try:
        plt.show()
    except KeyboardInterrupt:
        # ultra_simple gets the same Ctrl-C and stops the motor before it exits.
        # Keep reading until it does, so a broken pipe doesn't kill it first.
        plt.close("all")
        with contextlib.suppress(KeyboardInterrupt):
            drain(fd, timeout=2)
    if recording:
        save_gif(recording, args.range)  # still recording when the viewer was stopped


if __name__ == "__main__":
    main()
