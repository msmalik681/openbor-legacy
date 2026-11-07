# OpenBOR-SMART

**System, Media, Automated Rewrite Technology**

OpenBOR-SMART is a standalone Go-based toolkit for helping developers migrate and maintain OpenBOR game assets and scripts.

## Current implementation

The animation-text pass converts legacy whitespace-separated `@cmd` commands into explicit Lua calls while leaving all other animation text untouched.

For example:

```text
@cmd changeentityproperty getlocalvar("self") "velocity" 0 1 0
```

becomes:

```text
@cmd changeentityproperty(getlocalvar("self"),"velocity",0,1,0);
```

Quoted arguments are kept intact, including spaces inside quoted strings. Commands already written with parentheses are not rewritten. A command with no arguments becomes a zero-argument call, such as `@cmd clearL();`.

The current pass treats whitespace outside quoted strings as the separator between command arguments. It does not parse nested expressions containing whitespace, comments, or arbitrary C expressions; those forms must be tested against the original engine before being accepted as supported syntax.

The actual C-style script-to-Lua conversion is the next major implementation task. It must handle standalone `.c` script files and C-style `@script` blocks embedded in animation text, while preserving legacy function names, callback entry points, and engine API calls. API compatibility checks must use the target-era OpenBOR source (around builds 3000–4000); newer APIs must not be assumed to exist.

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

- Implement the parser and Lua generator for the legacy C script dialect.
- Translate supported C declarations, function definitions, control flow, and statements into Lua without corrupting strings or comments.
- Extract and translate embedded `@script` blocks while leaving surrounding animation data and converted `@cmd` commands intact.
- Add compatibility diagnostics for functions and properties missing from the target engine.
- Add safe folder/batch conversion and collision checks.

PAK extraction/packing, sound conversion, and automatic asset-path rewriting are outside the initial script-conversion scope.
