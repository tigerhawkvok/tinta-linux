"""plantuml-little (supramark main) on the corpus, one process per diagram,
30 s timeout; backend 'native' (graphviz-anywhere, what Tinta would ship) or
'wasm' (the crate's test backend, the Graphviz the Java references used).
Usage: puml_little_run.py native|wasm"""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import concurrent.futures
import json
import os
import subprocess
import sys
import time

ROOT = W + 'puml-docs/'
BIN = W + 'runner/target/release/pul-run'
CRATE = W + 'supramark/crates/plantuml-little'
backend = sys.argv[1]
OUT = ROOT + 'little-' + backend + '/'
os.makedirs(OUT, exist_ok=True)
env = dict(os.environ, PUL_BACKEND=backend, RUST_BACKTRACE='0')


def run(name):
    out = OUT + name[:-5] + '.svg'
    if os.path.exists(out):
        os.remove(out)
    t0 = time.monotonic()
    try:
        r = subprocess.run([BIN, ROOT + 'corpus/' + name, out], capture_output=True, env=env, cwd=CRATE,
                           timeout=30)
        wall = (time.monotonic() - t0) * 1000
        line = r.stdout.decode('utf-8', 'replace').strip()
        if r.returncode != 0:
            tail = r.stderr.decode('utf-8', 'replace').strip().splitlines()
            msg = next((t for t in tail if 'panicked' in t or 'overflow' in t), tail[-1] if tail else '')
            return name, {'status': 'crash', 'rc': r.returncode, 'msg': msg[:300], 'wall': round(wall)}
        status, ms, rest = (line.split('\t') + ['', ''])[:3]
        return name, {'status': status, 'ms': float(ms or 0), 'msg': rest[:300] if status == 'error' else '',
                      'wall': round(wall)}
    except subprocess.TimeoutExpired:
        return name, {'status': 'timeout', 'wall': 30000}


names = sorted(f for f in os.listdir(ROOT + 'corpus') if f.endswith('.puml'))
t0 = time.monotonic()
results = {}
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    for name, res in pool.map(run, names):
        results[name] = res
json.dump(results, open(ROOT + 'little-' + backend + '.json', 'w'), indent=1)
counts = {}
for res in results.values():
    counts[res['status']] = counts.get(res['status'], 0) + 1
print(backend, counts, '%.0f s wall' % (time.monotonic() - t0))
