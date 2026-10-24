"""For one page's 'text' failures: which source keywords do the failing
examples use, against the passing/close ones? A keyword far more common in
failures points at an unsupported construct. Usage: puml_seq_causes.py PAGE"""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import collections
import json
import re
import sys

ROOT = W + 'puml-docs/'
page = sys.argv[1]
rows = json.load(open(ROOT + 'score-wasm.json'))
good = collections.Counter()
bad = collections.Counter()
ng = nb = 0
for n, r in rows.items():
    if r['page'] != page or r['bucket'] == 'java-error':
        continue
    src = open(ROOT + 'corpus/' + n).read()
    words = set(re.findall(r'(?m)^\s*(!?[a-z][a-z_]*|<style>|skinparam \w+|#\w+|\[[#a-z]*\]|\|\|\||\.\.\.|==|-\[)', src.lower()))
    words |= {w for w in ('<<', '**', '//', '""', '<color', '<size', '<b>', '<i>', '<img', '<&', '<$', '\\n', 'creole',
                          '&', '++', '--', '**', 'return', 'autonumber', 'teoz', '%', '!') if w in src.lower()}
    if r['bucket'] in ('pass', 'close'):
        ng += 1
        good.update(words)
    else:
        nb += 1
        bad.update(words)
print('%s: %d right-ish, %d wrong' % (page, ng, nb))
print('%-24s %6s %6s' % ('construct', 'wrong', 'right'))
for w, v in sorted(bad.items(), key=lambda kv: -(kv[1] / nb - good[kv[0]] / max(ng, 1))):
    if v >= 3:
        print('%-24s %5d%% %5d%%  (%d)' % (w, 100 * v / nb, 100 * good[w] / max(ng, 1), v))
