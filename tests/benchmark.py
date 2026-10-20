import csv
import hashlib
import json
from pathlib import Path
import platform
import random
import re
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "report/inc"


def make_input(size, window=5, iterations=20):
    row = " ".join(str((i % 17) - 8) for i in range(size))
    kernel = " ".join([str(1 / (window * window))] * window)
    return (f"{size} {size}\n" + (row + "\n") * size + f"{window}\n"
            + (kernel + "\n") * window + f"{iterations}\n").encode()


def measure(data, threads):
    result = subprocess.run([str(ROOT / "convolution"), "--threads", str(threads)],
                            input=data, capture_output=True, timeout=120)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.decode())
    seconds = float(re.search(rb"elapsed_seconds=([0-9.]+)", result.stderr)[1])
    return seconds, hashlib.sha256(result.stdout).hexdigest()


def collect_measurements():
    records = []
    rng = random.Random(13)
    for size in (128, 512, 1024):
        data = make_input(size)
        reference_hash = None
        for repeat in range(6):
            order = [1, 2, 4, 8]
            rng.shuffle(order)
            for threads in order:
                seconds, checksum = measure(data, threads)
                if reference_hash is None:
                    reference_hash = checksum
                assert checksum == reference_hash, (size, threads, repeat)
                if repeat > 0:
                    records.append(dict(size=size, kernel=5, iterations=20,
                                        threads=threads, repeat=repeat,
                                        seconds=seconds, sha256=checksum))
        print(f"Benchmark {size}x{size}: warmup + 5 repeats, outputs match", flush=True)
    return records


def summarize(records):
    summary = []
    for size in (128, 512, 1024):
        baseline = statistics.median(r["seconds"] for r in records
                                     if r["size"] == size and r["threads"] == 1)
        for threads in (1, 2, 4, 8):
            samples = [r["seconds"] for r in records
                       if r["size"] == size and r["threads"] == threads]
            median = statistics.median(samples)
            summary.append(dict(size=size, threads=threads, seconds=median,
                                minimum=min(samples), maximum=max(samples),
                                speedup=baseline / median,
                                efficiency=baseline / median / threads))
    return summary


def save_measurements(records, summary):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with (OUTPUT / "benchmark.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=records[0])
        writer.writeheader()
        writer.writerows(records)
    (OUTPUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    metadata = dict(platform=platform.platform(),
                    cpu=subprocess.check_output(["lscpu"], text=True),
                    compiler=subprocess.check_output(["gcc", "--version"], text=True),
                    methodology="CLOCK_MONOTONIC in program; computation, scratch and thread management included; I/O excluded; one warmup per P per size, five shuffled repeats; median; identical SHA256 outputs")
    (OUTPUT / "machine.json").write_text(json.dumps(metadata, indent=2) + "\n")


def main():
    records = collect_measurements()
    summary = summarize(records)
    save_measurements(records, summary)
    for row in summary:
        print(f'{row["size"]:4d} P={row["threads"]} T={row["seconds"]:.6f} '
              f'S={row["speedup"]:.3f} E={row["efficiency"]:.3f}')


if __name__ == "__main__":
    main()
