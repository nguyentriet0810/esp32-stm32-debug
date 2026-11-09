"""Read-only, format-specific review of the KiCad Gerbers in ../Gerber_REV1.0.

Supports the constructs present in this set, with raster previews for inspection.
The previews and primitive comparison are not a replacement for a CAM DFM check.
"""
from pathlib import Path
from collections import Counter
import hashlib
import json
import math
import re
from PIL import Image, ImageDraw, ImageFont, ImageChops

HERE = Path(__file__).resolve().parent
ORIGINAL = HERE.parent / 'Gerber_REV1.0'
CURRENT = HERE / 'current_absolute'


def gerber(path):
    source = path.read_text()
    apertures, objects = {}, []
    x = y = 0.0
    aperture = None
    mode, polarity = 1, True
    region = None
    net = component = pad = None
    for token in re.findall(r'%[^%]*%|[^%*]+\*', source):
        token = token.strip()
        if not token:
            continue
        if token.startswith('%'):
            body = token[1:-1].rstrip('*')
            if body.startswith('ADD'):
                m = re.fullmatch(r'ADD(\d+)([A-Za-z]+)(?:,(.*))?', body)
                if not m:
                    raise ValueError(body)
                kind, values = m[2], tuple(float(v) for v in (m[3] or '').split('X') if v)
                if kind not in ('C', 'R', 'O', 'RoundRect'):
                    raise ValueError(kind)
                apertures[int(m[1])] = (kind, values)
            elif body == 'LPD':
                polarity = True
            elif body == 'LPC':
                polarity = False
            elif body.startswith('TO.N,'):
                net = body[5:]
            elif body.startswith('TO.C,'):
                component = body[5:]
            elif body.startswith('TO.P,'):
                pad = body[5:]
            elif body == 'TD':
                net = component = pad = None
            elif body.startswith(('AMRoundRect', 'TF.', 'TA.', 'TD.', 'TO.')) or body in ('MOMM', 'FSLAX46Y46'):
                pass
            else:
                raise ValueError(f'Unsupported parameter {body[:80]}')
            continue
        body = token.rstrip('*').strip()
        if body.startswith('G04') or body in ('G75', 'M02'):
            continue
        if body in ('G01', 'G02', 'G03'):
            mode = int(body[1:])
            continue
        if body == 'G36':
            region = []
            continue
        if body == 'G37':
            objects.append(dict(kind='region', points=region, dark=polarity, net=net, component=component, pad=pad))
            region = None
            continue
        if re.fullmatch(r'D\d+', body):
            aperture = apertures[int(body[1:])]
            continue
        if body.startswith(('X', 'Y', 'G01', 'G02', 'G03')):
            if body.startswith('G'):
                mode = int(body[1:3])
                body = body[3:]
            fields = dict(re.findall(r'([XYIJD])([+-]?\d+)', body))
            if not fields or ''.join(k+v for k,v in fields.items()) != body:
                raise ValueError(body)
            nx, ny = float(fields.get('X', x*1e6))/1e6, float(fields.get('Y', y*1e6))/1e6
            operation = int(fields['D'])
            meta = dict(dark=polarity, net=net, component=component, pad=pad)
            if operation == 2:
                if region is not None:
                    region.append([(nx, ny)])
            elif operation == 3:
                objects.append(dict(kind='flash', at=(nx, ny), aperture=aperture, **meta))
            elif operation == 1:
                if mode == 1:
                    points = [(x, y), (nx, ny)]
                else:
                    cx, cy = x+float(fields.get('I', 0))/1e6, y+float(fields.get('J', 0))/1e6
                    a, b = math.atan2(y-cy, x-cx), math.atan2(ny-cy, nx-cx)
                    delta = (b-a) % (2*math.pi) if mode == 3 else -((a-b) % (2*math.pi))
                    if abs(delta) < 1e-9:
                        delta = 2*math.pi if mode == 3 else -2*math.pi
                    radius = math.hypot(x-cx, y-cy)
                    count = max(8, math.ceil(abs(delta)*radius/0.01))
                    points = [(cx+radius*math.cos(a+delta*k/count), cy+radius*math.sin(a+delta*k/count)) for k in range(count+1)]
                if region is not None:
                    region[-1].extend(points[1:])
                else:
                    objects.append(dict(kind='line', points=points, aperture=aperture, **meta))
            else:
                raise ValueError(operation)
            x, y = nx, ny
            continue
        raise ValueError(f'Unsupported command {body[:80]}')
    if region is not None or not source.rstrip().endswith('M02*'):
        raise ValueError('Missing end of region/file')
    return objects


