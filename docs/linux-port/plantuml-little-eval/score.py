"""Score plantuml-little against the Java references.

Per diagram: Java's status, plantuml-little's status, the crate's own
comparator verdict (byte / canonical / fuzzy / differs), and for differences
structural measures: diagram type, the multiset of drawn text, shape counts
and size. Buckets:
  java-error   Java itself failed (error image or nothing); little scored on
               whether it also reports an error
  pass         the crate's comparator accepts it
  close        same type, same text, shape counts and size within 10%
  wrong-type   another diagram type was detected
  text         drawn text differs (missing, extra or mangled)
  shapes       same text, but shape counts or size differ by more than 10%
  little-error / little-crash   Java rendered it, plantuml-little failed
Usage: puml_score.py wasm|native"""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import collections
import json
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = W + 'puml-docs/'
CMP = W + 'runner/target/release/svg-cmp'
backend = sys.argv[1]
LITTLE = ROOT + 'little-' + backend + '/'
java = json.load(open(ROOT + 'java-results.json'))
little = json.load(open(ROOT + 'little-' + backend + '.json'))
names = sorted(java)


def java_failed(name):
    path = ROOT + 'java/' + name[:-5] + '.svg'
    data = open(path, 'rb').read()
    return java[name]['code'] != 0 or not data.strip() or b'has crashed' in data


pairs = [(LITTLE + n[:-5] + '.svg', ROOT + 'java/' + n[:-5] + '.svg') for n in names
         if little[n]['status'] == 'ok' and not java_failed(n)]
out = subprocess.run([CMP], input=''.join('%s\t%s\n' % p for p in pairs), capture_output=True, text=True).stdout
verdicts = {}
for line in out.splitlines():
    parts = line.split('\t')
    verdicts[os.path.basename(parts[0])[:-4] + '.puml'] = (parts[1], parts[2] if len(parts) > 2 else '')

SHAPES = ('rect', 'ellipse', 'polygon', 'path', 'line', 'polyline', 'circle', 'image')


def measure(path):
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError:
        return None
    m = {'type': root.get('data-diagram-type', ''), 'texts': collections.Counter(), 'shapes': collections.Counter()}
    style = root.get('style', '')
    w = re.search(r'width:\s*([\d.]+)', style) or re.search(r'([\d.]+)', root.get('width', '0'))
    h = re.search(r'height:\s*([\d.]+)', style) or re.search(r'([\d.]+)', root.get('height', '0'))
    m['w'] = float(w.group(1)) if w else 0.0
    m['h'] = float(h.group(1)) if h else 0.0
    for el in root.iter():
        tag = el.tag.split('}')[-1]
        if tag == 'text':
            t = ''.join(el.itertext()).strip()
            if t:
                m['texts'][t] += 1
        elif tag in SHAPES:
            m['shapes'][tag] += 1
    return m


def within(a, b, tol=0.10):
    return abs(a - b) <= tol * max(abs(a), abs(b), 1)


rows = {}
for n in names:
    page = n.rsplit('-', 1)[0]
    ls = little[n]['status']
    row = {'page': page, 'little': ls, 'msg': little[n].get('msg', '')}
    if java_failed(n):
        row['bucket'] = 'java-error'
        row['detail'] = 'little ' + ls
    elif ls == 'crash' or ls == 'timeout':
        row['bucket'] = 'little-crash'
    elif ls == 'error':
        row['bucket'] = 'little-error'
    else:
        verdict, why = verdicts.get(n, ('missing', ''))
        row['verdict'] = verdict
        if verdict in ('byte', 'canonical', 'fuzzy'):
            row['bucket'] = 'pass'
        else:
            a = measure(LITTLE + n[:-5] + '.svg')
            j = measure(ROOT + 'java/' + n[:-5] + '.svg')
            if a is None or j is None:
                row['bucket'] = 'text'
                row['detail'] = 'unparsable svg'
            elif a['type'] != j['type']:
                row['bucket'] = 'wrong-type'
                row['detail'] = '%s -> %s' % (j['type'], a['type'])
            elif a['texts'] != j['texts']:
                row['bucket'] = 'text'
                missing = list((j['texts'] - a['texts']).elements())
                extra = list((a['texts'] - j['texts']).elements())
                row['detail'] = 'missing %d %r extra %d %r' % (len(missing), missing[:4], len(extra), extra[:4])
            else:
                shapes_ok = all(within(a['shapes'][k], j['shapes'][k]) for k in set(a['shapes']) | set(j['shapes']))
                size_ok = within(a['w'], j['w']) and within(a['h'], j['h'])
                row['bucket'] = 'close' if shapes_ok and size_ok else 'shapes'
                row['detail'] = 'size %.0fx%.0f vs java %.0fx%.0f' % (a['w'], a['h'], j['w'], j['h'])
    rows[n] = row

json.dump(rows, open(ROOT + 'score-' + backend + '.json', 'w'), indent=1)
order = ['pass', 'close', 'shapes', 'text', 'wrong-type', 'little-error', 'little-crash', 'java-error']
total = collections.Counter(r['bucket'] for r in rows.values())
print(backend, 'total', len(rows), ' '.join('%s=%d' % (b, total[b]) for b in order))
pages = collections.defaultdict(collections.Counter)
for r in rows.values():
    pages[r['page']][r['bucket']] += 1
print('%-24s %5s ' % ('page', 'n') + ' '.join('%6s' % b[:6] for b in order))
for page in sorted(pages, key=lambda p: -sum(pages[p].values())):
    c = pages[page]
    print('%-24s %5d ' % (page, sum(c.values())) + ' '.join('%6d' % c[b] for b in order))
