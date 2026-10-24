"""Java PlantUML 1.2026.2 references for the corpus, as the crate's own
generate_reference.sh makes them (java -jar ... -tsvg -pipe), with Graphviz
through its wasm wrapper and DejaVu as the generic fonts; SANDBOX profile,
as Tinta runs PlantUML. Eight at a time; exit code and time per file.

The JDK matters: on Thoth-Linux the system OpenJDK 21 measures text
differently from the crate's references (2 of 20 fixtures match) while
Linuxbrew's OpenJDK 27 reproduces them (19 of 20). $JAVA picks the JDK
(default: java on PATH), and the run stops unless it reproduces the crate's
own committed references first."""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import concurrent.futures
import json
import random
import shutil
import os
import subprocess
import time

ROOT = W + 'puml-docs/'
JAR = ROOT + 'jar/plantuml-1.2026.2.jar'
WRAPPER = W + 'supramark/crates/plantuml-little/scripts/wasm-dot-wrapper.sh'
CRATE = W + 'supramark/crates/plantuml-little/'
CMP = W + 'runner/target/release/svg-cmp'
JAVA = _os.environ.get('JAVA') or shutil.which('java')
OUT = ROOT + 'java/'
os.makedirs(OUT, exist_ok=True)
env = dict(os.environ, GRAPHVIZ_DOT=WRAPPER, FONTCONFIG_FILE=ROOT + 'fonts.conf',
           PLANTUML_SECURITY_PROFILE='SANDBOX')


def render(name):
    src = open(ROOT + 'corpus/' + name, 'rb').read()
    t0 = time.monotonic()
    try:
        r = subprocess.run([JAVA, '-Djava.awt.headless=true', '-jar', JAR, '-tsvg', '-charset', 'UTF-8',
                            '-pipe'], input=src, capture_output=True, env=env, timeout=120)
        code, out = r.returncode, r.stdout
    except subprocess.TimeoutExpired:
        code, out = 'timeout', b''
    ms = (time.monotonic() - t0) * 1000
    open(OUT + name[:-5] + '.svg', 'wb').write(out)
    return name, code, ms


# Without a working dot, Java draws a "PlantUML has crashed" page and still
# exits 0, so check the wrapper first and the outputs afterwards
probe = subprocess.run([WRAPPER, '-Tsvg'], input=b'digraph G { a -> b }', capture_output=True, env=env, timeout=120)
if b'<svg' not in probe.stdout:
    raise SystemExit('Graphviz wrapper does not work (run setup.sh): ' + probe.stderr.decode('utf-8', 'replace')[-300:])

# The JDK and fonts must reproduce the crate's committed references
fixtures = []
for root, _, files in os.walk(CRATE + 'tests/reference'):
    for f in files:
        rel = os.path.relpath(os.path.join(root, f), CRATE + 'tests/reference')[:-4]
        if f.endswith('.svg') and os.path.exists(CRATE + 'tests/fixtures/' + rel + '.puml'):
            fixtures.append(rel)
fixtures.sort()
random.Random(5).shuffle(fixtures)
os.makedirs(ROOT + 'calib-jdk', exist_ok=True)
pairs = ''
for rel in fixtures[:20]:
    out = ROOT + 'calib-jdk/' + rel.replace('/', '_') + '.svg'
    r = subprocess.run([JAVA, '-Djava.awt.headless=true', '-jar', JAR, '-tsvg', '-charset', 'UTF-8', '-pipe'],
                       input=open(CRATE + 'tests/fixtures/' + rel + '.puml', 'rb').read(), capture_output=True,
                       env=env, timeout=120)
    open(out, 'wb').write(r.stdout)
    pairs += '%s\t%s\n' % (out, CRATE + 'tests/reference/' + rel + '.svg')
verdicts = subprocess.run([CMP], input=pairs, capture_output=True, text=True).stdout.splitlines()
matched = sum(1 for v in verdicts if v.split('\t')[1] in ('byte', 'canonical', 'fuzzy'))
print('%s reproduces %d of %d crate references' % (JAVA, matched, len(verdicts)))
if matched < 18:
    raise SystemExit('this JDK does not reproduce the references; set JAVA to another JDK')
names = sorted(f for f in os.listdir(ROOT + 'corpus') if f.endswith('.puml'))
results = {}
t0 = time.monotonic()
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    for name, code, ms in pool.map(render, names):
        results[name] = {'code': code, 'ms': round(ms)}
json.dump(results, open(ROOT + 'java-results.json', 'w'), indent=1)
ok = sum(1 for r in results.values() if r['code'] == 0)
crashed = [n for n in names if b'has crashed' in open(OUT + n[:-5] + '.svg', 'rb').read()]
if crashed:
    print('%d crash pages (scored as Java failures), e.g. %s' % (len(crashed), crashed[:3]))
print('%d files, %d exit 0, %d nonzero, %.0f s wall' % (len(results), ok, len(results) - ok, time.monotonic() - t0))
