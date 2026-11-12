#!/usr/bin/env python3
"""Build the QuestHalfTex mod tree from your own NMS Windows paks.

Reads questhalftex-files.tsv (mod path, pak path, pak), extracts each texture with
HGPAKtool 1.1.3, drops its top mip with halve_dds.py and writes OUT_DIR/TEXTURES/...

usage: build_questhalftex.py PCBANKS_DIR OUT_DIR [--limit N]
"""
import os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from halve_dds import halve, parse
from hgpaktool.api import HGPAKFile  # pinned to 1.1.3; _extractor_function is its private API

a = sys.argv[1:]
if len(a) not in (2, 4) or (len(a) == 4 and a[2] != '--limit'):
    sys.exit(__doc__)
pcbanks, out = a[0], a[1]
limit = int(a[3]) if len(a) == 4 else None
here = os.path.dirname(os.path.abspath(__file__))
rows = [l.rstrip('\n').split('\t') for l in open(os.path.join(here, 'questhalftex-files.tsv'))][1:limit and limit + 1]
bypak = collections.defaultdict(list)
for mod_rel, pak_rel, pak in rows:
    bypak[pak].append((mod_rel, pak_rel))
done = 0
for pak, files in sorted(bypak.items()):
    with HGPAKFile(os.path.join(pcbanks, pak), 'windows') as p:
        names = {k.lower(): k for k in p.files}
        for mod_rel, pak_rel in files:
            src = b''.join(p._extractor_function(names[pak_rel], -1))
            new, info = halve(src)  # every listed file halved cleanly in the reference build
            if parse(new)['mips'] != info['mips'] - 1 or new[0x74:0x78] != src[0x74:0x78]:
                raise RuntimeError('halved texture failed its check: ' + pak_rel)
            dst = os.path.join(out, mod_rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, 'wb') as f:
                f.write(new)
            done += 1
    print(pak, len(files), flush=True)
print('halved', done, 'of', len(rows))
