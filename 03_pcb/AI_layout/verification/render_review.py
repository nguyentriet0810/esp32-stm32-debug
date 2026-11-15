"""Render review PNGs from review_geometry.json (Python + Pillow).

Ground zone fills are deliberately hidden to expose signal routes. These are
engineering review graphics, not fabrication or impedance-validation outputs.
"""
import argparse,json,math
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

SPI={'/SD_MOSI':('#ffdc5e','MOSI'),'/SD_MISO':('#5be6ed','MISO'),'/SD_CS_N':('#ed8ddd','CS'),'/SD_CLK':('#b6ec77','CLK'),'/SD_SCK':('#b6ec77','SCK')}
FONT=Path('C:/Windows/Fonts/segoeui.ttf')
BOLD=Path('C:/Windows/Fonts/seguisb.ttf')
def font(n,b=False):return ImageFont.truetype(str(BOLD if b else FONT),n)

def draw_view(r,out,detail=False):
 if detail:
  box=(137.5,54.5,170.5,100.8);scale=26;left,top=58,132;size=(975,1430)
 else:
  box=(100,50,170,110);scale=22;left,top=118,130;size=(1800,1560)
 im=Image.new('RGB',size,'#111922');d=ImageDraw.Draw(im)
 def xy(pt):return (left+(pt[0]-box[0])*scale,top+(pt[1]-box[1])*scale)
 def line(a,b,color,width=1):
  ww=max(1,round(width*scale));d.line([xy(a),xy(b)],fill=color,width=ww)
  for point in [a,b]:
   xx,yy=xy(point);rr=ww/2;d.ellipse((xx-rr,yy-rr,xx+rr,yy+rr),fill=color)
 def ellipse(pos,rx,ry,fill,outline=None,width=1):
  x,y=xy(pos);d.ellipse((x-rx*scale,y-ry*scale,x+rx*scale,y+ry*scale),fill=fill,outline=outline,width=width)
 def path(pts,color,width):
  if len(pts)>1:d.line([xy(pt) for pt in pts],fill=color,width=max(1,round(width*scale)),joint='curve')
 # The actual board boundary is drawn below; use a board-sized dark panel.
 bx0,by0=xy((100,50));bx1,by1=xy((170,110))
 board_edge=next(s for s in r['shapes'] if s['layer']=='Edge.Cuts' and s['type']=='Rect')
 board_radius=board_edge.get('corner_radius',0)*scale
 d.rounded_rectangle((bx0,by0,bx1,by1),radius=board_radius,fill='#17232c')
 for s in r['shapes']:
  if s['layer']=='Edge.Cuts':continue
  color='#46515b' if s['layer']=='F.Fab' else '#5e686e'
  w=.075
  if s['type']=='Line':line(s['a'],s['b'],color,w)
  elif s['type']=='Rect':
   ax,ay=xy(s['a']);bx,by=xy(s['b']);d.rounded_rectangle((min(ax,bx),min(ay,by),max(ax,bx),max(ay,by)),radius=s.get('corner_radius',0)*scale,outline=color,width=1)
  elif s['type']=='Circle':ellipse(s['a'],s['radius'],s['radius'],None,color)
  elif s['type']=='Polygon':
   for pp in s['polygons']:path(pp+[pp[0]],color,w)
 # Each copper layer uses its conventional color. Detail highlights SPI.
 for layer in ['B.Cu','F.Cu']:
  for t in r['tracks']:
   if t['via'] or t['layer']!=layer:continue
   if detail and t['net'] in SPI:continue
   color=('#31465f' if layer=='B.Cu' else '#69494f') if detail else ('#4e8bd2' if layer=='B.Cu' else '#e26d74')
   line(t['a'],t['b'],color,t['width'])
 if detail:
  for t in r['tracks']:
   if not t['via'] and t['net'] in SPI:line(t['a'],t['b'],SPI[t['net']][0],t['width'])
 # Pad outlines are exported from KiCad effective copper polygons.
 for k in r['pads']:
  color='#d5bd79'
  if detail:color=SPI[k['net']][0] if k['net'] in SPI else '#8c815f'
  for pp in k['polygons']:
   if pp:d.polygon([xy(v) for v in pp],fill=color)
 for t in r['tracks']:
  if t['via']:
   color=SPI[t['net']][0] if detail and t['net'] in SPI else ('#a8996a' if detail else '#d9c789')
   ellipse(t['a'],t['width']/2,t['width']/2,color)
   ellipse(t['a'],t['drill']/2,t['drill']/2,'#111922')
 # Drill slots are centered and rotated along the pad's local axes.
 for k in r['pads']:
  dx,dy=k['drill']
  if max(dx,dy)<=0:continue
  short=min(dx,dy);long=max(dx,dy)
  if abs(long-short)<.001:ellipse(k['a'],dx/2,dy/2,'#101820')
  else:
   theta=math.radians(-k['angle']+(0 if dx>dy else 90));span=(long-short)/2
   off=(span*math.cos(theta),span*math.sin(theta))
   a=[k['a'][0]-off[0],k['a'][1]-off[1]];b=[k['a'][0]+off[0],k['a'][1]+off[1]]
   line(a,b,'#101820',short);ellipse(a,short/2,short/2,'#101820');ellipse(b,short/2,short/2,'#101820')
 # Component refs are review labels at their saved silkscreen positions.
 keep={'ESP32-S3','SD SCHOKET','R34','R33','R36','R37','R38','R39','R40','R41','Q1','Q2','UART','ANTEN','C15','C16'}
 for ref in r['refs']:
  if detail and ref['label'] not in keep:continue
  x,y=xy(ref['a'])
  if left-15<x<size[0]-20 and top+5<y<size[1]-90:
   d.text((x,y),ref['label'],font=font(15 if detail else 14),fill='#b1bdc6',anchor='mm',stroke_width=2,stroke_fill='#17232c')
 for s in r['shapes']:
  if s['layer']!='Edge.Cuts':continue
  if s['type']=='Rect':
   ax,ay=xy(s['a']);bx,by=xy(s['b']);d.rounded_rectangle((min(ax,bx),min(ay,by),max(ax,bx),max(ay,by)),radius=s.get('corner_radius',0)*scale,outline='#d0dae3',width=2)
  elif s['type']=='Line':line(s['a'],s['b'],'#d0dae3',.08)
 # Mask out geometry falling outside review viewport to keep the header legible.
 d.rectangle((0,0,size[0],110),fill='#111922')
 d.rectangle((0,size[1]-74,size[0],size[1]),fill='#111922')
 if detail:
  d.text((48,22),'AI_layout | SD SPI routing',font=font(29,True),fill='#f0f4f8')
  pos=48
  for net in ['/SD_MOSI','/SD_MISO','/SD_CS_N','/SD_CLK']:
   color,label=SPI[net];d.line((pos,83,pos+25,83),fill=color,width=5);d.text((pos+34,68),label,font=font(19),fill='#c9d3dc');pos+=145
  d.text((48,size[1]-61),'47.845 mm per signal path* | no added signal vias',font=font(18,True),fill='#dce5ec')
  d.text((48,size[1]-33),'*Includes 1.6 mm/via; excludes R34 body. Ground fill hidden.',font=font(14),fill='#95a7b5')
 else:
  d.text((118,22),'AI_layout | PCB routing overview',font=font(34,True),fill='#f0f4f8')
  d.text((118,72),'70 x 60 mm | 2 copper layers | board thickness 1.6 mm',font=font(19),fill='#b8c6d2')
  for x,c,s in [(1030,'#e26d74','Front copper'),(1265,'#4e8bd2','Back copper'),(1500,'#d9c789','Pads / vias')]:
   d.line((x,83,x+30,83),fill=c,width=6);d.text((x+40,68),s,font=font(18),fill='#c9d3dc')
  d.text((118,size[1]-57),'Ground copper fills hidden to expose routing. Both layers shown from the top.',font=font(18),fill='#a8bac8')
  d.text((118,size[1]-29),'Read-only visualization of AI_layout.kicad_pcb; use the saved DRC report for design-rule checks.',font=font(15),fill='#7f94a4')
 im.save(out)
 return out

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('geometry',type=Path);a=ap.parse_args();r=json.loads(a.geometry.read_text(encoding='utf-8'));w=a.geometry.parent
 print(draw_view(r,w/'AI_layout_overview.png'));print(draw_view(r,w/'AI_layout_SPI_detail.png',True))


