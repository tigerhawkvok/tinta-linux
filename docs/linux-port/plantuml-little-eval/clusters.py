"""Group plantuml-little's failures by cause."""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import collections
import json
import re

ROOT = W + 'puml-docs/'
rows = json.load(open(ROOT + 'score-wasm.json'))
little = json.load(open(ROOT + 'little-wasm.json'))

print('== wrong-type: Java type -> little type')
c = collections.Counter(r['detail'] for r in rows.values() if r['bucket'] == 'wrong-type')
for k, v in c.most_common():
    print('%4d  %s' % (v, k))
print('   pages:', dict(collections.Counter(r['page'] for r in rows.values() if r['bucket'] == 'wrong-type')))

print('== little errors, by message (numbers and quoted text folded)')
c = collections.Counter()
ex = {}
for n, r in rows.items():
    if r['bucket'] in ('little-error', 'little-crash'):
        m = re.sub(r'\d+', 'N', r['msg'])
        m = re.sub(r"'[^']*'|\"[^\"]*\"|`[^`]*`", 'Q', m)[:110]
        c[m] += 1
        ex.setdefault(m, n)
for k, v in c.most_common(25):
    print('%4d  %-110s %s' % (v, k, ex[k]))

print('== crashes')
for n, r in rows.items():
    if r['bucket'] == 'little-crash':
        print('  ', n, little[n])

print('== java errors and what little did')
for n, r in rows.items():
    if r['bucket'] == 'java-error':
        print('  ', n, r['detail'])

print('== malformed SVG (unescaped attribute values)')
print('  ', dict(collections.Counter(r['page'] for r in rows.values() if 'unparsable' in r.get('detail', ''))))
