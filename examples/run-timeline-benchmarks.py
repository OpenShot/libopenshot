#!/usr/bin/env python3
"""Run comparable CPU/GPU timeline workloads sequentially and retain raw evidence.

Example (inherit the Vulkan ICD and FFmpeg library environment):
  python3 examples/run-timeline-benchmarks.py --cpu-build build --vulkan-build build-vk \
      --video /path/to/4k.mp4 --output /tmp/timeline-results.json
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import statistics
import subprocess
import sys

RESULT = re.compile(
    r"frames=(\d+) layers=(\d+) readback=(\d+) deferred_frames=(\d+) "
    r"seconds=([\d.eE+-]+) fps=([\d.eE+-]+)"
)


def capture(command, timeout=20):
    try:
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=timeout)
        return {"command": command, "returncode": result.returncode, "output": result.stdout}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"command": command, "error": str(error)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpu-build", required=True, type=Path)
    parser.add_argument("--vulkan-build", required=True, type=Path)
    parser.add_argument("--video", required=True, type=Path)
    parser.add_argument("--frames", default=120, type=int)
    parser.add_argument("--runs", default=3, type=int)
    parser.add_argument("--timeout", default=180, type=int)
    parser.add_argument("--output", default=Path("timeline-results.json"), type=Path)
    parser.add_argument("--note", default="", help="Record background services or other measurement conditions")
    parser.add_argument("--verify-images", action="store_true", help="Save final-frame PNGs and compare CPU/GPU pixels (requires Pillow)")
    args = parser.parse_args()
    if min(args.frames, args.runs, args.timeout) < 1:
        parser.error("frames, runs and timeout must be positive")
    video = args.video.resolve(strict=True)
    binaries = {name: (directory / "examples/openshot-timeline-benchmark").resolve(strict=True)
                for name, directory in (("cpu", args.cpu_build), ("vulkan", args.vulkan_build))}
    output = args.output.resolve()
    if output == video or output in binaries.values():
        parser.error("output must not overwrite an input file")
    repo = Path(__file__).resolve().parent.parent
    report = {
        "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "machine": {"platform": platform.platform(), "processor": platform.processor(),
                    "logical_cpus": os.cpu_count()},
        "video": {"path": str(video), "bytes": video.stat().st_size,
                  "mtime_ns": video.stat().st_mtime_ns},
        "frames": args.frames, "runs": args.runs, "measurement_note": args.note,
        "environment": {name: os.environ[name] for name in (
            "VK_ICD_FILENAMES", "VK_DRIVER_FILES", "LD_LIBRARY_PATH", "QT_QPA_PLATFORM",
            "OMP_NUM_THREADS") if name in os.environ},
        "revision": capture(["git", "-C", str(repo), "rev-parse", "HEAD"]),
        "working_tree": capture(["git", "-C", str(repo), "status", "--short"]),
        "binaries": {}, "measurements": [], "summary": [],
        "notes": "Each trial is a fresh process; includes initialization. Run without other GPU/CPU workloads. "
                 "Readback=0 excludes presentation; readback=1 includes QImage materialization, not Qt display. "
                 "Compare library versions below before attributing differences to the backend."
    }
    for name, binary in binaries.items():
        report["binaries"][name] = {"path": str(binary),
            "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
            "libraries": capture(["ldd", str(binary)])}
    for name, command in (("gpu", ["nvidia-smi", "--query-gpu=name,driver_version,memory.total,memory.used,utilization.gpu", "--format=csv,noheader"]),
                          ("source", ["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(video)])):
        if shutil.which(command[0]):
            report[name] = capture(command)

    if shutil.which("nvidia-smi"):
        report["gpu_processes"] = capture(["nvidia-smi", "--query-compute-apps=pid,process_name,used_gpu_memory", "--format=csv,noheader"])

    def save():
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2) + "\n")

    if args.verify_images:
        from PIL import Image, ImageChops, ImageStat
        image_dir = output.parent / (output.stem + "-frames")
        image_dir.mkdir(parents=True, exist_ok=True)
    cpu_images = {}
    failed = False
    for layers in (1, 2):
        for readback in (0, 1):
            for backend, binary in binaries.items():
                successes = []
                for trial in range(1, args.runs + 1):
                    command = [str(binary), str(video), str(args.frames), str(layers), str(readback)]
                    image_path = None
                    if args.verify_images:
                        image_path = image_dir / f"{backend}-layers{layers}-readback{readback}-run{trial}.png"
                        command.append(str(image_path))
                    evidence = capture(command, args.timeout)
                    row = {"backend": backend, "layers": layers, "readback": readback,
                           "trial": trial, **evidence}
                    match = RESULT.search(evidence.get("output", ""))
                    if evidence.get("returncode") == 0 and match:
                        frames, got_layers, got_readback, deferred, seconds, fps = match.groups()
                        row.update(frames=int(frames), deferred_frames=int(deferred),
                                   seconds=float(seconds), fps=float(fps))
                        row["valid"] = (int(frames) == args.frames and int(got_layers) == layers
                                        and int(got_readback) == readback and float(seconds) > 0
                                        and (backend != "vulkan" or int(deferred) == args.frames))
                        if not row["valid"]:
                            row["error"] = "Incomplete workload or Vulkan CPU fallback detected"
                    else:
                        row["valid"] = False
                    if row["valid"] and image_path:
                        try:
                            with Image.open(image_path) as loaded:
                                pixels = loaded.convert("RGB")
                            means = ImageStat.Stat(pixels).mean
                            row["image"] = {"path": str(image_path), "size": pixels.size, "mean_rgb": means}
                            if backend == "cpu":
                                cpu_images[(layers, readback)] = pixels
                            else:
                                reference = cpu_images[(layers, readback)]
                                if pixels.size != reference.size:
                                    raise ValueError("GPU image dimensions differ from CPU")
                                row["image"]["cpu_mae_rgb"] = ImageStat.Stat(ImageChops.difference(pixels, reference)).mean
                                reference_means = ImageStat.Stat(reference).mean
                                if sum(reference_means) > 3 and sum(means) < 0.1 * sum(reference_means):
                                    raise ValueError("GPU image is black/near-black while matching CPU image is not")
                        except (OSError, ValueError) as error:
                            row["valid"] = False
                            row["error"] = str(error)
                    failed |= not row["valid"]
                    if row["valid"]:
                        successes.append(row["fps"])
                    report["measurements"].append(row)
                    print(f"{backend} layers={layers} readback={readback} trial={trial}: "
                          f"{row.get('fps', 'no result')} FPS valid={row['valid']}", flush=True)
                    save()
                report["summary"].append({"backend": backend, "layers": layers, "readback": readback,
                    "valid_runs": len(successes),
                    "median_fps": statistics.median(successes) if successes else None,
                    "min_fps": min(successes) if successes else None,
                    "max_fps": max(successes) if successes else None})
    report["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    report["valid"] = not failed
    save()
    print(f"Results: {output}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
