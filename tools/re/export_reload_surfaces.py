"""Resolve isolated reload-part materials through each official editing kit.

Authoring output stays in ignored out/. No process memory or installed game files
are accessed. Missing/ambiguous references are errors, never guessed textures.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import struct
import zlib
from io import BytesIO
from export_reload_sources import ROOT, OUT, KITS
from reload_tag_xml import read, value, descendant_blocks
sys.path.insert(0,str(ROOT/"out/pydeps"))
sys.path.insert(0,str(ROOT/"out/video-review-deps"))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ce_bitmap(bitmap,xml,target):
    # HCEEK bitmap fields: enums/flags, three reals, four shorts, two
    # 20-byte data references, reals/shorts, then the sequence/bitmap blocks.
    # Verify the native export's processed-data checksum before decoding its
    # independently exported format/dimensions with the standard DDS decoder.
    from PIL import Image
    from reload_tag_xml import block
    data=bitmap.read_bytes()
    assert data[36:40]==b"bitm"
    tree=read(xml)
    plate,size=struct.unpack_from(">I",data,0x5c)[0],struct.unpack_from(">I",data,0x70)[0]
    start=0xac+plate
    assert size>0 and start+size<=len(data)
    pixels=data[start:start+size]
    checksum=value(tree,"processed pixel data")
    assert checksum==f"checksum: {~zlib.crc32(pixels)&0xffffffff:08x}",(bitmap,checksum)
    descriptor=block(tree,"bitmaps")[0]
    width,height=int(value(descriptor,"width")),int(value(descriptor,"height"))
    assert value(descriptor,"type")=="0,2D texture" and 0<width<=8192 and 0<height<=8192
    offset=int(value(descriptor,"pixels offset"))
    format_name=value(descriptor,"format").split(",",1)[1].upper()
    assert format_name in ("DXT1","DXT3","DXT5"),(bitmap,format_name)
    count=((width+3)//4)*((height+3)//4)*(8 if format_name=="DXT1" else 16)
    assert 0<=offset and offset+count<=len(pixels)
    fourcc=struct.unpack("<I",format_name.encode())[0]
    header=[124,0x81007,height,width,count,0,1]+[0]*11+[32,4,fourcc,0,0,0,0,0]+[0x1000,0,0,0,0]
    assert len(header)==31
    Image.open(BytesIO(b"DDS "+struct.pack("<31I",*header)+pixels[offset:offset+count])).convert("RGBA").save(target)


def export(kit):
    work=ROOT/"out/ce-hands-hceek-tags" if kit=="HCEEK" else KITS/kit if kit=="H4EK" else OUT/kit
    tags=work/"tags"
    destination=OUT/"surfaces"/kit
    destination.mkdir(parents=True,exist_ok=True)
    tool=KITS/kit/"tool.exe"
    cache={}

    def resolve(reference,extension):
        reference=reference.split(",",1)[0].replace("\\","/")
        if not reference: raise ValueError("empty reference")
        path=tags/(reference+extension)
        if path.is_file(): return path
        if "/" not in reference:
            if extension not in cache:
                names={}
                for p in tags.rglob("*"+extension): names.setdefault(p.stem,[]).append(p)
                cache[extension]=names
            matches=cache[extension].get(reference,[])
            if len(matches)==1: return matches[0]
        raise ValueError((kit,"missing or ambiguous",reference,extension))

    def run(args):
        result=subprocess.run([str(tool),*map(str,args)],cwd=work,capture_output=True,
            creationflags=subprocess.CREATE_NO_WINDOW)
        if result.returncode:
            raise RuntimeError((args,result.stdout.decode(errors="replace"),result.stderr.decode(errors="replace")))
        return result.stdout.decode(errors="replace")

    materials={}
    for geometry in sorted((OUT/"geometry"/kit).glob("*.json")):
        model=json.loads(geometry.read_text())
        if not model["triangles"]: continue
        for index in set(model["triangle_materials"]):
            assert 0<=index<len(model["shaders"]),(kit,model["tag"],index)
            reference=model["shaders"][index]
            if reference in materials: continue
            shader=resolve(reference,".shader_model" if kit=="HCEEK" else ".material" if kit=="H4EK" else ".shader")
            key=digest(shader)
            xml=destination/(key+".xml")
            if not xml.exists() or not xml.stat().st_size:
                run(["export-tag-to-xml",shader.relative_to(tags) if kit=="HCEEK" else shader,xml])
            tree=read(xml)
            parameters=[p for b in descendant_blocks(tree,"parameters")+
                descendant_blocks(tree,"material parameters") for p in b]
            # The carbine battery includes its authored emissive meter. It has
            # no diffuse map; retain its explicit self-illumination/meter art.
            names=("color_map",) if kit=="H4EK" else ("base_map","self_illum_map","meter_map")
            selected=[]
            for name in names:
                selected=[p for p in parameters if value(p,"name" if kit=="H2EK" else "parameter name","")==name]
                if selected: break
            if kit=="HCEEK":
                name="base map"
                selected=[tree]
            if len(selected)!=1:
                raise ValueError((kit,reference,"diffuse parameter",name,len(selected)))
            bitmap=resolve(value(selected[0],"base map" if kit=="HCEEK" else "bitmap"),".bitmap")
            bitmap_key=digest(bitmap)
            prefix=destination/(bitmap_key+"-")
            images=sorted(destination.glob(bitmap_key+"-*.tga"))
            if not images and kit=="HCEEK":
                bitmap_xml=destination/(bitmap_key+".bitmap.xml")
                if not bitmap_xml.exists(): run(["export-tag-to-xml",bitmap.relative_to(tags),bitmap_xml])
                target=destination/(bitmap_key+"-native.tga")
                ce_bitmap(bitmap,bitmap_xml,target)
                images=[target]
            elif not images:
                relative=bitmap.relative_to(tags).with_suffix("")
                log=run(["export-bitmap-tga",relative,prefix])
                (destination/(bitmap_key+".log")).write_text(log)
                images=sorted(destination.glob(bitmap_key+"-*.tga"))
            assert images,(kit,bitmap,"no exported bitmap")
            tint=[1.0,1.0,1.0]
            if kit=="H4EK":
                color=[p for p in parameters if value(p,"parameter name","")=="albedo_tint"]
                if color:
                    argb=[float(x) for x in value(color[0],"color").split(",")]
                    assert len(argb)==4 and all(0<=c<=1 for c in argb)
                    tint=argb[1:]
            elif kit=="H2EK" and name=="meter_map":
                color=[p for p in parameters if value(p,"name","")=="meter_on_color"]
                if color:
                    tint=[float(x) for x in value(color[0],"const color").split(",")]
                    assert len(tint)==3 and all(0<=c<=1 for c in tint)
            materials[reference]=dict(shader=shader.relative_to(tags).as_posix(),shader_sha256=key,
                bitmap=bitmap.relative_to(tags).as_posix(),bitmap_sha256=bitmap_key,
                image=images[0].relative_to(ROOT).as_posix(),image_sha256=digest(images[0]),
                parameter=name,tint=tint)
            print(kit,reference,"=>",bitmap.name,flush=True)
        (OUT/(kit+"-reload-surfaces.json")).write_text(json.dumps(materials,indent=2)+"\n")
    return materials


if __name__=="__main__":
    for kit in sys.argv[1:]: export(kit)
