"""Of the 'text' bucket, how many draw the same characters split differently
(a measurement artefact) rather than different text?"""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import collections
import json
import sys
import xml.etree.ElementTree as ET

ROOT = W + 'puml-docs/'
rows = json.load(open(ROOT + 'score-wasm.json'))


def chars(path):
    root = ET.parse(path).getroot()
    s = ''.join(''.join(el.itertext()) for el in root.iter() if el.tag.split('}')[-1] == 'text')
    return collections.Counter(c for c in s if not c.isspace())


same = 0
near = 0
far = 0
for n, r in rows.items():
    if r['bucket'] != 'text' or 'unparsable' in r.get('detail', ''):
        continue
    a = chars(ROOT + 'little-wasm/' + n[:-5] + '.svg')
    j = chars(ROOT + 'java/' + n[:-5] + '.svg')
    diff = sum(((a - j) + (j - a)).values())
    total = max(sum(j.values()), 1)
    if diff == 0:
        same += 1
        r['split'] = True
    elif diff <= 0.05 * total:
        near += 1
    else:
        far += 1
print('text bucket: same chars (split only) %d, within 5%% of chars %d, further %d' % (same, near, far))
json.dump(rows, open(ROOT + 'score-wasm.json', 'w'), indent=1)
