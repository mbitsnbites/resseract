![Resseract](/doc/logo.jpg "Resseract")

Resseract is a first-person shooter game focused on instagib deathmatch and capture-the-flag
gameplay as well as cooperative in-game map editing.

It is a fork of [Tesseract](http://tesseract.gg/), which in turn is a fork of
[Cube 2: Sauerbraten](http://sauerbraten.org/).

# About

## Documentation

In anticipation of proper Resseract documentation, please refer to the following relevant
(but possibly dated) information:

* [Game manual](http://sauerbraten.org/README.html) (from Cube 2: Sauerbraten)
* [Tesseract introduction](doc/Tesseract.md)
* [Tesseract rendering engine](doc/Renderer.md)

## Motivation

This fork aims to be more user friendly and game-like than its predecessor (Tesseract), which is
quite technically oriented.

Furthermore, the code base is being cleaned up. For example:
* Use CMake instead of Make/VS/XCode.
* Use a more consistent and readable coding style (with the help of [clang-format](https://clang.llvm.org/docs/ClangFormat.html)).
* Only the source code is included in the repo (no binaries).
* Data files are kept in a [separate repository](https://github.com/mbitsnbites/resseract-data).
* The main documentation has been converted to Markdown.

# Build instructions

1. Clone this repository:

```bash
git clone https://github.com/mbitsnbites/resseract.git
cd resseract
git submodule update --init --recursive
```

2. Build the game and tools according to the instructions below.

3. Run the game from the build folder:

```bash
./runclient.sh
```


## Linux

### Prerequisites

* A C++ compiler (typically [GCC](https://gcc.gnu.org/) or [clang](http://clang.llvm.org/))
* [CMake](https://cmake.org/)
* [ninja](https://ninja-build.org/)
* [SDL2](https://www.libsdl.org) developer files
* [OpenGL](https://www.opengl.org/) developer files

Ubuntu users can install:

```bash
sudo apt install build-essential cmake ninja-build xorg-dev libgl1-mesa-dev libsdl2-mixer-dev libsdl2-image-dev
```

### Build

Create a build-directory, preferrably in a different directory than the Resseract source
(out-of-tree), and run CMake and ninja:

```bash
mkdir resseract-build
cd resseract-build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release path/to/resseract/src
ninja
```

## macOS (experimental)

### Prerequisites

* [Xcode Command Line Tools](http://railsapps.github.io/xcode-command-line-tools.html) (or Xcode).
* Using [Homebrew](https://brew.sh/), install [CMake](https://cmake.org/) and [ninja](https://ninja-build.org/):

```bash
brew install cmake ninja
```

* Install SDL2 frameworks from:
  * [SDL2-2.x.y.dmg](https://www.libsdl.org/download-2.0.php)
  * [SDL2_mixer-2.x.y.dmg](https://www.libsdl.org/projects/SDL_mixer/)
  * [SDL2_image-2.x.y.dmg](https://www.libsdl.org/projects/SDL_image/)

### Build

Use CMake in the same way as for Linux.

## Windows

Not tested.

