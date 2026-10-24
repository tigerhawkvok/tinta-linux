"""Contact sheet: N seeded-random diagrams from a bucket, Java (left) beside
plantuml-little (right), each scaled into a 520x300 box.
Usage: puml_sheet.py BUCKET N [SEED] -> puml-docs/sheets/BUCKET.png"""
import os as _os
W = _os.environ.get('PUL_EVAL', _os.path.expanduser('~/.cache/tinta-pul-eval')) + '/'
import io
import json
import os
import random
import subprocess
import sys

from PIL import Image, ImageDraw

ROOT = W + 'puml-docs/'
bucket, count = sys.argv[1], int(sys.argv[2])
seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
rows = json.load(open(ROOT + 'score-wasm.json'))
names = sorted(n for n, r in rows.items() if r['bucket'] == bucket)
random.Random(seed).shuffle(names)
names = sorted(names[:count])
BW, BH, LABEL = 520, 300, 16


def raster(path):
    r = subprocess.run(['rsvg-convert', '-b', 'white', path], capture_output=True)
    if r.returncode != 0 or not r.stdout:
        img = Image.new('RGB', (BW, 40), (255, 220, 220))
        ImageDraw.Draw(img).text((6, 12), 'cannot render', fill=(150, 0, 0))
        return img
    img = Image.open(io.BytesIO(r.stdout)).convert('RGB')
    scale = min(BW / img.width, BH / img.height, 1.0)
    return img.resize((max(1, int(img.width * scale)), max(1, int(img.height * scale))))


tiles = []
for n in names:
    a = raster(ROOT + 'java/' + n[:-5] + '.svg')
    b = raster(ROOT + 'little-wasm/' + n[:-5] + '.svg')
    h = max(a.height, b.height) + LABEL + 6
    tile = Image.new('RGB', (BW * 2 + 20, h), (200, 200, 200))
    d = ImageDraw.Draw(tile)
    d.text((4, 2), '%s  [%s]' % (n, rows[n].get('detail', '')[:110]), fill=(0, 0, 0))
    tile.paste(a, (0, LABEL))
    tile.paste(b, (BW + 20, LABEL))
    tiles.append(tile)
os.makedirs(ROOT + 'sheets', exist_ok=True)
# Split into parts no taller than 1600 px so each can be viewed
part, height, parts = [], 0, []
for t in tiles:
    if part and height + t.height > 1600:
        parts.append(part)
        part, height = [], 0
    part.append(t)
    height += t.height
if part:
    parts.append(part)
for i, p in enumerate(parts, 1):
    sheet = Image.new('RGB', (BW * 2 + 20, sum(t.height for t in p)), (200, 200, 200))
    y = 0
    for t in p:
        sheet.paste(t, (0, y))
        y += t.height
    out = ROOT + 'sheets/%s-%d.png' % (bucket, i)
    sheet.save(out)
    print(out)
