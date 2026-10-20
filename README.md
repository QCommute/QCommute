# QCommute — Nested Commutator Engine for Pauli Strings

QCommute computes arbitrarily deep nested commutators of the form

$$[H,\,[H,\,\ldots[H,\,O]\ldots]]$$

where **H** (Hamiltonian) and **O** (observable) are sums of Pauli strings on a
1-D, 2-D, or 3-D lattice. It can also compute **moments** and **quench
coefficients** of the resulting operator series, including diskless in-memory
statistics for the $(n+1)$-th layer and extended quench sequences for polarized
initial states.

The computation is:
- **exact** — coefficients are multivariate polynomials in the coupling
  constants with arbitrary-precision integer entries (GNU MP);
- **parallelised** — all available CPU cores are used simultaneously;
- **disk-backed & sharded** — intermediate results are streamed to disk in
  automatically sharded subdirectories so that multi-terabyte computations
  exceeding physical RAM and filesystem directory limits remain feasible.

### Publications & Citation

If you use QCommute in your research, please cite:

1. **O. Lychkovskiy, V. Khrushchev, and I. Shirokov**,  
   *"QCommute: A tool for symbolic computation of nested commutators in quantum many-body spin-1/2 systems"*,  
   [SciPost Phys. Codebases **84**, (2026)](https://doi.org/10.21468/SciPostPhysCodeb.84) ([arXiv:2604.04778](https://doi.org/10.48550/arXiv.2604.04778)).
2. **I. Shirokov, V. Khrushchev, F. Uskov, I. Dudinets, I. Ermakov, and O. Lychkovskiy**,  
   *"Recursion method for quench dynamics: Strengths and limitations"*,  
   [SciPost Phys. Core **9**, 049 (2026)](https://doi.org/10.21468/SciPostPhysCore.9.3.049) ([arXiv:2503.24362](https://doi.org/10.48550/arXiv.2503.24362)).

---

## Requirements

| Tool | Minimum version |
|------|----------------|
| C++ compiler | C++20 (GCC ≥ 10, Clang ≥ 13) |
| [Meson](https://mesonbuild.com/) | 0.56 |
| [Ninja](https://ninja-build.org/) | any |
| [GMP](https://gmplib.org/) | 6.x |

Install GMP on Ubuntu/Debian:

```sh
sudo apt install libgmp-dev

## Building

```sh
git clone https://github.com/QCommute/QCommute.git
cd QCommute

# Release build (optimised, recommended for production runs)
meson setup --buildtype=release build-release
ninja -C build-release

# Debug build (adds -DDEBUG for verbose per-window diagnostic output)
meson setup --buildtype=debug -Ddebug_output=true build-debug
ninja -C build-debug
```

Both commands produce the binary `build-release/main` (or `build-debug/main`).

After any source change, rebuilding is as simple as:

```sh
ninja -C build-release
```

Ninja detects which translation units changed and recompiles only those.

---

## Quick Start

### 1. Define the problem

Create a plain-text input file.  The format is:

```
H:
<Pauli string>
...
O:
<Pauli string>
...
n:
<number of commutators>
```

Each Pauli string is written as a product of factors; each factor is either a
polynomial coefficient `[c0,d0,d1,...]` or a single-site Pauli operator
`Sx(i)`, `Sy(i)`, `Sz(i)` (1-D) or `Sx(i,j)`, … (2-D / 3-D).

The example file `examples/Comm.txt` defines the **2-D transverse-field Ising
model** on a square lattice with coupling constant `h0`:

```
H:
[1]*Sx(1,1)*Sx(2,1)
[1]*Sx(1,1)*Sx(1,2)
[1,1]*Sz(1,1)
O:
[1]*Sz(1,1)
n:
23
```

Here `[1,1]` means the monomial `h₀¹` (one factor of h₀), while `[1]` is the
constant 1.

### 2. Compute commutators

```sh
./build-release/main examples/Comm.txt \
    --memory-limit 8192 \
    --res-dir ./results
```

Progress is written to **stderr**; the result directory (`./results/`) contains
one sub-directory per computed commutator level.

### 3. Compute moments

```sh
./build-release/main momentum ./results
```

Output is a Mathematica-ready list of polynomials in `h0`, `h1`, … printed to
**stdout**.

### 4. Compute quench coefficients

```sh
./build-release/main quench ./results
```

Output is a Mathematica-ready list of polynomials in `px`, `py`, `pz`, `h0`,
`h1`, … (where `px`, `py`, `pz` track the number of σˣ, σʸ, σᶻ operators).

---

## Advanced Workflows (v1.1)

### Resuming from an existing layer (`--start-from-layer`)

If a run was interrupted or you want to increase `n` in the input file without
recomputing layers `1..K`, use `--start-from-layer K`:

```sh
./build-release/main examples/Comm.txt \
    --res-dir ./results \
    --memory-limit 950000 \
    --start-from-layer 23

```

Starting with v1.1, each completed layer saves its metadata to
`<layer>/layer_meta.txt`. For legacy directories created with v1.0, QCommute
will automatically scan layer `K` in parallel, or you can supply the metadata
from a previous log directly via `--layer-meta <O_size> <tableSize> <expRatio> <avgBytesPerHash>`.

### Computing $(n+1)$-th layer statistics without disk storage (`next-layer-stats`)

When layer $n+1$ is too large to store on disk (e.g. requiring >20 TB), you can
compute its exact moment and quench polynomials directly in RAM window-by-window:

```sh
./build-release/main next-layer-stats examples/Comm.txt ./results 24 \
    --memory-limit 950000 > stats_layer25.txt

```

Because identical Pauli strings always hash to the same bucket, all like terms
within each hash window are completely combined in memory before accumulating
their squared polynomials. Lightweight binary checkpoints (`window_<W>.chk`)
are saved in `./results/stats_layer_25/` after every window so interrupted runs
resume automatically.

### Extending quench coefficients for a $z$-polarized state (`quench-z`)

For an initial product state polarized along the $z$-axis ($p_x = p_y = 0$),
Pauli strings with too many $\sigma^x$ or $\sigma^y$ operators at layer
$n_{\max} + k$ cannot contribute to the expectation value within the remaining
$m - k$ steps. The `quench-z` sub-command exploits this elimination rule to
compute $m$ additional quench coefficients beyond $n_{\max}$:

```sh
./build-release/main quench-z examples/Comm.txt ./results 24 6 \
    --memory-limit 900000 > quench_z_25_to_30.txt

```

### Recovering missing blocks in a layer (`recover-layer`)

If a layer directory has missing `.comm` block files (e.g. due to transient
filesystem errors on an unsharded v1.0 directory), `recover-layer` identifies
the missing blocks and recomputes **only** those blocks in a single pass over
the previous layer:

```sh
./build-release/main recover-layer examples/Comm.txt ./results 24 16635286208 48267883030

```

---

## Command-Line Reference

```
main [FILE] [OPTION]...                                             Compute nested commutators
main momentum DIRECTORY [LAYER]                                     Compute moments (all layers or a single layer)
main quench   DIRECTORY [LAYER]                                     Compute quench coefficients (all or single layer)
main next-layer-stats FILE RES_DIR BASE_LAYER [OPTION]...           Compute layer (BASE_LAYER+1) stats in RAM
main quench-z FILE RES_DIR N_MAX M [--memory-limit N] [--keep-temp] Compute M extra z-polarized quench coefficients
main recover-layer FILE RES_DIR LAYER PREV_TABLE_SIZE CUR_TABLE_SIZE Recompute missing .comm blocks in LAYER
main validate FILE1 FILE2                                           Check that two Pauli-string files match
main --help                                                         Print help message and exit
main --version                                                      Print version and licence information

```

### Options for the commutator computation

| Option | Default | Description |
| --- | --- | --- |
| `--memory-limit N` | unlimited | Cap total RAM usage at N megabytes. When the working set exceeds this limit the computation is automatically split into disk-backed windows. |
| `--res-dir DIRECTORY` | `./tmp` | Directory where commutator files are stored. |
| `--start-from-layer K` | `0` | Resume computation starting from an already completed layer `K`. |
| `--layer-meta O T E B` | auto | Explicitly provide `O_size`, `tableSize`, `expansionRatio`, and `avgBytesPerHash` when resuming from a v1.0 layer without `layer_meta.txt`. |
| `--force` | off | Delete the contents of `--res-dir` (or incomplete target layer when resuming) before starting. |
| `--text-output` | off | After computing, print the final commutator in human-readable form to stdout. Slow for large results. |

### Enabling diagnostic output

```sh
meson setup --buildtype=debug -Ddebug_output=true build-debug
ninja -C build-debug
./build-debug/main examples/Comm.txt --memory-limit 8192 --res-dir ./results

```

Each computation window then prints `[DIAG]` lines with the slot range,
estimated memory usage, and the adaptive bytes-per-hash value.

---

## Repository Layout

```
QCommute/
├── main.cpp                 Entry point; CLI sub-commands and window management
├── Expression.h / .cpp      Pauli-string term: coefficient × i^k × ∏ σ_a(r)
├── Poly.h / .cpp            Multivariate polynomial (compact packed encoding)
├── Poly2.h / .cpp           Sparse polynomial (map-based; used for statistics)
├── BigInt.h / .cpp          Arbitrary-precision integer (GMP wrapper)
├── Sigma.h                  Lattice-site Pauli operator (template over D=1,2,3)
├── SerializationHelpers.h / .cpp  Low-level byte-buffer I/O utilities
├── examples/
│   └── Comm.txt             2-D transverse-field Ising model example
├── meson.build              Build definition
└── meson_options.txt        Build options (e.g. debug_output)

```

---

## How It Works

1. **Input parsing** — H and O are read from the input file as lists of
`Expression` objects.
2. **Layer 0** — O is written to disk in blocks of 2048 terms
(`BLOCK_SIZE`), sorted and deduplicated by Pauli string.
3. **Layer k → k+1** — For every pair (H term, layer-k term), the commutator
is computed symbolically and the resulting terms are inserted into an
in-memory hash table, keyed by Pauli string. When two terms share the same
Pauli string their polynomial coefficients are added. The hash table is
then flushed to disk (automatically sharded into `shard_<N>/` subdirectories
when the layer exceeds 4 million block files).
4. **Memory management** — If `--memory-limit` is set, the hash table for
layer k+1 is partitioned into *windows* of consecutive hash-bucket ranges.
Each window is computed by a separate child process (via `fork`), so the OS
reclaims memory automatically between windows.
5. **Statistics** — The `momentum` and `quench` sub-commands read stored
layers in parallel and accumulate the results into a `Poly2` polynomial,
which is then printed in Mathematica syntax.

---

## Contributing

Contributions are welcome! Please open an issue or pull request on GitHub.

### Building and running tests

The `validate` sub-command can be used to compare two human-readable
Pauli-string files:

```sh
./build-release/main validate reference.txt output.txt

```

### Code style

* C++20, formatted roughly following the existing style (2-space indent, K&R
braces for functions).
* All user-visible strings and comments must be in **English**.
* New public functions and classes should carry a Doxygen `/** … */` comment
in the header explaining purpose, parameters, and any invariants.

---

## Licence

Copyright © 2025–2026 Ilya Shirokov, Viacheslav Khrushchev.

Licensed under **Creative Commons Attribution 4.0 International**
(CC BY 4.0). You are free to copy and redistribute the material for any
purpose, even commercially, provided you give appropriate credit.

See [LICENSE](https://www.google.com/search?q=LICENSE) or [https://creativecommons.org/licenses/by/4.0/](https://creativecommons.org/licenses/by/4.0/) for
the full licence text.
