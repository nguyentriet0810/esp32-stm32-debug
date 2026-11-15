import sys,json
from pathlib import Path
import pcbnew as p
root=Path('D:/esp32_debug/03_pcb/AI_layout');b=p.LoadBoard(str(root/'AI_layout.kicad_pcb'))
sys.path.insert(0,str(root/'verification'))
import export_review_geometry as e
e.dump(root/'AI_layout.kicad_pcb',root/'verification/filled_geometry.json')
r=json.loads((root/'verification/filled_geometry.json').read_text(encoding='utf-8'));r['zones']=[]
for z in b.Zones():
 for layer in [p.F_Cu,p.B_Cu]:
  if not z.IsOnLayer(layer):continue
  q=z.GetFilledPolysList(layer)
  for i in range(q.OutlineCount()):r['zones'].append({'layer':p.LayerName(layer),'outer':e.poly(q.COutline(i)),'holes':[e.poly(q.CHole(i,j)) for j in range(q.HoleCount(i))]})
for dst,k in zip(r['pads'],(k for f in b.GetFootprints() for k in f.Pads())):dst['layers']=[p.LayerName(l) for l in [p.F_Cu,p.B_Cu] if k.IsOnLayer(l)]
(root/'verification/filled_geometry.json').write_text(json.dumps(r),encoding='utf-8')
