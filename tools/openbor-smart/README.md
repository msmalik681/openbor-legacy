# OpenBOR-SMART

**System, Media, Automated Rewrite Technology**

OpenBOR-SMART is the maintained C99 converter for migrating legacy OpenBOR scripts to Lua. It uses only the C standard library plus the platform directory APIs used by the project.

## Build and test

From \`tools/openbor-smart/\`:

\`\`\`sh
make
make test
\`\`\`

## Convert one file

\`\`\`sh
./openbor-smart-c -in data/scripts/player.c
./openbor-smart-c -in data/chars/chris/chris.txt
\`\`\`

A \`.c\` file becomes a \`.lua\` file. A \`.txt\` animation file keeps its extension; every embedded \`@script\` block is converted while all animation data and \`@cmd\` lines are preserved.

Without \`-out\`, the tool performs an in-place migration. It displays a warning and requires confirmation (\`y\`, \`yes\`, \`yeah\`, \`yep\`, or \`yea\`). Declining or entering an unrecognised response cancels without changing the source. The original \`.c\` is removed only after its Lua replacement has been successfully prepared and installed.

## Convert a complete project

Selecting a directory recursively processes all files and subdirectories; no extra recursive flag is needed:

\`\`\`sh
./openbor-smart-c -in /path/to/game/data
\`\`\`

The in-place project migration:
- Converts all \`.c\` files to \`.lua\`.
- Converts embedded \`@script\` blocks in \`.txt\` files without changing other animation text or \`@cmd\` directives.
- Lowercases all file and directory names beneath the selected project directory.
- Copies non-script assets unchanged.
- Detects destination-name collisions before making changes.
- Builds a complete staged copy first, then swaps it into place. If installation fails, it attempts to restore the original directory.

## Non-destructive output

Use \`-out\` to keep the original project untouched:

\`\`\`sh
./openbor-smart-c -in /path/to/game/data -out /path/to/converted/data
\`\`\`

The converted tree preserves the directory structure, lowercases names, and converts \`.c\` to \`.lua\`. The output directory must not already exist unless \`-force\` is supplied. For a single file, \`-out\` names a separate output directory.

## Supported C subset

The converter handles C-style function definitions, typed parameters, local scalar and array declarations, assignments, arithmetic and comparison expressions, logical operators, function calls, array indexing, member access, unary not/sign operators, the conditional operator, and common control flow: \`if/else\`, \`while\`, \`do/while\`, \`for\`, non-fallthrough \`switch/case/default\`, \`break\`, and \`return\`.

C array indices are shifted by one for Lua's 1-based tables. C truth testing is preserved for conditions so numeric zero is false. Unsupported constructs are reported rather than silently translated; examples include pointer operations/declarations, \`sizeof\`, comma expressions, \`continue\`, switch fallthrough, array initializer lists, typedef-based declarations, preprocessor directives, and global declarations. Inspect errors and test converted scripts in the target OpenBOR build.

This is a script converter, not a complete C compiler. PAK extraction/packing, sound conversion, and automatic asset-path rewriting are outside its scope. Compatibility must be checked against the target-era OpenBOR API; newer engine APIs must not be assumed to exist.
