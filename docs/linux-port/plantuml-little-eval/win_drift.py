"""Windows (Wine) vs Linux native outputs: identical or not, by page; and the
crate comparator's pass count against Java for each."""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import collections
import json
import os
import subprocess
import xml.etree.ElementTree as ET

T = W + ''
ROOT = T + 'puml-docs/'
CMP = T + 'runner/target/release/svg-cmp'
win = json.load(open(ROOT + 'little-win.json'))
rows = json.load(open(ROOT + 'score-wasm.json'))
names = [n for n in sorted(win) if win[n] == 'ok' and os.path.exists(ROOT + 'little-native/' + n[:-5] + '.svg')]
same = collections.Counter()
diff = collections.Counter()
diff_names = []
for n in names:
    a = open(ROOT + 'little-win/' + n[:-5] + '.svg', 'rb').read()
    b = open(ROOT + 'little-native/' + n[:-5] + '.svg', 'rb').read()
    (same if a == b else diff)[rows[n]['page']] += 1
    if a != b:
        diff_names.append(n)
print('win vs linux: %d identical, %d differ' % (sum(same.values()), sum(diff.values())))
for page in sorted(set(same) | set(diff), key=lambda p: -diff[p]):
    if diff[page]:
        print('  %-24s differ %3d of %3d' % (page, diff[page], diff[page] + same[page]))


def size(path):
    try:
        r = ET.parse(path).getroot()
    except ET.ParseError:
        return None
    import re
    st = r.get('style', '')
    w = re.search(r'width:\s*([\d.]+)', st)
    h = re.search(r'height:\s*([\d.]+)', st)
    return (float(w.group(1)), float(h.group(1))) if w and h else None


ratios = []
for n in diff_names:
    a = size(ROOT + 'little-win/' + n[:-5] + '.svg')
    b = size(ROOT + 'little-native/' + n[:-5] + '.svg')
    if a and b:
        ratios.append((max(a[0] / b[0], b[0] / a[0]), max(a[1] / b[1], b[1] / a[1]), n))
ratios.sort(reverse=True)
big = [r for r in ratios if r[0] > 1.05 or r[1] > 1.05]
print('differing diagrams whose size changes by more than 5%%: %d of %d' % (len(big), len(ratios)))
for r in ratios[:6]:
    print('  %.2fx w %.2fx h  %s' % r)

for label, d in (('linux', 'little-native/'), ('windows', 'little-win/')):
    pairs = ''.join('%s%s.svg\t%sjava/%s.svg\n' % (ROOT + d, n[:-5], ROOT, n[:-5]) for n in names
                    if rows[n]['bucket'] != 'java-error')
    out = subprocess.run([CMP], input=pairs, capture_output=True, text=True).stdout
    c = collections.Counter(line.split('\t')[1] for line in out.splitlines())
    print('%-8s vs java: pass %d (byte %d, canonical %d, fuzzy %d), differ %d' % (
        label, c['byte'] + c['canonical'] + c['fuzzy'], c['byte'], c['canonical'], c['fuzzy'], c['differs']))
json.dump(diff_names, open(ROOT + 'win-diff.json', 'w'))
