import math
import os
from pathlib import Path
import pty
import random
import select
import subprocess
import sys
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / "convolution"


def problem(matrix, kernel, iterations):
    lines = [f"{len(matrix)} {len(matrix[0])}"]
    lines.extend(" ".join(map(str, row)) for row in matrix)
    lines.append(str(len(kernel)))
    lines.extend(" ".join(map(str, row)) for row in kernel)
    lines.append(str(iterations))
    return "\n".join(lines) + "\n"


def reference(matrix, kernel, iterations):
    rows, cols = len(matrix), len(matrix[0])
    center = len(kernel) // 2
    for _ in range(iterations):
        result = [[0.0] * cols for _ in range(rows)]
        for i in range(rows):
            for j in range(cols):
                for u, coefficients in enumerate(kernel):
                    for v, coefficient in enumerate(coefficients):
                        y, x = i + center - u, j + center - v
                        if 0 <= y < rows and 0 <= x < cols:
                            result[i][j] += matrix[y][x] * coefficient
        matrix = result
    return matrix


class FilterTests(unittest.TestCase):
    def run_program(self, text, threads=1, **kwargs):
        return subprocess.run([str(BINARY), "--threads", str(threads)],
                              input=text, text=True, capture_output=True,
                              timeout=20, **kwargs)

    def assert_filter(self, matrix, kernel, iterations, threads):
        result = self.run_program(problem(matrix, kernel, iterations), threads)
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = [[float(x) for x in row.split()] for row in result.stdout.splitlines()]
        expected = reference(matrix, kernel, iterations)
        self.assertEqual(len(actual), len(expected))
        for row, wanted in zip(actual, expected):
            self.assertEqual(len(row), len(wanted))
            for value, target in zip(row, wanted):
                self.assertTrue(math.isclose(value, target, rel_tol=1e-12, abs_tol=1e-12),
                                (value, target))
        self.assertIn(f"threads={min(threads, len(matrix))}\n", result.stderr)
        self.assertIn("elapsed_seconds=", result.stderr)

    def test_identity_and_many_passes(self):
        for threads in (1, 2, 20):
            with self.subTest(threads=threads):
                self.assert_filter([[1, 2, 3], [-4.5, 0, 6]], [[1]], 5, threads)

    def test_asymmetric_kernel_is_convolution(self):
        kernel = [[0, 1, 2], [-1, 0, 3], [4, -2, 1]]
        for threads in (1, 2, 4):
            with self.subTest(threads=threads):
                self.assert_filter([[1, 2, 3], [4, 5, 6], [7, 8, 9]], kernel, 2, threads)

    def test_subnormal_input_is_preserved(self):
        result = self.run_program(problem([[1e-320], [-1e-320]], [[1]], 1), 2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([float(x) for x in result.stdout.split()], [1e-320, -1e-320])

    def test_asymmetric_shift_has_known_answer(self):
        text = problem([[1, 2], [3, 4]], [[0, 0, 0], [1, 0, 0], [0, 0, 0]], 1)
        for threads in (1, 2):
            with self.subTest(threads=threads):
                result = self.run_program(text, threads)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, "2 0\n4 0\n")

    def test_kernel_larger_than_matrix(self):
        self.assert_filter([[2.5]], [[1] * 5 for _ in range(5)], 3, 8)
        self.assert_filter([[1, -1]], [[1] * 3 for _ in range(3)], 2, 2)

    def test_uneven_rows_and_fractional_values(self):
        rng = random.Random(13)
        matrix = [[rng.uniform(-2, 2) for _ in range(7)] for _ in range(5)]
        kernel = [[rng.uniform(-0.1, 0.1) for _ in range(3)] for _ in range(3)]
        for threads in (1, 2, 3, 4, 5, 100):
            with self.subTest(threads=threads):
                self.assert_filter(matrix, kernel, 3, threads)

    def test_sample(self):
        result = self.run_program((ROOT / "examples/input.txt").read_text(), 4)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, (ROOT / "examples/expected.txt").read_text())

    def test_default_threads_and_help(self):
        result = subprocess.run([str(BINARY)], input=problem([[1]], [[1]], 1),
                                text=True, capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("thread_limit=1 threads=1", result.stderr)
        help_result = subprocess.run([str(BINARY), "--help"], text=True,
                                     capture_output=True, timeout=5)
        self.assertEqual(help_result.returncode, 0)
        self.assertIn("--threads", help_result.stdout)

    def test_invalid_options(self):
        for arguments in (["--threads"], ["--threads", "0"], ["--threads", "-2"],
                          ["--threads", "1.5"], ["--threads", "1x"],
                          ["--threads", "9" * 40], ["--unknown"]):
            with self.subTest(arguments=arguments):
                result = subprocess.run([str(BINARY), *arguments], input="",
                                        text=True, capture_output=True, timeout=5)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")

    def test_invalid_data(self):
        good = problem([[1, 2]], [[1]], 1)
        cases = ["", "0 2\n", "-1 2\n", "1 2 extra\n", "1\n",
                 "18446744073709551615 2\n", "999999999999999999999 1\n",
                 good.replace("1 2\n1 2", "1 2\n1"),
                 good.replace("1 2\n1 2", "1 2\n1 2 3"),
                 "1 1\nnan\n", "1 1\ninf\n", "1 1\n1e999\n",
                 "1 1\n1,5\n", "1 1\n2\n2\n", "1 1\n2\n0\n",
                 "1 1\n2\n18446744073709551615\n", "1 1\n2\n3\n1 2\n",
                 "1 1\n2\n1\n1\n0\n", "1 1\n2\n1\n1\n-1\n"]
        for text in cases:
            with self.subTest(text=text):
                result = self.run_program(text, 4)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertTrue(result.stderr)

    def test_computation_overflow(self):
        for matrix, kernel, iterations in (([[1e308]], [[2]], 1),
                                           ([[1e154]] * 4, [[1e154]], 2)):
            result = self.run_program(problem(matrix, kernel, iterations), 4)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "")

    def test_output_failure(self):
        with open("/dev/full", "w") as output:
            result = subprocess.run([str(BINARY)], input=problem([[1]], [[1]], 1),
                                    text=True, stdout=output, stderr=subprocess.PIPE,
                                    timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Вывод матрицы", result.stderr)

    def test_interactive_retry(self):
        master, slave = pty.openpty()
        process = subprocess.Popen([str(BINARY), "--threads", "2"], stdin=slave,
                                   stdout=slave, stderr=slave, start_new_session=True)
        os.close(slave)
        transcript = b""
        try:
            os.write(master, b"bad\n2 2\nwrong\n1 2\n3 4\n2\n1\n1\n0\n2\n")
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                ready, _, _ = select.select([master], [], [], 0.1)
                if ready:
                    try:
                        chunk = os.read(master, 65536)
                    except OSError:
                        break
                    if not chunk:
                        break
                    transcript += chunk
                if process.poll() is not None and not ready:
                    break
            self.assertEqual(process.wait(timeout=3), 0, transcript.decode(errors="replace"))
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            os.close(master)
        text = transcript.decode()
        self.assertIn("Повторите строку", text)
        self.assertIn("нечётное", text)
        self.assertIn("Результат:\r\n1 2\r\n3 4\r\n", text)

    def test_thread_creation_and_join_failures(self):
        binary = ROOT / "convolution-fail"
        text = problem([[1] * 3 for _ in range(4)], [[1]], 3)
        for variable, number in (("LAB3_FAIL_CREATE", "1"),
                                 ("LAB3_FAIL_CREATE", "2"),
                                 ("LAB3_FAIL_JOIN", "1")):
            with self.subTest(variable=variable, number=number):
                result = subprocess.run([str(binary), "--threads", "4"],
                                        input=text, text=True, capture_output=True,
                                        env=dict(os.environ, **{variable: number}), timeout=5)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertTrue(result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
