from pathlib import Path
from collections import Counter
import hashlib
import json

root = Path(__file__).parent
load = lambda name: json.loads((root/name).read_text(encoding='utf-8'))
pcb = load('pcb_bom.json')
scope = load('scope.json')
excluded_refs = set(scope['excluded_pcb_references'])
modules, semis, passives, connectors = [load(n) for n in ('modules.json','semiconductors.json','passives.json','connectors.json')]
rows = []

def row(category, name, refs, base, sd=0, dnp=0, offer=None, status='available', note=''):
    offer = offer or {}
    rows.append(dict(category=category, required_part=name, refs=refs, base_quantity=base,
        sd_option_additional_quantity=sd, other_dnp_quantity=dnp,
        status=status, url=offer.get('url'), title=offer.get('title'),
        supplier_code=offer.get('code'), unit_price_vnd=offer.get('price'),
        inventory=offer.get('stock'), minimum_order_quantity=offer.get('moq'),
        order_multiple=offer.get('multiple'), notes=note))

for m in modules['items']:
    row('Module',m['bom_value'],m['refs'],1,offer=dict(url=m['url'],title=m['title'],code=m['retailer_code'],
        price=m['unit_price_vnd'],stock=m['stock_seen'],moq=m['minimum_order_quantity'],multiple=m['order_multiple']),note=m['notes'])

for item in semis['items']:
    offers=item['offers']
    offer=offers[0] if offers else {}
    row('Bán dẫn',item['bom_value']+' / '+item['required_package'],item['references'],item['quantity_per_board'],
        offer=dict(url=offer.get('url'),title=offer.get('title'),code=offer.get('retailer_code'),
          price=offer.get('unit_price_vnd'),stock=offer.get('inventory_units'),moq=offer.get('minimum_order_quantity'),multiple=offer.get('order_multiple')),
        status='available' if offers else 'not_found_exact',note=item['notes'])
    rows[-1]['alternative_offers']=offers[1:]

for item in passives['items']:
    status='not_found_exact' if not item['url'] else ('out_of_stock' if item.get('live_inventory')==0 else 'available')
    row('Điện trở / tụ',item['required_value']+' / '+item['required_package'],
        item['base_refs']+item['sd_option_refs']+item['other_dnp_refs'],item['base_quantity'],
        item['sd_option_quantity'],item['other_dnp_quantity'],
        offer=dict(url=item['url'],title=item['product_title'],code=item.get('retailer_code'),price=item.get('live_unit_price_vnd'),
            stock=item.get('live_inventory'),moq=item.get('live_moq'),multiple=item.get('live_multiple')),
        status=status,note=item.get('notes',''))

for item in connectors['items']:
    o=item['candidates'][0]
    ready=item['purchase_status']=='standard_pitch_substitute'
    sd=1 if item['refs']==['SD SOCKET'] else 0
    dnp=1 if item['refs']==['ANTEN'] else 0
    row('Đầu nối / cơ khí',item.get('native_mpn',item.get('purchase_unit','Header P2.54')),item['refs'],
        item['required_quantity'] if not item['optional_dnp'] else 0,sd,dnp,
        offer=dict(url=o['url'],title=o['title'],code=o['supplier_code'],price=o['price_vnd'],stock=o['inventory_live'],
            moq=o['minimum_order_quantity'],multiple=o['multiples']),
        status='available' if ready else 'mechanical_confirmation_required',note=o['notes'])
    if ready:
        rows[-1]['purchase_quantity']=item['purchase_quantity']
        rows[-1]['purchase_unit']=item['purchase_unit']

excluded_rows = [r for r in rows if r['refs'] and set(r['refs']).issubset(excluded_refs)]
rows = [r for r in rows if r not in excluded_rows]
expected_refs = {p['ref'] for p in pcb['parts'] if p['ref'] not in excluded_refs}
covered_refs = {ref for r in rows for ref in r['refs']}
assert expected_refs == covered_refs, 'Scoped procurement list must cover every remaining PCB reference'

