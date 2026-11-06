# OpenBOR-SMART

**System, Media, Automated Rewrite Technology**

OpenBOR-SMART is a standalone Go-based toolkit for helping developers migrate and maintain OpenBOR game assets and scripts.

## Initial focus

The first release focuses on converting legacy OpenBOR scripts into Lua while preserving compatibility with the Lua-enabled OpenBOR engine.

Planned initial capabilities:

- Convert a single legacy script file to Lua.
- Convert scripts in a folder, scanning `.c` files by default.
- Optionally include additional source extensions, such as `.openbor` and `.script`.
- Write converted files beside their sources or into a selected output directory.
- Detect output filename collisions and avoid silently overwriting existing files.

The converter will parse script structure rather than relying on unsafe global text replacements, so strings, comments, function calls, and legacy control structures can be handled deliberately.

## Building on Linux

OpenBOR-SMART is written in Go and will use the standard Go toolchain without unnecessary platform-specific dependencies.

Once the implementation is in place, build it from this directory with:

```sh
go build -o openbor-smart .
```

## Project scope

The initial version is limited to script conversion. PAK extraction/packing, sound conversion, automatic path rewriting, and other migration utilities are outside the first release and can be considered later.
