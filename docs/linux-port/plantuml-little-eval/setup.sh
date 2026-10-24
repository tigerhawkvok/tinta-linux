#!/bin/sh
# setup.sh [REF] : prepares $PUL_EVAL (default ~/.cache/tinta-pul-eval) for a
# run of the plantuml-little corpus evaluation (see README.md).
#   - plantuml-little at REF (default main) from the Actrium/supramark
#     monorepo, sparse: the crate and the two sibling crates it needs
#   - the crate's wasm Graphviz (tests/support, from its lockfile, install
#     scripts off), which the Java references use as their dot
#   - PlantUML 1.2026.2, the version the crate targets, checked by SHA-256
#   - the runner: pul-run (one conversion per process) and svg-cmp (the
#     crate's own reference-test comparator)
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
W=${PUL_EVAL:-$HOME/.cache/tinta-pul-eval}
REF=${1:-main}
JAR_SHA=3cdce52133c424dea22425b947ae9d47f2167b0866dfcf99e714d4ea1689975c
mkdir -p "$W/puml-docs/jar"

if [ ! -d "$W/supramark/.git" ]; then
    git clone -q --filter=blob:none --sparse https://github.com/Actrium/supramark "$W/supramark"
    git -C "$W/supramark" sparse-checkout set crates/plantuml-little crates/font-metrics crates/supramark-diagram-core
fi
git -C "$W/supramark" fetch -q origin
git -C "$W/supramark" checkout -q --detach "origin/$REF" 2>/dev/null || git -C "$W/supramark" checkout -q --detach "$REF"
git -C "$W/supramark" log -1 --format='plantuml-little at %h (%cd) %s' --date=short

(cd "$W/supramark/crates/plantuml-little/tests/support" && npm ci --ignore-scripts --no-audit --no-fund)

JAR=$W/puml-docs/jar/plantuml-1.2026.2.jar
[ -f "$JAR" ] || curl -fsSL -o "$JAR" https://github.com/plantuml/plantuml/releases/download/v1.2026.2/plantuml-1.2026.2.jar
echo "$JAR_SHA  $JAR" | sha256sum -c -

cp "$HERE/fonts.conf" "$W/puml-docs/fonts.conf"
rm -rf "$W/runner/src"
mkdir -p "$W/runner"
cp -r "$HERE/runner/Cargo.toml" "$HERE/runner/src" "$W/runner/"
cargo +1.95.0 build --release --manifest-path "$W/runner/Cargo.toml"
echo "ready: $W"
