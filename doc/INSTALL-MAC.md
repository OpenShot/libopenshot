<!--
© OpenShot Studios, LLC

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Building libopenshot for macOS

These instructions describe how to build `libopenshot` (and its dependency
`libopenshot-audio`) from source on a modern macOS system, producing a
working `libopenshot.dylib` and Python 3 bindings (`_openshot.so`,
`openshot.py`) that can be imported by `openshot-qt`.

They have been verified on macOS 15 (Apple Silicon, `arm64`) with
AppleClang 17 and CMake 4.0.3. The same steps apply to Intel Macs; the
only difference is that Homebrew lives at `/usr/local` on Intel and at
`/opt/homebrew` on Apple Silicon. Every path used below is derived via
`brew --prefix`, so the commands are architecture-neutral.

## Supported platforms

* macOS 11 (Big Sur) or newer.
* Both Apple Silicon (`arm64`) and Intel (`x86_64`) are supported.
* Earlier macOS versions are not tested; in particular, macOS 10.14 or
  newer is required because AppleClang on older SDKs lacked pieces needed
  by Qt 5 / modern C++17.

## Build tools

Install Xcode's Command Line Tools (this is enough, the full Xcode IDE is
not required):

```sh
xcode-select --install
```

Install Homebrew from <https://brew.sh> if you do not already have it.
After installation, confirm that `brew` is on your `PATH`:

```sh
brew --version
brew --prefix        # prints /opt/homebrew on Apple Silicon, /usr/local on Intel
```

## Dependencies

The following libraries and tools are required. All of them are available
as Homebrew formulae.

* `cmake` (build system)
* `swig` (generates the Python bindings)
* `qt@5` (GUI/image building blocks used by the library; keg-only, so it
  must be referenced via `CMAKE_PREFIX_PATH`)
* `ffmpeg@7` (recommended today; the default `ffmpeg` formula is already
  at FFmpeg 8, which is not yet supported on the `develop` branch)
* `libopenshot-audio` (built from source, see below)
* `libomp` (OpenMP runtime for AppleClang, which does not ship OpenMP
  out of the box)
* `zeromq` (IPC/logging)
* `cppzmq` (C++ header-only wrappers for ZeroMQ; used by `ZmqLogger`)
* `pkgconf` (used by FFmpeg detection)

Optional but recommended for a more complete build:

* `imagemagick` (adds extra image decode/encode paths)
* `resvg` or `librsvg` (SVG rendering)
* `babl` (colorspace conversions used by some effects)
* `unittest-cpp` and/or `catch2` (required only to build and run the unit tests)

## Install dependencies

```sh
brew install \
    cmake swig qt@5 ffmpeg@7 libomp zeromq cppzmq pkgconf

# Optional but recommended:
brew install imagemagick resvg babl unittest-cpp catch2
```

`qt@5`, `ffmpeg@7`, and `libomp` are keg-only on Homebrew and are
therefore not symlinked into the default prefix. That is expected, and the
`cmake` commands below locate them explicitly.

## Obtain the source

`libopenshot` and `libopenshot-audio` are separate repositories. Clone
them as siblings of each other:

```sh
git clone https://github.com/OpenShot/libopenshot-audio.git
git clone https://github.com/OpenShot/libopenshot.git
```

The build commands below assume both repositories sit in the same parent
directory.

## Build libopenshot-audio

`libopenshot-audio` provides the audio engine, based on JUCE. `libopenshot`
will not configure without it.

```sh
cd libopenshot-audio
cmake -B build -S .
cmake --build build -j
```

A successful build produces `build/libopenshot-audio.<version>.dylib` and
`build/src/openshot-audio-demo`. You do not need to run
`cmake --install build`; the next step points `libopenshot` directly at
the `build/` directory via `OpenShotAudio_ROOT`.

## Build libopenshot

From the sibling `libopenshot` directory:

```sh
cd ../libopenshot

QT5_PREFIX="$(brew --prefix qt@5)"
FFMPEG_PREFIX="$(brew --prefix ffmpeg@7)"
LIBOMP_PREFIX="$(brew --prefix libomp)"

PKG_CONFIG_PATH="$FFMPEG_PREFIX/lib/pkgconfig" \
cmake -B build -S . \
    -DCMAKE_PREFIX_PATH="$QT5_PREFIX;$FFMPEG_PREFIX" \
    -DOpenShotAudio_ROOT="$PWD/../libopenshot-audio/build" \
    -DOpenMP_C_FLAGS="-Xpreprocessor -fopenmp -I$LIBOMP_PREFIX/include" \
    -DOpenMP_CXX_FLAGS="-Xpreprocessor -fopenmp -I$LIBOMP_PREFIX/include" \
    -DOpenMP_C_LIB_NAMES=omp \
    -DOpenMP_CXX_LIB_NAMES=omp \
    -DOpenMP_omp_LIBRARY="$LIBOMP_PREFIX/lib/libomp.dylib" \
    -DENABLE_RUBY=0

cmake --build build -j
```

