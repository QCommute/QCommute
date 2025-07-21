# Intro

This program is designed to perform various operations with sumns of Pauli strings. 

For known Hamiltonian and observable, one can compute nested commutators:
$[H, ..., [H, O]]$
The commutators are saved on-disk in a space-efficient way. The computation is parallelized, and can be adapted to work under memory constraints.

It is also possible to compute _moments_ and _quench coefficients_ of Pauli string sums.

This program was created for the article "Quench dynamics via recursion method and dynamical quantum phase transitions", available [here](http://arxiv.org/abs/2503.24362).

# Building

The only external dependency necessary is gmp.

We use the Meson build system. To download the program and compile it, run

```
git clone https://gitlab.com/viacheslav_hrushev/commutators
cd commutators
meson setup --buildtype=release build-release
cd build-release
meson compile
```

This should produce the program binary, ```main```.

# Usage

For detailed information on how to invoke the program to perform various tasks, consult the built-in help message (```./main --help```).

# License

This program is distributed under ??? license.
