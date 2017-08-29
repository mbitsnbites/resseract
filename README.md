# Resseract

Resseract is a first-person shooter game focused on instagib deathmatch and capture-the-flag
gameplay as well as cooperative in-game map editing.

It is a fork of [Tesseract](http://tesseract.gg/), which in turn is a fork of
[Cube 2: Sauerbraten](http://sauerbraten.org/).

## Documentation
* [Game manual](http://sauerbraten.org/README.html) (from Cube 2: Sauerbraten)
* [Tesseract introduction](doc/Tesseract.md)
* [Tesseract rendering engine](doc/Renderer.md)

## Git fork

This is a fork of the [SVN repo](http://websvn.tuxfamily.org/tesseract/main/)
(svn://svn.tuxfamily.org/svnroot/tesseract/main).

Changes compared to the SVN original:
* Uses CMake instead of Make/VS/XCode.
* Only source code (no binaries) are included in the repo.
* Data files are kept in a [separate repository](https://github.com/mbitsnbites/resseract-data).
* Main documentation was converted to Markdown.

## Build instructions

### Ubuntu

Prerequisites:

```bash
sudo apt install cmake ninja-build xorg-dev libgl1-mesa-dev libsdl2-mixer-dev libsdl2-image-dev
```

Create a build-directory, preferrably in a different directory than the Tesseract source
(out-of-tree), and run CMake and ninja:

```bash
mkdir resseract-build
cd resseract-build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release path/to/resseract/src
ninja
```

### macOS (experimental)

Install the SDL2 frameworks from:
* [SDL2-2.x.y.dmg](https://www.libsdl.org/download-2.0.php)
* [SDL2_mixer-2.x.y.dmg](https://www.libsdl.org/projects/SDL_mixer/)
* [SDL2_image-2.x.y.dmg](https://www.libsdl.org/projects/SDL_image/)

Use CMake in the same way as for Ubuntu (make sure that you have XCode, CMake and Ninja installed).

