#!/bin/sh
set -eu

BIN=\${1:-./openbor-smart-c}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

PROJECT="$TMP/project"
mkdir -p "$PROJECT/data/scripts" "$PROJECT/data/chars/chris"
cat > "$PROJECT/data/scripts/boss.c" <<'EOF'
void boss() {
    int health = 100;
}
EOF
cat > "$PROJECT/data/scripts/boss.lua" <<'EOF'
function boss()
    local health = 1
end
EOF
cat > "$PROJECT/data/chars/chris/chris.txt" <<'EOF'
anim idle
@cmd clearL
@script
int framecheck = 1;
@end_script
EOF

if "$BIN" -in "$PROJECT" -force </dev/null >/dev/null 2>&1; then
    echo "FAIL: in-place migration should require confirmation" >&2
    exit 1
fi
test -f "$PROJECT/data/scripts/boss.c"
test -f "$PROJECT/data/scripts/boss.lua"

printf 'yes\n' | "$BIN" -in "$PROJECT" -force >/dev/null

test ! -e "$PROJECT/data/scripts/boss.c"
test -f "$PROJECT/data/scripts/boss.lua"
grep -q 'function boss()' "$PROJECT/data/scripts/boss.lua"
grep -q 'local health = 100' "$PROJECT/data/scripts/boss.lua"
! grep -q 'local health = 1' "$PROJECT/data/scripts/boss.lua"
grep -q '^@cmd clearL$' "$PROJECT/data/chars/chris/chris.txt"
grep -q '^-- OPENBOR_SMART_LUA$' "$PROJECT/data/chars/chris/chris.txt"

echo "PASS: force replaces existing Lua output and preserves @cmd animation directives"
