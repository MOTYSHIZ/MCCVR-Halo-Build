"""Compile the production optional model observers against bounded native fixtures."""
import argparse
from pathlib import Path
from generate_haptics_runtime_fixture import extract_function

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
definitions = [
    ('game.cpp', 'void LegacyObserveReloadModel(\n        GameTitle title, uint16_t renderModelTag, uint32_t generation,\n        const BoneMatrix* source, const int32_t* boneMap)'),
    ('halo2_observer_6dof.cpp', 'void Halo2ObserveReloadModel(uint32_t tag,uint32_t nodes) noexcept'),
]
output = ['// Extracted production bodies; never edit generated fixture.']
for name, signature in definitions:
    path = a.source_root.resolve()/'src/dll'/name
    body, line = extract_function(path.read_text(encoding='utf-8-sig'), signature)
    output.extend([f'#line {line} "{path.as_posix()}"', body])
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text('\n\n'.join(output)+'\n')