source = Path(pcb['parts'][0].get('source','D:/esp32_debug/03_pcb/AI_layout/AI_layout.kicad_pcb'))
current_hash=hashlib.sha256(source.read_bytes()).hexdigest()
assert current_hash==pcb['pcb_sha256'], 'PCB changed during lookup'
report=dict(date='2026-11-15',retailer='https://www.thegioiic.com/',source_pcb_sha256=current_hash,
    source_pcb_unchanged=True,quantities='For one PCB; SD optional and other DNP are not included in base quantity',
    scope=scope, excluded_purchase_groups=[dict(part=r['required_part'],refs=r['refs']) for r in excluded_rows],
    covered_pcb_references=len(covered_refs),
    price_scope='Live public retailer data on 2026-11-15; prices are per unit before shipping. MOQ/order multiple may exceed PCB quantities.',
    group_counts=dict(Counter(r['status'] for r in rows)),items=rows)
(root/'shopping_list.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')

money=lambda n: '—' if n is None else f'{int(n):,}'.replace(',','.')+' đ'
number=lambda n: '—' if n is None else f'{int(n):,}'.replace(',','.')
status_labels={'available':'Có hàng; khớp tiêu chí đã đối chiếu','out_of_stock':'Đúng loại, hết hàng',
    'not_found_exact':'Chưa tìm đúng mã/thông số','mechanical_confirmation_required':'Chỉ là ứng viên; chưa chốt lắp vừa'}
parts=['# Linh kiện probe trên ThegioiIC — 15/11/2026','',
    'Đối chiếu từ PCB `AI_layout.kicad_pcb` hiện tại. Không thay đổi PCB, không thêm giỏ hàng hoặc đặt mua.',
    '**Phạm vi đã cập nhật theo yêu cầu:** không tìm/mua SSD1306, rotary encoder hoặc các header. Các điện trở, tụ và linh kiện bảo vệ liên quan vẫn được giữ trong danh sách.',
    'Số lượng cho **một PCB**. Cột **SD thêm** là phần tùy chọn thẻ SD đang DNP; **DNP khác** giữ không lắp theo bản hiện tại. Giá là giá một con/thanh tại thời điểm tra cứu; phải xét MOQ và bội số khi mua.', '',
    '## Kết quả cần chú ý','',
    '- Tìm đúng module ESP32-S3-WROOM-1-N16R8, sáu nhóm bán dẫn và mười nhóm thụ động có hàng.',
    '- **C1 4,7 µF/1206 và C2 tantalum 22 µF–16 V case B/1411 tìm đúng sản phẩm nhưng tồn kho bằng 0.**',
    '- **BAT54S và điện trở 5,1 kΩ 1%/1206 chưa tìm được đúng loại** qua tìm kiếm trực tiếp của cửa hàng. BAT54C không thay trực tiếp cho BAT54S; điện trở 5% không được tự đánh dấu đúng BOM 1%.',
    '- USB-C, BOOT/RESET, microSD và SMA bên dưới chỉ là ứng viên cần kiểm tra cơ khí. Không khẳng định dùng được trên PCB đã đặt chỉ từ tên gọi hoặc số chân.',
    '- ANTEN đang DNP và chân trung tâm chưa nối RF, nên không cần mua cho chức năng antenna ngoài của phiên bản này.', '',
    '## Module và IC / diode / transistor','']

def table(items):
    out=['| Cần dùng / vị trí | Cơ bản | SD thêm | DNP khác | Link sản phẩm / mã cửa hàng | Giá/đơn vị | Tồn | MOQ / bội số | Kết quả |',
         '|---|---:|---:|---:|---|---:|---:|---|---|']
    for r in items:
        title=(r['title'] or 'Chưa tìm đúng').replace('|','/')
        link=f'[{title}]({r["url"]})' if r['url'] else title
        if r['supplier_code']: link+=' · '+r['supplier_code']
        need=(r['required_part']+' — '+', '.join(r['refs'])).replace('|','/')
        out.append(f'| {need} | {r["base_quantity"]} | {r["sd_option_additional_quantity"]} | {r["other_dnp_quantity"]} | {link} | {money(r["unit_price_vnd"])} | {number(r["inventory"])} | {number(r["minimum_order_quantity"])} / {number(r["order_multiple"])} | {status_labels[r["status"]]} |')
    return out

