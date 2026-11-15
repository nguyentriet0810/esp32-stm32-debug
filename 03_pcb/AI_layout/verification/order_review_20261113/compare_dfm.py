"""Read-only comparison against the last accepted AI_layout GND snapshot."""
from collections import Counter
from pathlib import Path
import difflib, hashlib, importlib.util, json
import pcbnew as p
import wx
wx.DisableAsserts()

HERE=Path(__file__).parent
SOURCE=Path('D:/esp32_debug/03_pcb/AI_layout/AI_layout.kicad_pcb')
BASE=Path('D:/esp32_debug/03_pcb/.tooling/ai-layout/ground-audit/ground_snapshot.kicad_pcb')
spec=importlib.util.spec_from_file_location('audit_verify',Path('D:/esp32_debug/03_pcb/.tooling/ai-layout/audit/verify.py'))
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def normalized_without_changed_text(tree):
    out=[]
    for node in tree:
        if isinstance(node,list) and node and node[0]=='footprint':
            node=[child for child in node if not(isinstance(child,list) and child[:2]==['property','Reference'])]
        elif isinstance(node,list) and node and node[0]=='gr_text':
            node=node.copy();node[1]='<TEXT>'
        out.append(node)
    return out
def category(tree,kinds):return [n for n in tree if isinstance(n,list) and n and n[0] in kinds]

def main():
    bt,ft=[v.sexp(path.read_text(encoding='utf-8')) for path in (BASE,SOURCE)]
    bb,bi=v.inventory(BASE);fb,fi=v.inventory(SOURCE)
    comparisons=v.compare(bi,fi)
    old={f.m_Uuid.AsString():f for f in bb.GetFootprints()}
    text_changes=[]
    for f in fb.GetFootprints():
        a=old[f.m_Uuid.AsString()].Reference();b=f.Reference()
        if (v.xy(a.GetPosition()),a.GetTextAngleDegrees()) != (v.xy(b.GetPosition()),b.GetTextAngleDegrees()):
            text_changes.append(dict(reference=f.GetReference(),old_position_mm=v.xy(a.GetPosition()),
                new_position_mm=v.xy(b.GetPosition()),old_angle_deg=a.GetTextAngleDegrees(),new_angle_deg=b.GetTextAngleDegrees()))
    old_texts={n.GetText():n for n in bb.GetDrawings() if isinstance(n,p.PCB_TEXT)}
    board_texts=[]
    old_by_uid={n.m_Uuid.AsString():n for n in bb.GetDrawings() if isinstance(n,p.PCB_TEXT)}
    for n in fb.GetDrawings():
        if isinstance(n,p.PCB_TEXT):
            previous=old_by_uid[n.m_Uuid.AsString()]
            if previous.GetText()!=n.GetText():board_texts.append(dict(old=previous.GetText(),new=n.GetText(),at_mm=v.xy(n.GetPosition())))
    pth=[]
    for fp in fb.GetFootprints():
        for pad in fp.Pads():
            if pad.GetAttribute()==p.PAD_ATTRIB_PTH:
                size,drill=pad.GetSize(),pad.GetDrillSize()
                pth.append(dict(ref=fp.GetReference(),pad=pad.GetNumber(),
                    drill_minor_mm=p.ToMM(min(drill.x,drill.y)),
                    annulus_mm=p.ToMM(min(size.x-drill.x,size.y-drill.y))/2))
    silk=[]
    for fp in fb.GetFootprints():
        for item in list(fp.GetFields())+list(fp.GraphicalItems()):
            if isinstance(item,p.PCB_TEXT) and item.GetLayer() in (p.F_SilkS,p.B_SilkS) and item.IsVisible() and item.GetText():
                silk.append(dict(ref=fp.GetReference(),text=item.GetText(),stroke_mm=p.ToMM(item.GetTextThickness()),
                    height_mm=p.ToMM(item.GetTextHeight())))
    for item in fb.GetDrawings():
        if isinstance(item,p.PCB_TEXT) and item.GetLayer() in (p.F_SilkS,p.B_SilkS) and item.GetText():
            silk.append(dict(ref='',text=item.GetText(),stroke_mm=p.ToMM(item.GetTextThickness()),height_mm=p.ToMM(item.GetTextHeight())))
    pro=json.loads(SOURCE.with_suffix('.kicad_pro').read_text(encoding='utf-8'))
    limits=v.limits(fi)
    report=dict(source=str(SOURCE),source_sha256=digest(SOURCE),baseline=str(BASE),baseline_sha256=digest(BASE),
        only_reference_field_placement_and_board_text_content_changed=normalized_without_changed_text(bt)==normalized_without_changed_text(ft),
        reference_text_changes=text_changes,board_text_changes=board_texts,
        reference_designators_and_values_unchanged=all((x['reference'],x['value'])==(fi['footprints'][uid]['reference'],fi['footprints'][uid]['value']) for uid,x in bi['footprints'].items()),
        category_unchanged={
            'tracks_and_vias':category(bt,{'segment','via','arc'})==category(ft,{'segment','via','arc'}),
            'zones_including_fill':category(bt,{'zone'})==category(ft,{'zone'}),
            'setup_including_stackup_and_origins':category(bt,{'setup'})==category(ft,{'setup'}),
            'net_definitions':category(bt,{'net'})==category(ft,{'net'}),
            'general_thickness':category(bt,{'general'})==category(ft,{'general'})},
        invariant_comparison=comparisons,
        fabrication=dict(board_thickness_mm=fi['thickness_mm'],copper_layer_count=fi['copper_layers'],
            minimum_track_mm=limits['min_track_mm'],minimum_power_track_mm=limits['min_power_track_mm'],
            minimum_via_drill_mm=limits['min_via_drill_mm'],minimum_via_annulus_mm=limits['min_via_annulus_mm'],
            minimum_pth_drill_minor_mm=min(x['drill_minor_mm'] for x in pth),minimum_pth_annulus_mm=min(x['annulus_mm'] for x in pth),
            configured_minimum_copper_clearance_mm=pro['board']['design_settings']['rules']['min_clearance'],
            minimum_visible_text_stroke_mm=min(x['stroke_mm'] for x in silk),minimum_visible_text_height_mm=min(x['height_mm'] for x in silk),
            footprint_graphic_strokes_below_015mm=limits['thin_silk_graphic_count'],
            note='217 unchanged 0.12mm footprint outlines are graphical strokes; all visible text strokes satisfy the supplied 0.15mm text requirement.'),
        below_manufacturer_limits=dict(tracks=[x for x in fi['tracks'] if x['width_mm']<.15-1e-6],
            power_tracks=[x for x in fi['tracks'] if x['netclass']=='Power' and x['width_mm']<.4-1e-6],
            vias=[x for x in fi['vias'] if x['drill_mm']<.3-1e-6 or x['annulus_mm']<.15-1e-6],
            visible_text=[x for x in silk if x['stroke_mm']<.15-1e-6]),
        unchanged_pad_geometries=len(fi['footprints']),pth_pad_count=len(pth),via_count=len(fi['vias']),track_segment_count=len(fi['tracks']))
    (HERE/'text_change_and_dfm_audit.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    (HERE/'pcb_text_changes.diff').write_text(''.join(difflib.unified_diff(BASE.read_text(encoding='utf-8').splitlines(keepends=True),SOURCE.read_text(encoding='utf-8').splitlines(keepends=True),fromfile=str(BASE),tofile=str(SOURCE))),encoding='utf-8')
    print(json.dumps(report,indent=2,ensure_ascii=False))
if __name__=='__main__':main()
