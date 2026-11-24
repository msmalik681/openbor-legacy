# OpenBOR-SMART — C implementation

This is the maintained and sole implementation of OpenBOR-SMART. It is written in C99 and uses only the standard C library.

## Build and test

From this directory:

```sh
make
make test
```

The build produces `openbor-smart-c`.

## Usage

Convert one script:

```sh
./openbor-smart-c -in data/scripts/boss.c
```

Convert a directory recursively:

```sh
./openbor-smart-c -in /path/to/game/data/scripts -recursive
```

Write converted files to a separate directory:

```sh
./openbor-smart-c -in /path/to/game/data/scripts -recursive -out /path/to/converted-scripts
```

Use `-force` to overwrite existing Lua files. Without it, existing outputs are preserved.

## Compatibility requirements

- Preserve legacy C-to-Lua conversion behaviour.
- Keep animation text and legacy `@cmd` lines untouched; the engine handles legacy animation command syntax.
- Do not generate `__openbor_smart_truth`; the OpenBOR Lua runtime implements the legacy zero-is-false truth semantics.
- Reject unsupported C constructs explicitly rather than emitting unsafe or misleading Lua.

The converter is a script converter, not a complete C compiler. Check conversion errors and test generated scripts in the target OpenBOR build before using them in a release.
