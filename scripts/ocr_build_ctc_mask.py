"""Build an offline CTC exclusion set from a pinned OpenCC STCharacters table.

Only unambiguous non-identity single-character sources are excluded. This emits
model-class metadata; it performs no conversion of recognized text.
"""
import argparse
import hashlib
import json
from pathlib import Path

def build_mask(table,version,variants=None):
    blocked=set()
    for line in table.read_text(encoding='utf-8-sig').splitlines():
        if not line or line.startswith('#'):continue
        source,values=line.split('\t',1);targets=values.split()
        if len(source)==1 and targets and all(len(t)==1 for t in targets) and source not in targets:
            blocked.add(source)
    # OpenCC's generic ST table also normalizes traditional variants (e.g.
    # 群->羣, 床->牀). Taiwan's legitimate target forms must remain model classes.
    if variants:
        traditional=set()
        for line in variants.read_text(encoding='utf-8-sig').splitlines():
            if not line or line.startswith('#'):continue
            _,values=line.split('\t',1)
            traditional.update(c for c in values.split() if len(c)==1)
        blocked-=traditional
    return {'version':version,'source_sha256':hashlib.sha256(table.read_bytes()).hexdigest(),
            'blocked_characters':''.join(sorted(blocked))}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--table',type=Path,required=True);parser.add_argument('--version',required=True)
    parser.add_argument('--variants',type=Path)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    with args.output.open('x',encoding='utf-8') as f:json.dump(build_mask(args.table,args.version,args.variants),f,ensure_ascii=False,indent=2)
