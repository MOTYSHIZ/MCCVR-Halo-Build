"""Verify compiled palm markers against each title's official tag XML export.

Consumes saved official tool exports in out/left-hand-shared-evidence; never
reads or modifies a running game. Missing right markers are explicitly derived
from the same rig's bilateral digit landmarks, not another title's constants.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / 'out/left-hand-shared-evidence'
RIGS = [
    ('h3', 'objects_characters_masterchief_fp_fp.render_model.xml', 504041493, 37, 'left_hand'),
    ('h3', 'objects_characters_elite_fp_arms_fp_arms.render_model.xml', 269159697, 31, 'left_hand_elite'),
    ('h3', 'objects_characters_dervish_fp_fp.render_model.xml', 268439051, 31, 'left_hand_elite'),
    ('odst', 'objects_characters_odst_recon_fp_fp.render_model.xml', 286525724, 37, 'left_hand'),
    ('odst', 'objects_characters_odst_oni_op_fp_fp.render_model.xml', 403178001, 37, 'left_hand'),
    ('reach', 'objects_characters_spartans_fp_fp.render_model.xml', 404622103, 47, 'left_hand'),
    ('reach', 'objects_characters_elite_fp_fp.render_model.xml', 419566353, 41, 'left_hand_elite'),
    ('h4', 'storm-fp.xml', 353173504, 80, 'left_hand'),
]

def fields(element):
    return {f.get('name'): f.get('value') for f in element.findall('field')}

def block(element, name):
    nested = element.find(f"block[@name='{name}']")
    if nested is not None:
        return nested.findall('element')
    children = list(element)
    start = next(i for i,c in enumerate(children) if c.tag=='field' and c.get('name')==name and c.get('type')=='block')
    count = int(children[start].get('value'))
    values = children[start+1:start+1+count]
    assert all(c.tag=='element' for c in values)
    return values

def floats(text):
    return [float(v) for v in text.split(',')]

def transform(marker):
    q = floats(marker['rotation']); length = math.sqrt(sum(v*v for v in q))
    x,y,z,w = [v/length for v in q]
    # Independent quaternion formula, output column-major like native palettes.
    return [1, 1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),
            2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),
            2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),
            *floats(marker['translation'])]

def parent_index(node, nodes):
    value = node['parent node']
    if ',' in value: return int(value.split(',')[-1])
    if value=='NONE': return -1
    return next(i for i,n in enumerate(nodes) if n['name']==value)

def bilateral(nodes, left, right):
    for wrist in (left,right):
        q=floats(nodes[wrist]['default rotation'])
        assert max(abs(v) for v in q[:3])<.00001 and abs(abs(q[3])-1)<.00001
    pairs=[]
    for i,n in enumerate(nodes):
        if parent_index(n,nodes)!=left: continue
        name=n['name'].replace('l_', 'r_', 1)
        candidates=[j for j,r in enumerate(nodes) if r['name']==name and parent_index(r,nodes)==right]
        if len(candidates)!=1: continue
        a=floats(n['default translation']); b=floats(nodes[candidates[0]]['default translation'])
        residual=max(abs(a[k]-b[k]*(1,-1,1)[k]) for k in range(3))
        # Native thumbs are deliberately asymmetric. Index and pinky establish
        # the local reflection plane; retain all measured pairs in the report.
        pairs.append(dict(left=n['name'],right=name,left_position=a,right_position=b,residual=residual))
    precise=[p for p in pairs if 'index' in p['left'] or 'pinky' in p['left']]
    assert len(precise)>=2 and all(p['residual']<.00002 for p in precise), precise
    return pairs

def verify(adapter):
    lines=subprocess.check_output([str(adapter), '--markers'],text=True).splitlines()
    assert len(lines)==len(RIGS)
    records=[]
    for (title,filename,checksum,count,left_name),line in zip(RIGS,lines):
        path=EVIDENCE/title/filename
        # Official exporters emit unescaped error/material strings later in
        # some files. Nodes/markers precede materials and form a complete XML
        # prefix; do not modify the evidence file or interpret malformed tails.
        raw=path.read_bytes()
        ends=[raw.index(token) for token in (b'<block name="materials"',b'<field name="materials"') if token in raw]
        root=ET.fromstring(raw[:min(ends)]+b'</tag>')
        assert int(fields(root)['runtime import info checksum'])==checksum
        nodes=[fields(n) for n in block(root,'nodes')]
        assert len(nodes)==count
        groups={fields(g)['name']: [fields(m) for m in block(g,'markers')] for g in block(root,'marker groups')}
        left=groups[left_name][0]
        li=int(left['node index']); derived=False; pairs=[]
        right_name='right_hand' if 'right_hand' in groups else 'right_hand_elite'
        if right_name in groups:
            right=groups[right_name][0]; ri=int(right['node index'])
        else:
            derived=True
            ri=next(i for i,n in enumerate(nodes) if n['name']=='r_hand')
            pairs=bilateral(nodes,li,ri)
            p=floats(left['translation']); q=floats(left['rotation'])
            right=dict(translation=','.join(map(str,(p[0],-p[1],p[2]))),
                       rotation=','.join(map(str,(-q[0],q[1],-q[2],q[3]))))
        if title=='h4':
            assert li==54 and nodes[li]['name']=='b_l_hand_marker_offset'
            assert floats(nodes[li]['default translation'])==[0,0,0]
            assert floats(nodes[li]['default rotation'])==[0,0,0,1]
            li=parent_index(nodes[li],nodes)
        values=[float(v) for v in line.split()]
        expected=[ri,li,*transform(right),*transform(left)]
        error=max(abs(a-b) for a,b in zip(values,expected))
        assert len(values)==len(expected) and error<.000002,(title,filename,error)
        tag=EVIDENCE/title/'tags'/Path(root.get('id').replace('\\','/')+'.render_model')
        records.append(dict(title=title,rig=root.get('id'),checksum=checksum,nodes=count,
                            tag_sha256=hashlib.sha256(tag.read_bytes()).hexdigest().upper(),
                            xml_sha256=hashlib.sha256(path.read_bytes()).hexdigest().upper(),
                            right_node=ri,left_node=li,left=left,right=right,
                            right_is_derived=derived,bilateral_landmarks=pairs,max_compiled_error=error))
    return dict(rigs=records,adapter_sha256=hashlib.sha256(adapter.read_bytes()).hexdigest().upper(),
                result='PASS',limit='Official asset and compiled math checks, not headset acceptance.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adapter-exe',type=Path,required=True)
    parser.add_argument('--record',type=Path,required=True)
    args=parser.parse_args()
    report=verify(args.adapter_exe.resolve())
    args.record.write_text(json.dumps(report,indent=2)+'\n')
    print(f"PASS: {len(report['rigs'])} official title-specific rigs match compiled palm markers")
