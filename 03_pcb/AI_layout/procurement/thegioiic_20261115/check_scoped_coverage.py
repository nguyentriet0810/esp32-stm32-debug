"""Read-only QA of the user-scoped shopping report against the saved PCB."""
from collections import Counter
from pathlib import Path
from urllib.parse import urlparse
import hashlib,json
import pcbnew as p
import wx
wx.DisableAsserts()

HERE=Path(__file__).parent
PCB=Path('D:/esp32_debug/03_pcb/AI_layout/AI_layout.kicad_pcb')
EXCLUDE={'OLED','SW1','SWDIO','UART'}
SD={'SD SOCKET','R33','R34','R36','R37','R38','R39','R40','R41','C15','C16'}
KEEP={'R14','R15','R23','R24','R25','R26','C10','C1','C2','C3','C4','D3','D5','D6','D7','D8','D12','D14','D1','D13'}
def load(name):return json.loads((HERE/name).read_text(encoding='utf-8'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    initial_hash=sha(PCB)
    board=p.LoadBoard(str(PCB))
    parts={f.GetReference():dict(value=f.GetValue(),dnp=f.IsDNP(),footprint=f.GetFPIDAsString())
           for f in board.GetFootprints() if f.GetReference() and not f.IsExcludedFromBOM()}
    report=load('shopping_list.json');scope=load('scope.json');rows=report['items']
    refs=[ref for row in rows for ref in row['refs']]
    expected=set(parts)-EXCLUDE
    counts=Counter(refs)
    q_errors=[]
    for row in rows:
        actual=dict(base_quantity=sum(not parts[r]['dnp'] for r in row['refs']),
            sd_option_additional_quantity=sum(r in SD for r in row['refs']),
            other_dnp_quantity=sum(parts[r]['dnp'] and r not in SD for r in row['refs']))
        for key,value in actual.items():
            if row[key]!=value:q_errors.append(dict(part=row['required_part'],field=key,expected=value,actual=row[key]))
    md=(HERE/'THEGIOIIC_BOM.md').read_text(encoding='utf-8')
    table_lines=[s for s in md.splitlines() if s.startswith('| ') and ' — 'in s]
    md_errors=[]
    money=lambda n:'—'if n is None else f'{int(n):,}'.replace(',','.')+' đ'
    number=lambda n:'—'if n is None else f'{int(n):,}'.replace(',','.')
    statuses={'available':'Có hàng; khớp tiêu chí đã đối chiếu','out_of_stock':'Đúng loại, hết hàng',
        'not_found_exact':'Chưa tìm đúng mã/thông số','mechanical_confirmation_required':'Chỉ là ứng viên; chưa chốt lắp vừa'}
    for row in rows:
        need=(row['required_part']+' — '+', '.join(row['refs'])).replace('|','/')
        matches=[s for s in table_lines if s.split(' | ')[0]=='| '+need]
        if len(matches)!=1:
            md_errors.append(dict(part=row['required_part'],reason='missing/duplicate Markdown row'));continue
        columns=matches[0].split(' | ')
        desired=[str(row['base_quantity']),str(row['sd_option_additional_quantity']),str(row['other_dnp_quantity'])]
        if columns[1:4]!=desired:md_errors.append(dict(part=row['required_part'],reason='quantity mismatch'))
        if row['url'] and ']('+row['url']+')'not in columns[4]:md_errors.append(dict(part=row['required_part'],reason='product link mismatch'))
        if columns[5]!=money(row['unit_price_vnd'])or columns[6]!=number(row['inventory']):md_errors.append(dict(part=row['required_part'],reason='price/stock mismatch'))
        if columns[7]!=number(row['minimum_order_quantity'])+' / '+number(row['order_multiple']):md_errors.append(dict(part=row['required_part'],reason='MOQ/multiple mismatch'))
        if columns[8].removesuffix(' |')!=statuses[row['status']]:md_errors.append(dict(part=row['required_part'],reason='status mismatch'))
    source_mismatches=[]
    passive_by_refs={frozenset(x['base_refs']+x['sd_option_refs']+x['other_dnp_refs']):x for x in load('passives.json')['items']}
    for row in rows:
        if row['category']=='Điện trở / tụ':
            src=passive_by_refs[frozenset(row['refs'])]
            pairs={'url':'url','unit_price_vnd':'live_unit_price_vnd','inventory':'live_inventory','minimum_order_quantity':'live_moq','order_multiple':'live_multiple'}
            for outkey,inkey in pairs.items():
                if row.get(outkey)!=src.get(inkey):source_mismatches.append(dict(part=row['required_part'],field=outkey))
    status_counts=dict(Counter(x['status']for x in rows))
    check=dict(scope_exclusions_exact=set(scope['excluded_pcb_references'])==EXCLUDE,
        report_scope_matches_file=report['scope']==scope,purchase_group_count_25=len(rows)==25,
        covered_exact_remaining_55_refs=set(refs)==expected and len(expected)==55,
        excluded_refs_absent=not(set(refs)&EXCLUDE),no_duplicate_ref_coverage=all(n==1 for n in counts.values()),
        required_associated_passives_and_protection_retained=KEEP.issubset(set(refs)),
        quantities_match_actual_pcb=not q_errors,
        base41_sd11_otherdnp3=[sum(row[k]for row in rows)for k in ('base_quantity','sd_option_additional_quantity','other_dnp_quantity')]==[41,11,3],
        status_counts_expected=status_counts==dict(available=17,not_found_exact=2,out_of_stock=2,mechanical_confirmation_required=4),
        markdown_has_exact25_purchase_rows=len(table_lines)==25,markdown_matches_json=not md_errors,
        passive_links_prices_quantities_match_source=not source_mismatches,
        all_product_links_primary_retailer=all(not row['url']or urlparse(row['url']).hostname in ('www.thegioiic.com','thegioiic.com') for row in rows),
        source_pcb_hash_matches=initial_hash==report['source_pcb_sha256']==load('pcb_bom.json')['pcb_sha256'],
        source_pcb_unchanged_during_qa=initial_hash==sha(PCB))
    output=dict(date='2026-11-15',review_type='Read-only scoped procurement QA; no PCB/cart/order edits',
        source_pcb_sha256=initial_hash,passed=all(check.values()),checks=check,
        scope_excluded_refs=sorted(EXCLUDE),covered_ref_count=len(expected),purchase_group_count=len(rows),
        base_quantity=41,sd_optional_dnp_quantity=11,other_dnp_quantity=3,status_counts=status_counts,
        retained_associated_refs=sorted(KEEP),quantity_errors=q_errors,markdown_errors=md_errors,source_mismatches=source_mismatches,
        outstanding_gaps=[dict(refs=row['refs'],required=row['required_part'],status=row['status']) for row in rows if row['status']!='available'],
        note='Excluded only SSD1306/OLED module, rotary encoder and header purchase groups. OLED pull-ups, encoder support, all supplies and protections remain. Price and inventory are prior live snapshots, not refreshed in this QA.')
    (HERE/'scoped_coverage_check.json').write_text(json.dumps(output,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps(output,indent=2,ensure_ascii=True))
if __name__=='__main__':main()