def geometry_key(o):
    return json.dumps({k:v for k,v in o.items() if k not in ('net', 'component', 'pad')}, sort_keys=True)


def drill(path):
    tools, objects = {}, []
    tool = None
    position = None
    route = False
    for line in path.read_text().splitlines():
        if m := re.fullmatch(r'T(\d+)C([\d.]+)', line):
            tools[int(m[1])] = float(m[2])
        elif m := re.fullmatch(r'T(\d+)', line):
            tool = int(m[1])
        elif line == 'M15':
            route = True
        elif line == 'M16':
            route = False
        elif m := re.fullmatch(r'(G00|G01)?X([\d.-]+)Y([\d.-]+)', line):
            p = (float(m[2]), float(m[3]))
            if m[1] == 'G00':
                position = p
            elif route and m[1] == 'G01':
                objects.append(dict(kind='slot', start=position, end=p, diameter=tools[tool], tool=tool))
            else:
                objects.append(dict(kind='hole', at=p, diameter=tools[tool], tool=tool))
            position = p
    return objects


def raster(objects, bounds=(98, -112, 172, -48), scale=30):
    xmin,ymin,xmax,ymax = bounds
    result = Image.new('L', (round((xmax-xmin)*scale),round((ymax-ymin)*scale)))
    draw = ImageDraw.Draw(result)
    def xy(p):
        return ((p[0]-xmin)*scale, (ymax-p[1])*scale)
    for o in objects:
        color = 255 if o['dark'] else 0
        if o['kind'] == 'region':
            for contour in o['points']:
                draw.polygon([xy(p) for p in contour], fill=color)
        elif o['kind'] == 'line':
            kind, values = o['aperture']
            if kind != 'C':
                raise ValueError('Non circular drawing aperture')
            w = max(1, round(values[0]*scale))
            pts = [xy(p) for p in o['points']]
            draw.line(pts, fill=color, width=w, joint='curve')
            for px,py in [pts[0],pts[-1]]:
                draw.ellipse((px-w/2,py-w/2,px+w/2,py+w/2), fill=color)
        else:
            kind, values = o['aperture']
            px,py = xy(o['at'])
            if kind == 'RoundRect':
                radius = values[0]
                xs,ys = values[1:9:2],values[2:9:2]
                box = (px+(min(xs)-radius)*scale, py-(max(ys)+radius)*scale,
                       px+(max(xs)+radius)*scale, py-(min(ys)-radius)*scale)
                draw.rounded_rectangle(box, radius=radius*scale, fill=color)
            else:
                w,h = values[0], values[1] if kind != 'C' else values[0]
                box = (px-w*scale/2,py-h*scale/2,px+w*scale/2,py+h*scale/2)
                if kind == 'C':
                    draw.ellipse(box, fill=color)
                elif kind == 'R':
                    draw.rectangle(box, fill=color)
                else:
                    draw.rounded_rectangle(box, radius=min(w,h)*scale/2, fill=color)
    return result


