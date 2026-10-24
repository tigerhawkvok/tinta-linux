"""The Windows build (prebuilt Windows Graphviz, no pango) on the corpus under
Wine, in a scratch prefix; outputs to puml-docs/little-win/, compared with
the Linux native build's outputs."""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import concurrent.futures
import json
import os
import subprocess
import time

T = W + ''
ROOT = T + 'puml-docs/'
EXE = T + 'runner/target-win/x86_64-pc-windows-msvc/release/pul-run.exe'
OUT = ROOT + 'little-win/'
os.makedirs(OUT, exist_ok=True)
env = dict(os.environ, WINEPREFIX=T + 'wineprefix-pul', WINEDEBUG='-all', WINEDLLOVERRIDES='mscoree,mshtml=')
subprocess.run(['wineboot', '-i'], env=env, capture_output=True, timeout=300)
subprocess.Popen(['wineserver', '-p'], env=env)  # keep it alive between runs
time.sleep(1)


def run(name):
    out = OUT + name[:-5] + '.svg'
    if os.path.exists(out):
        os.remove(out)
    try:
        r = subprocess.run(['wine', EXE, ROOT + 'corpus/' + name, out], capture_output=True, env=env, timeout=60)
        line = r.stdout.decode('utf-8', 'replace').strip()
        status = line.split('\t')[0] if r.returncode == 0 else 'crash'
        return name, status
    except subprocess.TimeoutExpired:
        return name, 'timeout'


names = sorted(f for f in os.listdir(ROOT + 'corpus') if f.endswith('.puml'))
t0 = time.monotonic()
results = {}
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    for name, status in pool.map(run, names):
        results[name] = status
subprocess.run(['wineserver', '-k'], env=env)
json.dump(results, open(ROOT + 'little-win.json', 'w'), indent=1)
counts = {}
for s in results.values():
    counts[s] = counts.get(s, 0) + 1
print('wine', counts, '%.0f s wall' % (time.monotonic() - t0))
