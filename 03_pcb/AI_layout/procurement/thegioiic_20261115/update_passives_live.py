"""Fold fetched public retailer product data into the passive lookup report."""
import hashlib,json
from pathlib import Path
BASE=Path(__file__).parent
SRC=BASE/'sources'
p=BASE/'passives.json'
j=json.loads(p.read_text(encoding='utf-8'))
j['evidence_limits']='Product information, prices, quantities, MOQ and multiples were read from public live retailer HTML/Next.js data on 2026-11-15. Saved source HTML, extracted products and property tables are in sources/passive_*. Inventory and price can change after this snapshot; no order/cart action was performed. Ceramic dielectric is not published in these product property tables.'
j['source_pcb_sha256']=hashlib.sha256(Path(j['pcb']).read_bytes()).hexdigest()
j['summary'].update(matched_groups=12,not_found_exact_groups=1,matched_but_out_of_stock_groups=2)
for i,x in enumerate(j['items']):
    if i==5:
        x['evidence']=['Live retailer own search q=5.1 1206 1% returns zero products (structured numberOfItems=0).',
                       'Live own search q=5.1 1206 returns one 1206 product, but it is 5% and out of stock.']
        x['supporting_urls']=['https://www.thegioiic.com/tim-kiem?q=5.1%201206%201%25','https://www.thegioiic.com/tim-kiem?q=5.1%201206']
        x['rejected_candidates']=[dict(title='Điện Trở 5.1 KOhm 1206 5%',
            url='https://www.thegioiic.com/dien-tro-5-1-kohm-1206-5-',reason='Wrong specified tolerance:5% versus required1%; not approved as substitution.',
            live_inventory=0,live_unit_price_vnd=87,live_moq=25,live_multiple=25)]
        x['notes']='R1/R2 explicitly specify1% in PCB. Do not automatically substitute5% or another package. This is an exact-spec search gap, not proof no vendor product can ever be obtained.'
        x['source_files']=['sources/passive_r51_1percent.html','sources/passive_r51_1percent.products.json','sources/passive_r51_5percent.detail.json']
        continue
    product=json.loads((SRC/f'passive_product_{i}.products.json').read_text(encoding='utf-8'))[0]
    detail=json.loads((SRC/f'passive_product_{i}.detail.json').read_text(encoding='utf-8'))
    props={r['name']:r['value'] for r in detail['product_properties']}
    x.update(status='matched_value_package',product_title=product['name'],
        url='https://www.thegioiic.com/'+product['slug'],retailer_code='IC'+str(product['id']),
        retailer_brand=product.get('supplier',{}).get('name'),live_stock_verified=True,
        live_unit_price_vnd=product['price'],live_inventory=product['inventory'],
        live_stock_status='in_stock' if product['inventory']>0 else 'out_of_stock',
        live_moq=product.get('minimum_order_quantity'),live_multiple=product.get('multiples'),
        live_price_tiers=product.get('prices'),retailer_product_properties=props,
        evidence=['Live public retailer product HTML contains matching product identity, price, inventory and property table.'],
        source_files=[f'sources/passive_product_{i}.html',f'sources/passive_product_{i}.products.json',f'sources/passive_product_{i}.detail.json'])
    x['verified_specs']=x.get('verified_specs') or {}
    x['verified_specs']['package']=props.get('Kiểu đóng gói')
    if x['kind']=='resistor':x['verified_specs'].update(power_w=.25,technology='Thick Film')
    else:x['verified_specs'].update(tolerance_percent=10)
    if i==7:
        x['verified_specs'].update(capacitance_uf=4.7,voltage_v=50,body_mm=[3.2,1.6,1.27],dielectric=None)
        x['notes']='Exact4.7uF1206MLCC product found by retailer own search; currently out of stock. Keep ceramic type; do not replace with same-number tantalum1206 whose body/footprint convention differs.'
    if i==8:
        x['verified_specs'].update(capacitance_uf=22,voltage_v=16,body_mm=[3.5,2.8,1.9],chemistry='tantalum')
        x['notes']='Kemet B /1411(3528M) matches3.5x2.8 footprint body size and fits2.1mmheight envelope. Polarized tantalum. Exact product and seven additional22uF16V1411 choices all have liveinventory0; do not substitute2917/2312 or ceramic automatically.'
        x['supporting_urls'].append('https://www.thegioiic.com/tim-kiem?q=22uF%2016V%201411')
    x.pop('indexed_observation',None)