def compose(front, back, silk, outline, holes, offset=(100,-110)):
    result = Image.new('RGB', front.size, '#101a26')
    result.paste('#395b92', mask=back)
    result.paste('#dca548', mask=front)
    result.paste('#f1f4f9', mask=silk)
    result.paste('#4ce6c3', mask=outline)
    draw = ImageDraw.Draw(result)
    def xy(p):
        return ((p[0]+offset[0]-98)*30,(-48-p[1]-offset[1])*30)
    for o in holes:
        w=max(1,round(o['diameter']*30))
        if o['kind'] == 'hole':
            px,py=xy(o['at'])
            draw.ellipse((px-w/2,py-w/2,px+w/2,py+w/2),fill='#05090e',outline='#ff6584',width=2)
        else:
            draw.line([xy(o['start']),xy(o['end'])],fill='#ff6584',width=w)
    return result


def main():
    old, new, summary = {}, {}, {}
    for p in sorted(ORIGINAL.glob('*.gbr')):
        a,b = gerber(p),gerber(CURRENT/p.name)
        old[p.stem],new[p.stem] = a,b
        ca,cb = Counter(map(geometry_key,a)),Counter(map(geometry_key,b))
        ra,rb = raster(a),raster(b)
        diff=ImageChops.difference(ra,rb)
        summary[p.name] = dict(objects_original=len(a), objects_current=len(b),
            primitives_removed=sum((ca-cb).values()), primitives_added=sum((cb-ca).values()),
            raster_difference_pixels=sum(diff.histogram()[1:]), original_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
            aperture_draw_widths_mm=sorted({o['aperture'][1][0] for o in a if o['kind']=='line'}))
        if diff.getbbox():
            diff.save(HERE/(p.stem+'_difference.png'))
    all_holes=[]
    drills={}
    for p in sorted(ORIGINAL.glob('*.drl')):
        a,b=drill(p),drill(HERE/'current_drill_plot'/p.name)
        drills[p.name]=dict(count=len(a),round_holes=sum(o['kind']=='hole' for o in a),
            slots=sum(o['kind']=='slot' for o in a),diameter_counts=dict(Counter(o['diameter'] for o in a)),
            same_as_current=a==b, original_sha256=hashlib.sha256(p.read_bytes()).hexdigest())
        all_holes.extend(a)
    summary['drills']=drills
    summary['origin_mismatch']=dict(gerber_outline_x_mm=[100,170],gerber_outline_y_mm=[-110,-50],
        drill_to_gerber_translation_mm=[100,-110],example_npth_drill=[5,5],example_matching_gerber=[105,-105])
    (HERE/'gerber_audit.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    renders={key:raster(value) for key,value in old.items()}
    compose(renders['schematic-F_Cu'],renders['schematic-B_Cu'],renders['schematic-F_Silkscreen'],
            renders['schematic-Edge_Cuts'],all_holes).save(HERE/'gerber_top_drills_translated_for_review.png')
    compose(renders['schematic-B_Cu'],renders['schematic-F_Cu'],renders['schematic-B_Silkscreen'],
            renders['schematic-Edge_Cuts'],all_holes).save(HERE/'gerber_bottom_drills_translated_for_review.png')
    # Shared coordinate axes show the uncorrected mismatch without auto-alignment.
    bounds=(-5,-115,175,65)
    image=Image.new('RGB',(1080,1080),'#101a26')
    image.paste('#4ce6c3',mask=raster(old['schematic-Edge_Cuts'],bounds,6))
    draw=ImageDraw.Draw(image)
    for o in all_holes:
        points=[o['at']] if o['kind']=='hole' else [o['start'],o['end']]
        for x,y in points:
            px,py=(x+5)*6,(65-y)*6
            r=max(2,o['diameter']*3)
            draw.ellipse((px-r,py-r,px+r,py+r),fill='#ff6584')
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',22)
    draw.text((55,12),'Drill files (as supplied)',fill='#ff6584',font=font)
    draw.text((630,700),'Gerber board outline',fill='#4ce6c3',font=font)
    draw.text((40,1035),'Shared axes in mm; no automatic alignment',fill='white',font=font)
    image.save(HERE/'origin_mismatch.png')
    print(json.dumps(summary,indent=2))


if __name__ == '__main__':
    main()
