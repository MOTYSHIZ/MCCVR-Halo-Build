"""Inventory authored FP muzzle markers from each official title's XML export.

Read-only source evidence. This does not assign a native firing binding, assume
a default marker, or turn model movement into authority over projectile origins.
"""
import hashlib
import json
from pathlib import Path
import math
import struct
import sys
from export_reload_sources import ROOT, OUT
from reload_tag_xml import read, block, value, values
from verify_ce_first_person_tags import inspect
from verify_ce_weapon_mesh import graph_identity


def vector(node, name, count):
    result=[float(x) for x in value(node,name).split(',')]
    assert len(result)==count and all(math.isfinite(x) for x in result)
    return result


def markers(model):
    result=[]
    for group in block(model,'marker groups'):
        name=value(group,'name')
        for marker in block(group,'markers'):
            result.append(dict(name=name,
                region=int(value(marker,'region index')),
                permutation=int(value(marker,'permutation index')),
                node=int(value(marker,'node index')),
                position=vector(marker,'translation',3),
                rotation=vector(marker,'rotation',4),
                authored_scale=float(value(marker,'scale','0'))))
    return result


def ce_markers():
    # HCEEK descriptor C49A20: 0x50-byte region/permutation marker. Verify
    # field names/types before interpreting the big-endian extracted tags.
    sys.path.insert(0,str(ROOT/'out/pydeps'))
    import pefile
    kit=ROOT/'out/deps/re-tools/inputs/halo_tag_test.exe'
    raw=kit.read_bytes()
    assert hashlib.sha256(raw).hexdigest().upper()=='FC9E2B6193C6F6D9FF988278B0D39A983747F3FDBDECA7CAFADD28F1F0C53E73'
    image=pefile.PE(data=raw).get_memory_mapped_image()
    u32=lambda va:struct.unpack_from('<I',image,va-0x400000)[0]
    text=lambda va:image[va-0x400000:].split(b'\0',1)[0].decode()
    assert text(u32(0xC49A20))=='model_region_permutation_marker_block' and u32(0xC49A34)==0x50
    for address,kind,name in [(0xC499B0,0,'name^*'),(0xC499C0,36,'node index*'),
                              (0xC499E0,21,'rotation*'),(0xC499F0,18,'translation*')]:
        assert u32(address)==kind and text(u32(address+4))==name
    result=[]
    for path in sorted((ROOT/'out/ce-hands-hceek-tags/tags/weapons').glob('*/fp/*.gbxmodel')):
        data=path.read_bytes();be=lambda at:struct.unpack_from('>I',data,at)[0]
        assert data[36:40]==b'mod2' and be(0xEC)==0 and be(0x104)==1
        count=be(0xF8);assert 0<count<=64
        names=[data[0x128+i*0x9C:0x148+i*0x9C].split(b'\0',1)[0].decode() for i in range(count)]
        graph,fixture=inspect(path.with_suffix('.model_animations'))
        graph_nodes=[fixture[i:i+64] for i in range(4,len(fixture),64)]
        graph_names=[n[:32].split(b'\0',1)[0].decode() for n in graph_nodes]
        cursor=0x128+count*0x9C
        permutations=be(cursor+64);assert 0<permutations<=32
        cursor+=0x4C
        counts=[be(cursor+i*0x58+76) for i in range(permutations)]
        cursor+=permutations*0x58
        groups=[]
        for permutation,marker_count in enumerate(counts):
            assert marker_count<=64
            for _ in range(marker_count):
                name=data[cursor:cursor+32].split(b'\0',1)[0].decode()
                node=struct.unpack_from('>h',data,cursor+32)[0]
                rotation=list(struct.unpack_from('>4f',data,cursor+36))
                position=list(struct.unpack_from('>3f',data,cursor+52))
                assert 0<=node<count and abs(sum(x*x for x in rotation)-1)<.001
                assert all(math.isfinite(x) for x in position)
                groups.append(dict(name=name,region=0,permutation=permutation,node=node,
                    graph_node=graph_names.index(names[node]),position=position,rotation=rotation))
                cursor+=0x50
        result.append(dict(kit='HCEEK',model=path.relative_to(ROOT).as_posix(),
            model_sha256=hashlib.sha256(data).hexdigest(),graph=graph,
            graph_identity=f'{graph_identity(graph_nodes):016X}',nodes=count,markers=groups))
    assert len(result)==12
    print('HCEEK',len(result),'models;',sum(len(x['markers']) for x in result),'authored markers',flush=True)
    return result


def main():
    records=ce_markers()
    for kit in ['H2EK','H3EK','H3ODSTEK','HREK','H4EK']:
        definitions={w['tag']:w for w in json.loads((OUT/(kit+'-weapons.json')).read_text())}
        entries=json.loads((OUT/(kit+'-models.json')).read_text())['models']
        total=0
        for entry in entries:
            path=ROOT/entry['xml'];model=read(path)
            groups=markers(model)
            weapons=[]
            for weapon in entry['weapons']:
                source=definitions[weapon];definition=read(ROOT/source['xml'])
                barrels=[]
                for index,barrel in enumerate(block(definition,'barrels')):
                    names=values(barrel,'optional barrel marker name')
                    if not names:names=values(barrel,'barrel marker name')
                    # An empty value is deliberately not converted to a default.
                    name=names[0] if len(names)==1 else None
                    selected=[m for m in groups if m['name']==name] if name else []
                    barrels.append(dict(index=index,authored_marker=name,
                        matching_markers=selected))
                weapons.append(dict(tag=weapon,sha256=source['sha256'],barrels=barrels))
            nodes=block(model,'nodes')
            for marker in groups:
                assert -1<=marker['node']<len(nodes),(kit,entry['tag'],marker)
                assert abs(sum(x*x for x in marker['rotation'])-1)<.001,(kit,entry['tag'],marker)
            records.append(dict(kit=kit,model=entry['tag'],model_sha256=entry['sha256'],
                xml_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                checksum=int(value(model,'runtime import info checksum','0'))&0xFFFFFFFF,
                nodes=len(nodes),markers=groups,weapons=weapons))
            total+=sum(1 for m in groups if 'trigger' in m['name'] or 'muzzle' in m['name'])
        print(kit,len(entries),'models;',total,'authored trigger/muzzle markers',flush=True)
    destination=ROOT/'out/refinement-20260918/authored-muzzle-audit.json'
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(json.dumps(dict(source='Official per-title kit XML; no inferred defaults or runtime authority',models=records),indent=2)+'\n')
    print(destination)


if __name__=='__main__':main()
