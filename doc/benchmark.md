# Running the benchmarks

This document describes how to build and run the Spider benchmarks.

## Unix-like operating systems

On a Unix-like system, build Spider with the CMake flag
`-DSPIDER_BUILD_BENCHMARKS=ON`, then run `benchmark/run.sh` in the
build directory.

This script requires the external programs:

- [dcm2niix](https://www.nitrc.org/plugins/mwiki/index.php/dcm2nii:MainPage)
- [elastix](https://elastix.dev/)
- [gnuplot](https://sourceforge.net/projects/gnuplot/)
- [docutils](https://docutils.sourceforge.io/) (optional)

## Using [GNU Guix](https://guix.gnu.org)

In a checkout of this repository, run

```sh
guix build -f benchmark/guix.scm
```
