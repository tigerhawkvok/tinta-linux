"""The PlantUML documentation's examples as a corpus: each <textarea> on the
diagram pages of plantuml.com, unescaped, one .puml per example, labelled by
page; duplicates dropped. Pages are fetched once (cached) a second apart."""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import hashlib
import html
import os
import re
import time
import urllib.request

ROOT = W + 'puml-docs/'
PAGES = [
    'sequence-diagram', 'use-case-diagram', 'class-diagram', 'object-diagram', 'activity-diagram-beta',
    'component-diagram', 'deployment-diagram', 'state-diagram', 'timing-diagram', 'gantt-diagram',
    'mindmap-diagram', 'wbs-diagram', 'json', 'yaml', 'nwdiag', 'salt', 'ie-diagram', 'er-diagram',
    'archimate-diagram', 'chronology-diagram', 'regex', 'ebnf', 'chen-diagram', 'files-diagram',
    'commons', 'creole', 'preprocessing', 'activity-diagram-legacy', 'board', 'packet-diagram', 'chart',
]
os.makedirs(ROOT + 'html', exist_ok=True)
os.makedirs(ROOT + 'corpus', exist_ok=True)
seen = set()
counts = {}
for page in PAGES:
    path = ROOT + 'html/' + page + '.html'
    if not os.path.exists(path):
        try:
            req = urllib.request.Request('https://plantuml.com/' + page,
                                         headers={'User-Agent': 'tinta-mono-research (philip)'})
            data = urllib.request.urlopen(req, timeout=30).read()
            open(path, 'wb').write(data)
        except Exception as e:  # a page that does not exist
            print('skip', page, e)
            open(path, 'wb').write(b'')
        time.sleep(1.0)
    text = open(path, encoding='utf-8', errors='replace').read()
    n = 0
    for m in re.finditer(r'(?s)<textarea[^>]*>(.*?)</textarea>', text):
        src = html.unescape(m.group(1)).replace('\r\n', '\n').strip('\n') + '\n'
        if '@start' not in src:
            continue
        key = hashlib.sha1(src.encode()).hexdigest()
        if key in seen:
            continue
        seen.add(key)
        n += 1
        open(ROOT + 'corpus/%s-%03d.puml' % (page, n), 'w').write(src)
    counts[page] = n
for page, n in counts.items():
    print('%-26s %4d' % (page, n))
print('total', sum(counts.values()))
