# QCommute — Nested Commutator Engine for Pauli Strings

QCommute computes arbitrarily deep nested commutators of the form

$$[H,\,[H,\,\ldots[H,\,O]\ldots]]$$

where **H** (Hamiltonian) and **O** (observable) are sums of Pauli strings on a
1-D, 2-D, or 3-D lattice.  It can also compute **moments** and **quench
coefficients** of the resulting operator series.

The computation is:
- **exact** — coefficients are multivariate polynomials in the coupling
  constants with arbitrary-precision integer entries (GNU MP);
- **parallelised** — all available CPU cores are used simultaneously;
- **disk-backed** — intermediate results are streamed to disk so that
  computations exceeding physical RAM remain feasible.

This program was developed for the article  
*"Quench dynamics via recursion method and dynamical quantum phase transitions"*,
available at <http://arxiv.org/abs/2503.24362>.

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
```

---

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

## Command-Line Reference

```
main [FILE] [OPTION]...          Compute nested commutators
main momentum DIRECTORY          Compute moments of stored commutators
main quench   DIRECTORY          Compute quench coefficients
main validate FILE1 FILE2        Check that two Pauli-string files match
main --help                      Print this message and exit
main --version                   Print version and licence information
```

### Options for the commutator computation

| Option | Default | Description |
|--------|---------|-------------|
| `--memory-limit N` | unlimited | Cap total RAM usage at N megabytes.  When the working set exceeds this limit the computation is automatically split into disk-backed windows. |
| `--res-dir DIRECTORY` | `./tmp` | Directory where commutator files are stored. |
| `--force` | off | Delete the contents of `--res-dir` before starting.  Without this flag the program refuses to overwrite an existing result. |
| `--text-output` | off | After computing, print the final commutator in human-readable form to stdout.  Slow for large results. |

### Enabling diagnostic output

```sh
# Build with -DDEBUG, then run normally:
meson setup --buildtype=debug -Ddebug_output=true build-debug
ninja -C build-debug
./build-debug/main examples/Comm.txt --memory-limit 8192 --res-dir ./results
```

Each computation window then prints `[DIAG]` lines with the slot range,
estimated memory usage, and the adaptive bytes-per-hash value.  These are
useful for tuning `--memory-limit` and for debugging unexpected behaviour.

---

## Repository Layout

```
QCommute/
├── main.cpp                 Entry point; argument parsing and top-level loop
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
   in-memory hash table, keyed by Pauli string.  When two terms share the same
   Pauli string their polynomial coefficients are added.  The hash table is
   then flushed to disk.
4. **Memory management** — If `--memory-limit` is set, the hash table for
   layer k+1 is partitioned into *windows* of consecutive hash-bucket ranges.
   Each window is computed by a separate child process (via `fork`), so the OS
   reclaims memory automatically between windows.
5. **Statistics** — The `momentum` and `quench` sub-commands read all stored
   layers in parallel (one thread per file block) and accumulate the results
   into a `Poly2` polynomial, which is then printed in Mathematica syntax.

---

## Contributing

Contributions are welcome!  Please open an issue or pull request on GitHub.

### Building and running tests

There is currently no automated test suite.  The `validate` sub-command can be
used to compare two human-readable Pauli-string files:

```sh
./build-release/main validate reference.txt output.txt
```

### Code style

- C++20, formatted roughly following the existing style (2-space indent, K&R
  braces for functions).
- All user-visible strings and comments must be in **English**.
- New public functions and classes should carry a Doxygen `/** … */` comment
  in the header explaining purpose, parameters, and any invariants.

---

## Licence

Copyright © 2026 Ilya Shirokov, Viacheslav Khrushchev.

Licensed under **Creative Commons Attribution 4.0 International**
(CC BY 4.0).  You are free to copy and redistribute the material for any
purpose, even commercially, provided you give appropriate credit.

See [LICENSE](LICENSE) or <https://creativecommons.org/licenses/by/4.0/> for
the full licence text.
