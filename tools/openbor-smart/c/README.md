# OpenBOR Smart — C port (work in progress)

This directory contains the pure C implementation being developed alongside the existing Go converter. The Go implementation remains the reference and is not removed or replaced until the C implementation has passed equivalent regression tests and real-game conversion checks.

## Build

From this directory:

```sh
make
make test
```

The build uses a C99 compiler and the standard C library only.

## Usage (once the port is complete)

```sh
./openbor-smart-c -in data/scripts/boss.c
./openbor-smart-c -in /path/to/game/data/scripts -recursive
./openbor-smart-c -in /path/to/game/data/scripts -recursive -out /path/to/converted-scripts
```

The command-line interface is intended to retain the Go tool's `-in`, `-out`, `-recursive`, and `-force` options. This C port is experimental until its regression suite is complete.

## Compatibility requirements

- Preserve the Go converter's C-to-Lua conversion behaviour and output wherever practical.
- Keep the Go implementation available as the reference during the port.
- Keep animation text and legacy `@cmd` lines untouched; the engine handles legacy animation command syntax.
- Do not generate `__openbor_smart_truth`; the OpenBOR Lua runtime now implements the legacy zero-is-false truth semantics.
- Reject unsupported C constructs explicitly rather than emitting unsafe or misleading Lua.
