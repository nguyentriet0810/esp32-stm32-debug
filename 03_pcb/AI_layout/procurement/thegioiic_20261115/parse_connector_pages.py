import json,re,sys
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
w=Path('D:/esp32_debug/03_pcb/AI_layout/procurement/thegioiic_20261115/connector_sources')
def walk(o):
 if isinstance(o,dict):
  if {'id','name','slug','price'}<=set(o):yield o
  for x in o.values():yield from walk(x)
 elif isinstance(o,list):
  for x in o:yield from walk(x)
for path in w.glob('*.html'):
 s=''
 for chunk in re.findall(r'self\.__next_f\.push\((.*?)\)</script>',path.read_text(encoding='utf-8-sig'),re.S):
  try:
   v=json.loads(chunk)
   if len(v)>1 and isinstance(v[1],str):s+=v[1]
  except (json.JSONDecodeError,TypeError):pass
 products={}
 for marker in re.finditer(r'[a-f0-9]+:(?=[\[{])',s):
  try:
   v,_=json.JSONDecoder().raw_decode(s[marker.end():])
   for p in walk(v):products[p['id']]=p
  except (json.JSONDecodeError,IndexError):pass
 path.with_suffix('.rsc.txt').write_text(s,encoding='utf-8');path.with_suffix('.products.json').write_text(json.dumps(list(products.values()),indent=2,ensure_ascii=False),encoding='utf-8')
 print(path.stem,len(products))
 for p in products.values():print(json.dumps({k:p.get(k) for k in ['id','name','slug','manufacturer_code','price','inventory','minimum_order_quantity','multiples','sheetfile']},ensure_ascii=False))

