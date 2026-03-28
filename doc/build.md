# Building Spider

Spider is built using [CMake](https://cmake.org/).
It requires [ITK](https://itk.org/).
If your C++ standard library is libc++, you may also need Howard
Hinnant's [date library](https://github.com/HowardHinnant/date) with
timezone support.

Building Spider's tests additionally requires
[GoogleTest](https://google.github.io/googletest/) and passing CMake
the flag `-DBUILD_TESTING=ON`.
The tests can then be ran using `ctest`.

## Using Guix

The code in `guix.scm` in the repository root evaluates to a [GNU
Guix](https://guix.gnu.org) package definition for Spider.
Running the following command starts an interactive shell with Spider
available:

```sh
guix time-machine -C channels.scm -- shell -f guix.scm
```
