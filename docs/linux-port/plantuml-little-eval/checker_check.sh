#!/bin/sh
# Calibrate the comparison: the crate's own fixtures and their committed Java
# references, through my runner and my Java setup.
T=${PUL_EVAL:-$HOME/.cache/tinta-pul-eval}
C=$T/supramark/crates/plantuml-little
B=$T/runner/target/release
D=$T/puml-docs/calib
rm -rf "$D"; mkdir -p "$D"
cd "$C"
for f in usecase/basic class/a0005 sequence/a0000 activity/if_else_endif component/colors001 state/scxml0001; do
  [ -f "tests/fixtures/$f.puml" ] || { echo "no fixture $f"; continue; }
  n=$(echo "$f" | tr / _)
  PUL_BACKEND=wasm "$B/pul-run" "tests/fixtures/$f.puml" "$D/$n.little.svg" > /dev/null
  GRAPHVIZ_DOT=$C/scripts/wasm-dot-wrapper.sh FONTCONFIG_FILE=$T/puml-docs/fonts.conf PLANTUML_SECURITY_PROFILE=SANDBOX \
    java -Djava.awt.headless=true -jar $T/puml-docs/jar/plantuml-1.2026.2.jar -tsvg -charset UTF-8 -pipe \
    < "tests/fixtures/$f.puml" > "$D/$n.java.svg" 2>/dev/null
  printf '%s\t%s\n' "$D/$n.little.svg" "tests/reference/$f.svg"
  printf '%s\t%s\n' "$D/$n.java.svg" "tests/reference/$f.svg"
  printf '%s\t%s\n' "$D/$n.little.svg" "$D/$n.java.svg"
done > "$D/pairs.txt"
# A negative control: two different diagrams must differ
printf '%s\t%s\n' "$D/usecase_basic.java.svg" "$D/sequence_a0000.java.svg" >> "$D/pairs.txt"
"$B/svg-cmp" < "$D/pairs.txt" | sed "s|$D/||" | cut -c1-200
