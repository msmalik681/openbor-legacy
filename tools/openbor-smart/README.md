# OpenBOR-SMART

**System, Media, Automated Rewrite Technology**

OpenBOR-SMART is a standalone Go-based toolkit for helping developers migrate and maintain OpenBOR game assets and scripts.

## Current implementation

The first implemented conversion pass handles legacy animation `@cmd` lines in OpenBOR text files.

Legacy commands use whitespace-separated arguments. The converter preserves each token and changes the whitespace separators into commas, wrapping the arguments in a function call. For example:

```text
@cmd changeentityproperty getlocalvar("self") "velocity" 0 1 0
```

becomes:

```text
@cmd changeentityproperty(getlocalvar("self"),"velocity",0,1,0)
```

This intentionally follows the legacy whitespace-token rule rather than trying to parse expressions or reinterpret parentheses and quotes. The command remains attached to the animation data, so the engine can execute it in the player's animation-script context, where that script's helper functions are available.

## Build and use

Build from this directory:

```sh
go build -o openbor-smart .
```

Convert an animation text file:

```sh
./openbor-smart -in path/to/animation.txt -out converted/animation.txt
```

Run the tests:

```sh
go test ./...
```

## Planned next steps

- Convert legacy C-style script files into Lua while preserving legacy function names and behavior.
- Process script blocks embedded in animation text files.
- Add safe folder/batch conversion and collision checks.
- Add compatibility diagnostics for APIs not provided by the target engine.

PAK extraction/packing, sound conversion, and automatic asset-path rewriting are outside the initial script-conversion scope.
