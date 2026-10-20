import json
from pathlib import Path
import re
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "report/inc"


def input_data(rows, cols, kernel, iterations):
    return (f"{rows} {cols}\n" + (" ".join(["1"] * cols) + "\n") * rows
            + f"{kernel}\n" + (" ".join([str(1 / kernel**2)] * kernel) + "\n") * kernel
            + f"{iterations}\n").encode()


def observe(rows, cols, kernel, iterations, limit):
    with tempfile.TemporaryFile() as source, tempfile.TemporaryFile() as diagnostic:
        source.write(input_data(rows, cols, kernel, iterations))
        source.seek(0)
        process = subprocess.Popen([str(ROOT / "convolution"), "--threads", str(limit)],
                                   stdin=source, stdout=subprocess.DEVNULL, stderr=diagnostic)
        peak = 0
        snapshot = ""
        deadline = time.monotonic() + 60
        try:
            while process.poll() is None:
                if time.monotonic() > deadline:
                    raise TimeoutError("Thread observation timed out")
                result = subprocess.run(["ps", "-L", "-p", str(process.pid),
                                         "-o", "pid,tid,nlwp,stat,comm"],
                                        capture_output=True, text=True, timeout=5)
                lines = result.stdout.strip().splitlines()[1:]
                count = len(lines)
                assert count <= min(limit, rows), result.stdout
                if count > peak:
                    peak, snapshot = count, result.stdout
                time.sleep(0.005)
            assert process.returncode == 0
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
        assert peak == min(limit, rows), (peak, limit, rows)
        diagnostic.seek(0)
        return dict(rows=rows, cols=cols, limit=limit, peak=peak,
                    ps=snapshot, diagnostic=diagnostic.read().decode())


def main():
    results = [observe(512, 512, 7, 12, limit) for limit in (1, 2, 4)]
    results.append(observe(2, 262144, 7, 20, 8))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / "threads.json").write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        print(f'rows={result["rows"]} requested={result["limit"]} peak={result["peak"]}')
        print(result["ps"], end="")
    trace = OUTPUT / "threads-trace.txt"
    result = subprocess.run(["strace", "-f", "-e", "trace=clone,clone3,exit,exit_group",
                             "-o", str(trace), str(ROOT / "convolution"), "--threads", "4"],
                            input=input_data(8, 8, 3, 2), stdout=subprocess.DEVNULL,
                            stderr=subprocess.PIPE, timeout=15)
    assert result.returncode == 0, result.stderr
    text = trace.read_text()
    created = len(re.findall(r"(?:clone|clone3)\(.*= [1-9][0-9]*$", text, re.MULTILINE))
    created += len(re.findall(r"<\.\.\. (?:clone|clone3) resumed>.*= [1-9][0-9]*$", text, re.MULTILINE))
    assert created == 6, (created, text)
    print("strace: 6 successful worker creations for 2 passes at P=4")


if __name__ == "__main__":
    main()
