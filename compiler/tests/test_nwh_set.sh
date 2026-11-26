#!/usr/bin/env bash
# Regression tests for binary .set archives, the link tool and diagnostics.

set -eu

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[1/9] Building NwC and link..."
bash compiler/build.sh >/dev/null

NWC="$ROOT/compiler/builded/nwc"
LINK="$ROOT/compiler/builded/link"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cd "$TMP"

expect_file() {
    if ! diff -u "$1" "$2"; then
        echo "FAIL: unexpected diagnostics"
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# 1. Binary set + linking, including multiple headers in one archive.
# ---------------------------------------------------------------------------
cat > a.nwh <<'EOF'
#def A_NWH
#set "unused.set"
0A hi_a();
#end
EOF

cat > b.nwh <<'EOF'
#def B_NWH
1A add(1A x, 1A y);
#end
EOF

cat > impl.nw <<'EOF'
0A hi_a() { nw::out << "A" << nw::endl; }
1A add(1A x, 1A y) { return x + y; }
EOF

cat > main.nw <<'EOF'
#include <nwc.h>
1A main() { hi_a(); return add(2, 3); }
EOF

echo "[2/9] Compiling a binary .set from two headers..."
"$NWC" -h a.nwh b.nwh -o headers.set >/dev/null

if [ "$(head -c 8 headers.set)" != "NWSET001" ]; then
    echo "FAIL: .set is not a binary NWSET001 archive"
    exit 1
fi

echo "[3/9] Linking .set with .nw sources..."
"$LINK" headers.set impl.nw main.nw -o app.nwo >/dev/null

if [ "$(head -c 4 app.nwo)" != "NWOB" ]; then
    echo "FAIL: link did not produce an NWO file"
    exit 1
fi

for name in hi_a add main; do
    if ! grep -aq "$name" app.nwo; then
        echo "FAIL: '$name' is missing from the linked program"
        exit 1
    fi
done

# ---------------------------------------------------------------------------
# 2. Unknown function from a .set points at the originating .nwh.
# ---------------------------------------------------------------------------
{
    for i in $(seq 1 13); do echo "// pad $i"; done
    echo 'function_name(1A count);'
} > hello.nwh

"$NWC" -h hello.nwh -o FILE.set >/dev/null

cat > nomain.nw <<'EOF'
1A main() { return 0; }
EOF

echo "[4/9] Unknown function diagnostic..."
if "$LINK" FILE.set nomain.nw -o missing.nwo >/dev/null 2>unknown.err; then
    echo "FAIL: missing implementation was accepted"
    exit 1
fi

cat > unknown.expected <<'EOF'
[ ERROR ]: Unknown function name
FILE.set | hello.nwh
14. function_name(1A count);
    ~~~~~~~~~~~~~~~~~~~~~~
EOF
expect_file unknown.expected <(head -n 4 unknown.err)

# ---------------------------------------------------------------------------
# 3. Unsupported void diagnostic.
# ---------------------------------------------------------------------------
{
    for i in $(seq 1 31); do echo "// pad $i"; done
    echo 'void say_hello() {'
    echo '}'
    echo '1A main() { return 0; }'
} > void.nw

echo "[5/9] void diagnostic..."
if "$NWC" void.nw -o void.nwo >/dev/null 2>void.err; then
    echo "FAIL: void was accepted"
    exit 1
fi

cat > void.expected <<'EOF'
[ ERROR ] NwC Didn't support 'void', Did you mean '0A'?
void.nw
32. void say_hello() {
    ~~~~
EOF
expect_file void.expected <(head -n 4 void.err)

# ---------------------------------------------------------------------------
# 4. Duplicate function diagnostic.
# ---------------------------------------------------------------------------
{
    for i in $(seq 1 21); do echo "// pad $i"; done
    echo '0A func_hi() {'
    echo '}'
    for i in $(seq 1 19); do echo "// middle $i"; done
    echo '0A func_hi() {'
    echo '}'
    echo '1A main() { return 0; }'
} > dup.nw

echo "[6/9] Duplicate function diagnostic..."
if "$NWC" dup.nw -o dup.nwo >/dev/null 2>dup.err; then
    echo "FAIL: duplicate functions were accepted"
    exit 1
fi

cat > dup.expected <<'EOF'
[ ERROR ] Conflicting function names
dup.nw
22. 0A func_hi() {
       ~~~~~~~
dup.nw
43. 0A func_hi() {
       ~~~~~~~
Note: Help this, Remove one of func_hi
EOF
expect_file dup.expected dup.err

# ---------------------------------------------------------------------------
# 5. Nested headers and multiple sets.
# ---------------------------------------------------------------------------
cat > inner.nwh <<'EOF'
#def INNER_NWH
0A from_inner();
#end
EOF

cat > outer.nwh <<'EOF'
#def OUTER_NWH
#include "inner.nwh"
0A from_outer();
#end
EOF

cat > all_impl.nw <<'EOF'
0A from_inner() {}
0A from_outer() {}
1A main() { from_inner(); from_outer(); return 0; }
EOF

cat > extra.nwh <<'EOF'
#def EXTRA_NWH
1A twice(1A x);
#end
EOF

cat > extra_impl.nw <<'EOF'
1A twice(1A x) { return x + x; }
EOF

echo "[7/9] Nested headers and multiple .set archives..."
"$NWC" -h outer.nwh -o nested.set >/dev/null
"$NWC" -h extra.nwh -o extra.set >/dev/null
"$LINK" nested.set extra.set all_impl.nw extra_impl.nw \
    -o linked.nwo >/dev/null

if [ "$(head -c 4 linked.nwo)" != "NWOB" ]; then
    echo "FAIL: multi-set link did not produce an NWO file"
    exit 1
fi

# ---------------------------------------------------------------------------
# 6. Signature mismatch is reported at both locations.
# ---------------------------------------------------------------------------
cat > bad.nwh <<'EOF'
#def BAD_NWH
1A add(1A x);
#end
EOF

"$NWC" -h bad.nwh -o bad.set >/dev/null

echo "[8/9] Prototype/implementation mismatch..."
if "$LINK" bad.set impl.nw main.nw -o bad.nwo >/dev/null 2>mismatch.err; then
    echo "FAIL: signature mismatch was accepted"
    exit 1
fi

if ! grep -q "Prototype does not match implementation" mismatch.err; then
    echo "FAIL: mismatch diagnostic is missing"
    exit 1
fi

if ! grep -q "bad.nwh" mismatch.err || ! grep -q "impl.nw" mismatch.err; then
    echo "FAIL: mismatch diagnostic is missing source locations"
    exit 1
fi

# ---------------------------------------------------------------------------
# 7. Plain .nw compilation still works without any .set files.
# ---------------------------------------------------------------------------
echo "[9/9] Plain .nw compilation..."
cat > plain.nw <<'EOF'
#include <nwc.h>
1A main() { nw::out << "plain" << nw::endl; return 0; }
EOF

"$NWC" plain.nw -o plain.nwo >/dev/null

if [ "$(head -c 4 plain.nwo)" != "NWOB" ]; then
    echo "FAIL: plain compilation did not produce an NWO file"
    exit 1
fi

if ! grep -aq "plain" plain.nwo; then
    echo "FAIL: plain program payload is missing"
    exit 1
fi

echo
echo "All binary .set / link tests passed."
