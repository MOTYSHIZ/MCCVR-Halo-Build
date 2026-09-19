"""Pack only the texture regions used by the isolated reload assemblies."""
import hashlib
import json
import math
import sys
from io import BytesIO
from export_reload_sources import ROOT, OUT
sys.path.insert(0,str(ROOT/"out/video-review-deps"))
from PIL import Image


class Atlas:
    width=1024
    def __init__(self):
        self.x=16;self.y=0;self.row=16;self.tiles=[];self.cache={};self.evidence=[]
        self.materials={}

    def add(self,kit,model):
        if not model["triangles"]: return []
        if kit not in self.materials:
            self.materials[kit]=json.loads((OUT/(kit+"-reload-surfaces.json")).read_text())
        result=[None]*len(model["triangles"])
        for material in sorted(set(model["triangle_materials"])):
            reference=model["shaders"][material]
            source=self.materials[kit][reference]
            path=ROOT/source["image"]
            assert hashlib.sha256(path.read_bytes()).hexdigest()==source["image_sha256"]
            image=Image.open(path).convert("RGB")
            tint=source.get("tint",[1,1,1])
            image=Image.merge("RGB",tuple(c.point([round(i*tint[n]) for i in range(256)])
                for n,c in enumerate(image.split())))
            image.thumbnail((512,512),Image.Resampling.LANCZOS)
            chosen=[i for i,m in enumerate(model["triangle_materials"]) if m==material]
            coords=[uv for i in chosen for uv in model["triangle_uvs"][i]]
            assert coords and all(len(uv)==2 and all(math.isfinite(v) for v in uv) for uv in coords)
            w,h=image.size
            left=math.floor(min(uv[0] for uv in coords)*w)-4
            top=math.floor(min(uv[1] for uv in coords)*h)-4
            right=math.ceil(max(uv[0] for uv in coords)*w)+4
            bottom=math.ceil(max(uv[1] for uv in coords)*h)+4
            tw,th=right-left,bottom-top
            assert 0<tw<self.width and 0<th<self.width,(kit,reference,(tw,th))
            key=(source["image_sha256"],tuple(tint),left,top,right,bottom,w,h)
            if key not in self.cache:
                # Authored weapon UVs can cross 0/1 slightly. Bake their repeat
                # here, then use a clamped atlas sampler with a four-pixel gutter.
                tile=Image.new("RGB",(tw,th))
                for by in range(math.floor(top/h),math.ceil(bottom/h)):
                    for bx in range(math.floor(left/w),math.ceil(right/w)):
                        tile.paste(image,(bx*w-left,by*h-top))
                aw=(tw+3)//4*4;ah=(th+3)//4*4
                if self.x+aw>self.width:self.y+=self.row;self.x=0;self.row=0
                assert self.y+ah<=8192
                x,y=self.x,self.y;self.x+=aw;self.row=max(self.row,ah)
                self.tiles.append((tile,x,y));self.cache[key]=(x,y)
            x,y=self.cache[key]
            for i in chosen:
                result[i]=[(x+u*w-left,y+v*h-top) for u,v in model["triangle_uvs"][i]]
            self.evidence.append(dict(title=kit,model=model["tag"],material=reference,
                **source,crop=[left,top,right,bottom],scaled_image=[w,h],atlas_position=[x,y]))
        assert all(result)
        return result

    def finish(self):
        height=1<<(self.y+self.row-1).bit_length()
        image=Image.new("RGB",(self.width,height),(255,255,255))
        for tile,x,y in self.tiles:image.paste(tile,(x,y))
        encoded=BytesIO();image.save(encoded,format="DDS",pixel_format="DXT1")
        dds=encoded.getvalue();assert dds[:4]==b"DDS " and dds[84:88]==b"DXT1"
        pixels=dds[128:];assert len(pixels)==self.width*height//2
        lines=["// Generated isolated reload-part atlas, standard BC1 blocks.","#pragma once",
            "#include <cstdint>","namespace weapon_model {",
            f"inline constexpr unsigned kSurfaceWidth={self.width},kSurfaceHeight={height};",
            "inline constexpr uint8_t kSurfaceBlocks[]{"]
        lines += ["    "+",".join(str(v) for v in pixels[i:i+32])+"," for i in range(0,len(pixels),32)]
        lines += ["};","}",""]
        (ROOT/"src/common/weapon_surfaces.generated.h").write_text("\n".join(lines))
        (ROOT/"docs/RELOAD-SURFACE-EVIDENCE-2026-09-18.json").write_text(json.dumps(dict(
            dimensions=[self.width,height],bc1_sha256=hashlib.sha256(pixels).hexdigest(),
            materials=self.evidence),indent=2)+"\n")
        image.save(OUT/"reload-surface-atlas.png")
        return self.width,height