parts+=table([r for r in rows if r['category'] in ('Module','Bán dẫn')])
parts+=['','SS34 có lựa chọn [TWGMC SS34 SMA](https://www.thegioiic.com/twgmc-ss34-sma-diode-schottky-3a) giá 1.140 đ/con, MOQ1: mua đúng hai con có thể rẻ hơn tổng lô SS34 OEM MOQ10. MMBT3904 MOQ20; SMAJ5.0A MOQ10. Đây là số lượng mua, khác số lượng linh kiện cần lắp.','',
    '## Điện trở và tụ','',
    'Điện trở và tụ gốm dùng vỏ **1206 (3216 metric)**. Riêng C2 là **tantalum case B/1411, EIA-3528-21**, không phải tụ gốm 1206. Tên giá trị/vỏ đã được đối chiếu; điện môi và dung sai của một số tụ gốm chưa được cửa hàng công bố trong bảng thông số.', '']
parts+=table([r for r in rows if r['category']=='Điện trở / tụ'])
parts+=['','## USB-C, nút nhấn và socket tùy chọn','']
parts+=table([r for r in rows if r['category']=='Đầu nối / cơ khí'])
parts+=['',
    '## Các điểm tránh mua nhầm','',
    '- ESP32 phải là module rời **WROOM-1-N16R8** có antenna PCB, không phải DevKit/CAM board hoặc WROOM-1U.',
    '- SS34 phải **SMA**, không chọn SMB; SMAJ5.0**A** một chiều, không tự thay bằng **CA** hai chiều.',
    '- USBLC6-2**SC6** là SOT-23-6; bản **P6** dùng package khác.',
    '- BOOT/RESET của PCB là switch SMD **hai pad** tâm cách 7,75 mm. Nút OEM 3×6 mm có bản vẽ gần tương tự nhưng chưa được xác nhận thay trực tiếp.',
    '- MicroSD IC4602 không được xác nhận là JXTCONN TF-01A; cần khớp toàn bộ chân vỏ, detect và land pattern.',
    '- C7/C8 10 pF đang DNP và không thuộc cụm SD. Cụm SD cần socket + R33/R34/R36–R41 + C15/C16 khi lắp tùy chọn.', '',
    '## Nguồn và bằng chứng','',
    '- [BOM gốc trích từ PCB](pcb_bom.json)',
    '- [Bán dẫn và đối chiếu pinout](semiconductors.md) · [dữ liệu giá/tồn](semiconductors.json)',
    '- [Điện trở/tụ](passives.md) · [dữ liệu trực tiếp](passives.json)',
    '- [Đầu nối và bản vẽ cơ khí](connectors.md) · [dữ liệu trực tiếp](connectors.json)',
    '- [Dữ liệu module đã đọc trực tiếp trên trang; OLED thuộc phần đã loại khỏi danh sách mua](modules.json)',
    '- [Danh sách chuẩn hóa 25 nhóm trong phạm vi hiện tại](shopping_list.json)',
    '- [Phạm vi loại trừ theo yêu cầu](scope.json)', '',
    'Giá/tồn kho là ảnh chụp dữ liệu ngày 15/11/2026, có thể thay đổi khi bạn đặt mua. Chưa tính phí giao hàng. Không có tổng giá hoàn thiện board vì một số linh kiện thiếu/hết hàng hoặc chưa chốt cơ khí.']
(root/'THEGIOIIC_BOM.md').write_text('\n'.join(parts)+'\n',encoding='utf-8')
print(json.dumps(dict(groups=len(rows),statuses=report['group_counts'],pcb_unchanged=True,
    report=str(root/'THEGIOIIC_BOM.md')),ensure_ascii=False,indent=2))
