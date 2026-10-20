# Implementation contract

Target: Linux/WSL, C11 + POSIX pthreads, one local interactive user.

Input: --threads P (default 1, counts main). Console asks rows/columns, matrix rows, odd kernel size, kernel rows, positive K. Prompts go to stderr only in a terminal. Piped/file input uses the same whitespace token format. Invalid interactive rows are retried; EOF exits with an error. Results go to stdout, timings and effective thread count to stderr. --help documents everything.

Math: output[i,j] = sum(kernel[u,v] * source[i + center - u, j + center - v]); out-of-bounds source is zero. Each pass reads immutable source and writes disjoint complete rows of destination. A scratch matrix ping-pongs with input. Finite input required; non-finite computed results cause failure before result output.

Decomposition (user-approved simplification): four ordinary C modules. matrix.c/.h owns Matrix, safe allocation and row convolution; filter.c/.h owns pthread row scheduling, joins and K-pass buffer swapping; console.c/.h owns input and output; main.c owns options, timing and cleanup. No architectural layer directories, DI callback or generic scheduler.

Threading: P=min(requested limit, rows). Allocate P-1 thread handles/jobs per pass; these allocations are included in filtering time. For each pass start workers on static ranges, main computes its range, then join all started workers before buffer swap. If creation fails, join started workers and return failure without freeing in-use data. A failed join is fatal to the process because ownership cannot be safely released. No barrier, no worker pool, no locks: join is the phase boundary. Creating per pass is a deliberate simple choice; performance measurements include that overhead.

Verification: Python standard-library reference convolution including asymmetric kernels, 1x1, larger-than-matrix kernels, negative/fractional values, multiple passes, rows not divisible by thread count, cap above rows, bad input and malformed options. Assert sequential and parallel outputs against reference. ASan/UBSan checks if available. Capture ps -L while a sufficiently long process runs and strace thread creation/exit; report peak live application threads. Benchmark 1/2/4/8 threads on several matrix sizes with deterministic equal input, warmup and repeated interleaved runs; speedup T1/Tp and efficiency speedup/P. Report actual measurements, no assumed scaling.

Deliver report with the real assignment, algorithm, boundaries, module responsibilities, thread cap evidence, benchmark table/plot and honest conclusions; use the existing lab report typography and compile with XeLaTeX. Render and inspect pages. Final archive must match current sources and PDF and exclude build/render junk.

Plan review revision: validate all dimension products and byte sizes before allocation; total allocated element bytes <= PTRDIFF_MAX guarantees signed coordinate arithmetic stays representable. Cast coordinates before center subtraction. Convolution detects non-finite sums in each worker; join collects every worker result before swapping buffers. getline consumes a complete bad row before retry. Scratch/thread allocation failures return errors; join failure exits the process immediately.

Final user constraints: no goto; all C/Python functions <=40 physical lines, main <=20. main owns matrices and calls input and timed processing; read_problem owns the input line buffer. Final benchmark: K=20, five measured shuffled repeats after warmup.