j['site_search_gaps']=[dict(refs=['R1','R2'],required='5.1kOhm1%1206',result='Exact query0products; only5%1206product shown by broader search.')]
j['site_search_matching_out_of_stock']=[dict(refs=['C1'],product='Tụ Gốm1206 4.7uF50V',inventory=0),dict(refs=['C2'],product='T491B226K016AT1411/3528',inventory=0)]
p.write_text(json.dumps(j,indent=2,ensure_ascii=False),encoding='utf-8')

lines=['# Linh kiện thụ động AI_layout — Thegioiic,15/11/2026','',
'Đọc BOM từ PCB hiện tại và đối chiếu dữ liệu công khai trực tiếp trên website ngày15/11/2026. Tất cả số lượng là cho một bo; phần SD hiện DNP, C7/C8 cũng DNP và không thuộc SD. Không sửa PCB, thêm giỏ hoặc đặt hàng.','',
'Đã tìm đúng12/13nhóm. C1 vàC2 có đúng sản phẩm nhưng đang hết hàng. R1/R2 chưa tìm thấy đúng5,1kΩ1%1206; sản phẩm5%không được tự động dùng thay.','',
'| Giá trị /vỏ | Reference cơ bản | SL cơ bản | SD DNP | DNP khác | Giáđ/con | MOQ/bội số | Tồn hiện tại | Sản phẩm |',
'|---|---|---:|---:|---:|---:|---|---:|---|']
for x in j['items']:
    lines.append('| '+' | '.join([x['required_value']+' /'+x['required_package'],', '.join(x['base_refs'])or'—',str(x['base_quantity']),str(x['sd_option_quantity']),str(x['other_dnp_quantity']),
        str(x.get('live_unit_price_vnd') or '—'),f"{x['live_moq']}/{x['live_multiple']}" if x.get('live_moq') else '—',str(x['live_inventory']) if 'live_inventory'in x else 'Chưa tìm đúng',
        f"[{x['product_title']}]({x['url']})"if x['url']else'Chưa tìm đúng'])+' |')
lines += ['',
'Các điện trở tìm được đều có bảng thông số1206(3216M),0,25W. Các tụ gốm được xác minh1206(3216M),±10%; điện áp từng mã trong tên sản phẩm. Nhà bán không công bố loại điện môi trong bảng này, nên không khẳng định X7R/X5R. Giá/tồn kho là ảnh chụp dữ liệu ngày tra cứu và có thể thay đổi; MOQ có thể lớn hơn số linh kiện cần lắp.','',
'Những chỗ chưa thể mua đủ ngay:','',
'- **R1/R2:** [tìm5.1 12061%](https://www.thegioiic.com/tim-kiem?q=5.1%201206%201%25) trả0sản phẩm. [Mã12065%](https://www.thegioiic.com/dien-tro-5-1-kohm-1206-5-) sai dung sai yêu cầu và cũng hết hàng.','- **C1:** [4,7µF50V1206](https://www.thegioiic.com/tu-gom-1206-4-7uf-50v) giá2.772đ, tồn0. Không nhầm4,7nF hoặc vỏ0805.','- **C2:** [T491B226K016AT22µF16V1411](https://www.thegioiic.com/tu-tantalum-22uf-16v-1411-t491b226k016at) giá17.100đ,tồn0. Thân3,5×2,8×1,9mm phù hợp mặt bằngEIA3528vàgiới hạn chiều cao2,1mm. Cả8kết quả22µF16V1411đều tồn0.','',
'C2phải giữ tantalum có cực đúng chiều. [T491D226K016AT2917](https://www.thegioiic.com/tu-tantalum-22uf-16v-2917-t491d226k016at) là vỏD7,3×4,3mm, không thay trực tiếp cho caseB. Không tự động thay bằng tụ gốm22µF1206/1210.','',
'R33,R34,R36–R41,C15/C16 là10linh kiện tùy chọnSD hiệnDNP. C7/C8(10pF)giữDNPtrênUSB, không thuộc số lượng cơ bản.','',
'Chi tiết giá theo bậc,sai số,công suất,MOQ,bội số,thuộc tính kỹ thuật,và đường dẫn nguồn HTML/JSONđược lưu trongpassives.json; các trang nguồn trongsources/passive_*.']
(BASE/'passives.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('updated12matched,2outofstock,1exactspecgap')
