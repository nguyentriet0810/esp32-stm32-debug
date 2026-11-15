import json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
w=Path('D:/esp32_debug/03_pcb/AI_layout/verification');r=json.loads((w/'filled_geometry.json').read_text(encoding='utf-8'));scale=20;left=70;top=115
for layer,side,mirrored,plane,trace in [('F.Cu','top',False,'#95474b','#f38d83'),('B.Cu','bottom',True,'#335c96','#7aacf0')]:
 im=Image.new('RGB',(1540,1410),'#111b25');d=ImageDraw.Draw(im)
 def xy(v):return (left+((170-v[0]) if mirrored else v[0]-100)*scale,top+(v[1]-50)*scale)
 def line(a,b,color,width):
  aw=width*scale;pa,pb=xy(a),xy(b);d.line((pa,pb),fill=color,width=max(1,round(aw)))
  for x,y in [pa,pb]:d.ellipse((x-aw/2,y-aw/2,x+aw/2,y+aw/2),fill=color)
 def disc(v,diameter,color):
  x,y=xy(v);rr=diameter*scale/2;d.ellipse((x-rr,y-rr,x+rr,y+rr),fill=color)
 d.rounded_rectangle((left,top,left+1400,top+1200),radius=60,fill='#19252d')
 for z in r['zones']:
  if z['layer']!=layer:continue
  d.polygon([xy(q) for q in z['outer']],fill=plane)
  for h in z['holes']:d.polygon([xy(q) for q in h],fill='#19252d')
 for t in r['tracks']:
  if not t['via'] and t['layer']==layer:line(t['a'],t['b'],trace,t['width'])
 for k in r['pads']:
  if layer in k['layers']:
   for pp in k['polygons']:
    if pp:d.polygon([xy(q) for q in pp],fill='#dfbd73')
 for t in r['tracks']:
  if t['via']:disc(t['a'],t['width'],'#dfbd73');disc(t['a'],t['drill'],'#111b25')
 # Native drilled holes are overlaid from exported pad drill size/orientation.
 import math
 for k in r['pads']:
  dx,dy=k['drill']
  if max(dx,dy)<=0:continue
  short=min(dx,dy);long=max(dx,dy)
  if abs(long-short)<.001:disc(k['a'],short,'#111b25')
  else:
   theta=math.radians(-k['angle']+(0 if dx>dy else 90));span=(long-short)/2
   off=(span*math.cos(theta),span*math.sin(theta));a=[k['a'][0]-off[0],k['a'][1]-off[1]];b=[k['a'][0]+off[0],k['a'][1]+off[1]];line(a,b,'#111b25',short)
 d.rounded_rectangle((left,top,left+1400,top+1200),radius=60,outline='#ced7df',width=2)
 d.rectangle((0,0,1540,100),fill='#111b25')
 title='Top copper' if side=='top' else 'Bottom copper - mirrored back view'
 d.text((70,18),'AI_layout | '+title,font=ImageFont.truetype('C:/Windows/Fonts/seguisb.ttf',29),fill='white')
 d.text((70,64),'Saved GND fill contours, tracks, pads and vias | 70 x 60 mm, R3 corners',font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18),fill='#adbdcb')
 d.text((70,1340),'Ground copper fill shown; bright traces and gold pads/vias help inspect connectivity.',font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18),fill='#adbdcb')
 d.text((70,1372),'Read-only visualization from native KiCad fill polygons. No PCB edits or zone refill performed.',font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',16),fill='#8298a9')
 im.save(w/('AI_layout_filled_'+side+'.png'))
 print(w/('AI_layout_filled_'+side+'.png'))