Notes on the arguments above:

* `CMAKE_PREFIX_PATH` tells CMake where to find Qt 5 and FFmpeg, both of
  which are keg-only in Homebrew.
* `OpenShotAudio_ROOT` points at the uninstalled `libopenshot-audio` build
  tree.
* The five `OpenMP_*` flags are required because AppleClang does not ship
  with OpenMP; they tell CMake to use the Homebrew `libomp` via the
  `-Xpreprocessor -fopenmp` idiom.
* `ENABLE_RUBY=0` is required today. Apple's system Ruby headers in
  `/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/Ruby.framework`
  still declare APIs with the `register` storage-class specifier, which
  C++17 has removed, so attempting to build the Ruby bindings fails with
  `ISO C++17 does not allow 'register' storage class specifier`. The Python
  bindings are unaffected.

A successful build produces:

* `build/src/libopenshot.<version>.dylib`
* `build/bindings/python/_openshot.so`
* `build/bindings/python/openshot.py`
* `build/src/examples/openshot-example`, `openshot-html-example`,
  `openshot-player`

## Verify the build

Before touching your system Python, confirm the Python bindings import
directly from the build tree:

```sh
cd build/bindings/python
python3 -c "import openshot; print(openshot.OPENSHOT_VERSION_FULL)"
```

This should print the libopenshot version (for example, `0.7.0`). If it
does, the build is healthy.

## Install (optional)

To install into Homebrew's prefix so that `openshot-qt` can find the
bindings without manual `PYTHONPATH` tweaks:

```sh
cd /path/to/libopenshot
cmake --install build
```

The default `CMAKE_INSTALL_PREFIX` comes from CMake, which on macOS is
`/usr/local` regardless of the host architecture. You can override it
to match your Homebrew prefix:

```sh
cmake -B build -S . \
    -DCMAKE_INSTALL_PREFIX="$(brew --prefix)" \
    ...   # plus the other flags listed above
cmake --install build
```

## Running the tests (optional)

Unit tests use Catch2 (preferred) or UnitTest++.

```sh
brew install catch2      # or: brew install unittest-cpp
cmake -B build -S . -DENABLE_TESTS=1 ...    # plus the flags above
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Troubleshooting

### `ld: framework 'AGL' not found` when building libopenshot-audio

`AGL` was removed from the macOS SDK in macOS 10.14 (Mojave, 2018). This
reference was removed from `libopenshot-audio` in its 0.6.x series; if
you see this error, update to the latest `develop` branch of
`libopenshot-audio`.

### `zmq.hpp file not found`

`libopenshot` includes `<zmq.hpp>` (provided by the `cppzmq` Homebrew
formula) unconditionally, even though CMake currently marks `cppzmq` as
"optional". Install it:

```sh
brew install cppzmq
```

### `Could NOT find OpenMP_C` during configure

AppleClang does not ship an OpenMP runtime. Install `libomp` and pass the
five `OpenMP_*` hint flags shown in the build command above.

### `ISO C++17 does not allow 'register' storage class specifier` in Ruby wrappers

Apple's system Ruby headers have not yet been updated for C++17. Pass
`-DENABLE_RUBY=0` to disable the Ruby bindings. Python bindings are not
affected.

### `Could not find a package configuration file provided by "OpenShotAudio"`

`libopenshot`'s CMake cannot find `libopenshot-audio`. Either build it as
described above and pass `-DOpenShotAudio_ROOT=/path/to/libopenshot-audio/build`,
or install it into a directory listed in `CMAKE_PREFIX_PATH`.

### FFmpeg version mismatches

The current `develop` branch of `libopenshot` targets FFmpeg 7. Homebrew's
default `ffmpeg` formula has moved on to FFmpeg 8, which is not yet
supported. Install the pinned `ffmpeg@7` formula as shown above.

## Next steps

Once `libopenshot` builds and `import openshot` works, the graphical
editor `openshot-qt` can be run against this build. See
<https://github.com/OpenShot/openshot-qt> for the Qt/Python frontend.

To contribute patches, please read
<https://github.com/OpenShot/openshot-qt/wiki/Become-a-Developer>.
