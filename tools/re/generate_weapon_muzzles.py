"""Generate single unambiguous authored muzzle records; no runtime hook guesses.

Default barrel names come from independently checked kit selectors. Multiple
different markers/permutations or conflicting weapon definitions stay absent.
"""
import hashlib
import json
import math
from pathlib import Path
from export_reload_sources import ROOT

TITLES={'HCEEK':'HaloCE','H2EK':'Halo2','H3EK':'Halo3','H3ODSTEK':'Halo3ODST','HREK':'HaloReach','H4EK':'Halo4'}


def number(value):
    assert math.isfinite(value)
    text=f'{value:.9g}'
    return text+('f' if '.' in text or 'e' in text else '.0f')


def main():
    audit=ROOT/'out/refinement-20260918/authored-muzzle-audit.json'
    records=[];refused=[]
    for model in json.loads(audit.read_text())['models']:
        kit=model['kit'];title=TITLES[kit]
        identity=int(model['graph_identity'],16) if kit=='HCEEK' else (
            int(hashlib.sha256(model['model'].encode()).hexdigest()[:16],16) if kit=='H2EK' else model['checksum'])
        if not identity:continue
        for barrel in range(2):
            if kit=='HCEEK':names={['primary trigger','secondary trigger'][barrel]}
            else:
                names={b['authored_marker'] or ['primary_trigger','secondary_trigger'][barrel]
                    for w in model['weapons'] for b in w['barrels'] if b['index']==barrel}
            if not names:continue
            if len(names)!=1:
                refused.append(dict(kit=kit,model=model['model'],barrel=barrel,reason='conflicting authored names'));continue
            name=next(iter(names))
            matches=[m for m in model['markers'] if m['name']==name]
            # Identical records in several permutations are still one geometric
            # answer. Distinct transforms need a proven native selector.
            unique={}
            for marker in matches:
                node=marker['graph_node'] if kit=='HCEEK' else marker['node']
                if node<0:continue
                q=marker['rotation'];length=math.sqrt(sum(x*x for x in q));q=[x/length for x in q]
                x,y,z,w=q
                forward=[1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y)]
                up=[2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)]
                key=(node,*marker['position'],*(round(x,8) for x in forward+up))
                unique[key]=(node,marker['position'],forward,up)
            if len(unique)!=1:
                refused.append(dict(kit=kit,model=model['model'],barrel=barrel,reason='missing or distinct authored markers',count=len(unique)));continue
            node,position,forward,up=next(iter(unique.values()))
            count=model['graph']['nodes'] if kit=='HCEEK' else model['nodes']
            assert 0<=node<count<=255
            record=dict(title=title,identity=f'{identity:016X}',node_count=count,node=node,barrel=barrel,
                marker=name,position=position,forward=forward,up=up,source=model['model'],sha256=model['model_sha256'])
            old=next((r for r in records if (r['title'],r['identity'],r['barrel'])==(title,record['identity'],barrel)),None)
            if old:
                assert all(old[k]==record[k] for k in ['node_count','node','position','forward','up']),('identity collision',old,record)
            else:records.append(record)
    lines=['// Generated from official per-title authored markers; ambiguous entries stay stock.',
           '#pragma once','namespace weapon_muzzle {','inline constexpr Marker kMarkers[]{']
    for record in records:
        lines.append('    {GameTitle::%s,0x%sull,%d,%d,%d,{%s},{%s},{%s}}, // %s'%(
            record['title'],record['identity'],record['node_count'],record['node'],record['barrel'],
            ','.join(map(number,record['position'])),','.join(map(number,record['forward'])),
            ','.join(map(number,record['up'])),record['marker']))
    lines+=['};','}', '']
    (ROOT/'src/common/weapon_muzzles.generated.h').write_text('\n'.join(lines))
    evidence=dict(source='Official per-title kit tags and selector code; runtime activation still pending',
        audit_sha256=hashlib.sha256(audit.read_bytes()).hexdigest(),records=records,refused=refused)
    (ROOT/'docs/WEAPON-MUZZLE-CATALOG-2026-09-18.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(len(records),'unambiguous authored barrel records;',len(refused),'missing/ambiguous entries retained as stock')


if __name__=='__main__':main()
