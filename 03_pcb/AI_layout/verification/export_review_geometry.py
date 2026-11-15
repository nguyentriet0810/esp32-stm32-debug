"""Export read-only PCB geometry for review PNGs; execute with KiCad Python."""
import argparse,json
from pathlib import Path
import pcbnew as p

def xy(v):return [p.ToMM(v.x),p.ToMM(v.y)]
def poly(q):return [xy(q.CPoint(i)) for i in range(q.PointCount())]
def shape(s):
 t=s.GetShapeStr()
 r={'type':t,'a':xy(s.GetStart()),'b':xy(s.GetEnd()),'width':p.ToMM(s.GetWidth()),'layer':s.GetLayerName()}
 if t=='Arc':r['mid']=xy(s.GetArcMid())
 if t=='Rect':r['corner_radius']=p.ToMM(s.GetCornerRadius())
 if t=='Circle':r['radius']=p.ToMM(s.GetRadius())
 if t=='Polygon':
  q=s.GetPolyShape()
  r['polygons']=[poly(q.COutline(i)) for i in range(q.OutlineCount())]
 return r

def dump(board_path,output):
 b=p.LoadBoard(str(board_path))
 r={'board':str(board_path),'tracks':[],'pads':[],'refs':[],'shapes':[]}
 for t in b.GetTracks():
  r['tracks'].append({'net':t.GetNetname(),'a':xy(t.GetStart()),'b':xy(t.GetEnd()),'layer':t.GetLayerName(),'via':isinstance(t,p.PCB_VIA),'width':p.ToMM(t.GetWidth(p.F_Cu) if isinstance(t,p.PCB_VIA) else t.GetWidth()),'drill':p.ToMM(t.GetDrillValue()) if isinstance(t,p.PCB_VIA) else 0})
 for f in b.GetFootprints():
  for k in f.Pads():
   q=k.GetEffectivePolygon(p.F_Cu,p.ERROR_INSIDE)
   if not q.OutlineCount():q=k.GetEffectivePolygon(p.B_Cu,p.ERROR_INSIDE)
   r['pads'].append({'a':xy(k.GetPosition()),'size':xy(k.GetSize()),'drill':xy(k.GetDrillSize()),'net':k.GetNetname(),'angle':k.GetOrientationDegrees(),'polygons':[poly(q.COutline(i)) for i in range(q.OutlineCount())]})
  if f.GetReference():r['refs'].append({'label':f.GetReference(),'a':xy(f.Reference().GetPosition()),'body':xy(f.GetPosition()),'locked':f.IsLocked()})
  for s in f.GraphicalItems():
   if isinstance(s,p.PCB_SHAPE) and s.GetLayerName() in ['F.SilkS','F.Fab']:
    r['shapes'].append(shape(s))
 for s in b.GetDrawings():
  if isinstance(s,p.PCB_SHAPE) and s.GetLayerName()=='Edge.Cuts':r['shapes'].append(shape(s))
 output.write_text(json.dumps(r),encoding='utf-8')
 print(output)

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('board',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args();dump(a.board.resolve(),a.output.resolve())

