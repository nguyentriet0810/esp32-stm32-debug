"""Recheck the user's regenerated files without changing their content."""
from pathlib import Path
from collections import Counter
import hashlib
import json
import math
import sys
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import audit_gerbers as audit

SOURCE = HERE.parent.parent / 'Gerber_REV1.0'
REFERENCE = HERE / 'reference'


def normalized(value):
    if isinstance(value, float):
        return round(value, 6)
    if isinstance(value, (tuple, list)):
        return [normalized(v) for v in value]
    if isinstance(value, dict):
        return {k:normalized(v) for k,v in value.items() if k not in ('net', 'component', 'pad')}
    return value


def counter(objects):
    return Counter(json.dumps(normalized(o), sort_keys=True) for o in objects)


def main():
    hashes = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in SOURCE.iterdir() if p.is_file()}
    layers, comparison = {}, {}
    for p in sorted(SOURCE.glob('*.gbr')):
        actual, expected = audit.gerber(p), audit.gerber(REFERENCE/p.name)
        a,b = counter(actual),counter(expected)
        layers[p.stem] = actual
        comparison[p.name] = dict(objects=len(actual),same_geometry_as_saved_pcb=a==b,
                                 extra_primitives=sum((a-b).values()),missing_primitives=sum((b-a).values()))
    holes = []
    drill_summary = {}
    for p in sorted(SOURCE.glob('*.drl')):
        actual,expected = audit.drill(p),audit.drill(REFERENCE/p.name)
        holes.extend(actual)
        drill_summary[p.name] = dict(count=len(actual),round_holes=sum(o['kind']=='hole' for o in actual),
                                    slots=sum(o['kind']=='slot' for o in actual),same_as_saved_pcb=actual==expected)
    pth = audit.drill(SOURCE/'schematic-PTH.drl')
    alignment = {}
    for layer in ('schematic-F_Cu','schematic-B_Cu'):
        flashes = [o['at'] for o in layers[layer] if o['kind']=='flash' and o['dark']]
        mismatches = []
        max_error = 0
        for hole in pth:
            at = hole['at'] if hole['kind']=='hole' else tuple((a+b)/2 for a,b in zip(hole['start'],hole['end']))
            error = min(math.dist(at,p) for p in flashes)
            max_error = max(max_error,error)
            if error > 0.002:
                mismatches.append(dict(hole=hole,nearest_copper_flash_center_distance_mm=error))
        alignment[layer] = dict(checked_pth_positions=len(pth),maximum_center_error_mm=max_error,mismatches=mismatches)
    bounds,scale = (-2,-2,72,62),24
    raster = {name:audit.raster(objects,bounds,scale) for name,objects in layers.items()}
    for side,other in [('F','B'),('B','F')]:
        image = Image.new('RGB',raster['schematic-F_Cu'].size,'#101a26')
        for layer,color in [(f'schematic-{other}_Cu','#395b92'),(f'schematic-{side}_Cu','#dca548'),
                            (f'schematic-{side}_Silkscreen','#f1f4f9'),('schematic-Edge_Cuts','#4ce6c3')]:
            image.paste(color,mask=raster[layer])
        draw = ImageDraw.Draw(image)
        def xy(p):
            return ((p[0]+2)*scale,(62-p[1])*scale)
        for hole in holes:
            width=max(1,round(hole['diameter']*scale))
            if hole['kind']=='hole':
                x,y=xy(hole['at'])
                draw.ellipse((x-width/2,y-width/2,x+width/2,y+width/2),fill='#05090e',outline='#ff6584',width=2)
            else:
                pts=[xy(hole['start']),xy(hole['end'])]
                draw.line(pts,fill='#ff6584',width=width)
                for x,y in pts:
                    draw.ellipse((x-width/2,y-width/2,x+width/2,y+width/2),fill='#ff6584')
        image.save(HERE/('top_actual_alignment.png' if side=='F' else 'bottom_actual_alignment.png'))
    drc=json.loads((HERE/'drc.json').read_text())
    summary=dict(gerber_geometry=comparison,drill_geometry=drill_summary,pth_alignment=alignment,
                 board_outline_mm=dict(x=[0,70],y=[0,60],size=[70,60]),
                 drc_errors=sum(v['severity']=='error' for v in drc['violations']),
                 drc_warning_types=dict(Counter(v['type'] for v in drc['violations'])),
                 unconnected_items=len(drc['unconnected_items']),schematic_parity_warnings=len(drc['schematic_parity']),
                 source_sha256=hashes,source_content_preserved=all(hashlib.sha256((SOURCE/name).read_bytes()).hexdigest()==digest for name,digest in hashes.items()))
    (HERE/'result.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    main()
