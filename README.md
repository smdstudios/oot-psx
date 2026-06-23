# oot-psx

A PS1 homebrew prototype that loads data from *The Legend of Zelda: Ocarina of Time* and renders it with a small runtime built on top of [psyqo](https://github.com/grumpycoders/pcsx-redux/tree/main/src/mips/psyqo).

This is not a full game or a finished port, and it was never meant to become one. I built it as a weekend study project to learn more about PS1 3D rendering, room streaming, collision, skeletal animation, and camera controls under real hardware constraints.

No ROM, ISO, or Nintendo assets are included. To build it you need your own NTSC 1.2 Ocarina of Time ROM.

## Current State

The prototype can:

- extract room geometry, textures, collision, transitions, and Link's skeleton from the ROM
- convert the extracted data into PS1-friendly binary formats
- render several OoT scenes on PS1 hardware or in an emulator
- move Link around with basic collision, gravity, and room transitions
- use a DualShock-style camera setup for testing movement and scene traversal

It is rough. There are rendering bugs, performance problems, missing gameplay systems, and plenty of places where the result is closer to a testbed than a game. The repo is here as a technical reference and as a record of the experiment.

## Requirements

- `mipsel-none-elf` cross-compiler toolchain
- `mkpsxiso`
- Python 3
- An OoT NTSC 1.2 ROM named `baserom.z64`

On macOS with Homebrew:

```bash
brew install mkpsxiso
```

For the MIPS toolchain, see the psyqo setup notes:

https://github.com/grumpycoders/pcsx-redux/blob/main/src/mips/psyqo/GETTING_STARTED.md

## Building

Put your ROM at:

```text
rom/baserom.z64
```

Then extract the game data:

```bash
make extract
```

You can also pass the ROM path directly:

```bash
make extract ROM=/path/to/oot-ntsc-1.2.z64
```

Build the disc image:

```bash
make disc BUILD=Release
```

The output is written to:

```text
build/oot.bin
build/oot.cue
```

## Controls

| Input | Action |
|-------|--------|
| Left stick | Move |
| Right stick | Orbit camera |
| Circle, hold | Run |
| Triangle | Toggle Link visibility |
| Select | Cycle scenes |
| L1 / R1 | Cycle rooms inside the current scene |
| Start | Toggle texture grid debug view |

## Repo Layout

```text
src/
  main.cpp            App setup, frame loop, camera, input, HUD
  room.cpp            Scene tables, CD loading, texture upload, room rendering
  skeleton.cpp        Player movement, collision, transitions, skeleton rendering
  scene.h             Shared scene declarations
  prm.h               Room mesh format
  skm.h               Skeleton and animation format
  col.h               Collision format
  trn.h               Room transition format
  vram_alloc.h        VRAM texture and CLUT allocator

tools/
  extract_room.py       Extracts PRM, COL, and TRN data from the ROM
  extract_skeleton.py   Extracts Link's skeleton and animations
  extract_rooms.sh      Batch extraction for the supported scenes

data/
  rooms/             Extracted room meshes
  collision/         Extracted collision meshes
  transitions/       Extracted room transition data
  link.skm           Extracted Link skeleton

third_party/
  nugget/            psyqo SDK and toolchain support
  oot/               OoT decompilation reference
```

## Notes

The extraction tools read N64 F3DEX2 display lists and convert the data into simpler formats for the PS1 runtime. Room geometry is transformed through the GTE, sorted into an ordering table, and submitted as textured primitives. Textures are converted to indexed PS1 formats with CLUTs.

The coordinate systems do not match cleanly: OoT uses a right-handed, Y-up world while the PS1 GTE setup here expects Y-down and forward Z in view space. The runtime handles that conversion in the camera/view transform.

This project was a stepping stone toward other PS1 tooling work, so I do not expect to turn it into a complete game.

## Acknowledgments

- [zeldaret/oot](https://github.com/zeldaret/oot)
- [grumpycoders/pcsx-redux](https://github.com/grumpycoders/pcsx-redux)
- [Lameguy64/mkpsxiso](https://github.com/Lameguy64/mkpsxiso)
- [psx-emu-dev/sm64-psx](https://github.com/psx-emu-dev/sm64-psx)
- [ChenThread/fromage](https://github.com/ChenThread/fromage)
- [ChenThread/candyk-psx](https://github.com/ChenThread/candyk-psx)
