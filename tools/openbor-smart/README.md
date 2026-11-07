# OpenBOR-SMART

**System, Media, Automated Rewrite Technology**

OpenBOR-SMART is a standalone Go-based toolkit for helping developers migrate and maintain OpenBOR game assets and scripts.

## Current implementation

The command-line tool currently copies animation text files without modifying their contents. This is intentional: legacy OpenBOR already accepts whitespace-separated `@cmd` arguments, for example:

```text
@cmd changeentityproperty getlocalvar("self") "velocity" 0 1 0
```

Do not rewrite this as a parenthesized call. A broad whitespace-to-comma conversion is not a safe parser and can alter valid expressions or quoted arguments. Animation text should remain byte-for-byte unchanged unless a future syntax-aware pass identifies a specific transformation that is required.

The actual C-style script-to-Lua conversion is the next major implementation task. It must handle standalone `.c` script files and C-style `@script` blocks embedded in animation text, while preserving legacy function names, callback entry points, and engine API calls. API compatibility checks must use the target-era OpenBOR source (around builds 3000–4000); newer APIs must not be assumed to exist.

## Build and use

Build from this directory:

```sh
go build -o openbor-smart .
```

Copy an animation text file unchanged:

```sh
./openbor-smart -in path/to/animation.txt -out converted/animation.txt
```

Run the tests:

```sh
go test ./...
```

## Planned next steps

- Implement a lexer for the legacy C script dialect (comments, strings, identifiers, numbers, operators, and balanced delimiters).
- Translate supported C declarations, function definitions, control flow, and statements into Lua without corrupting strings or comments.
- Extract and translate embedded `@script` blocks while leaving surrounding animation data and legacy `@cmd` lines untouched.
- Add compatibility diagnostics for functions and properties missing from the target engine.
- Add safe folder/batch conversion and collision checks.

PAK extraction/packing, sound conversion, and automatic asset-path rewriting are outside the initial script-conversion scope.
