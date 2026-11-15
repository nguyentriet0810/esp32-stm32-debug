import json,re,sys
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).parent/'sources'
def walk(obj):
    if isinstance(obj,dict):
        yield obj
        for value in obj.values():yield from walk(value)
    elif isinstance(obj,list):
        for value in obj:yield from walk(value)
for path in root.glob('passive_*.html'):
    chunks=[]
    for chunk in re.findall(r'self\.__next_f\.push\((.*?)\)</script>',path.read_text(encoding='utf-8-sig'),re.S):
        try:
            data=json.loads(chunk)
            if len(data)>1 and isinstance(data[1],str):chunks.append(data[1])
        except json.JSONDecodeError:pass
    stream=''.join(chunks);products={};detail=None
    for line in stream.splitlines():
        try:obj=json.loads(line.split(':',1)[1])
        except (json.JSONDecodeError,IndexError):continue
        for value in walk(obj):
            if {'id','name','slug','price'}.issubset(value):products[value['id']]=value
            if 'product_properties' in value:detail=value
    path.with_suffix('.rsc.txt').write_text(stream,encoding='utf-8')
    path.with_suffix('.products.json').write_text(json.dumps(list(products.values()),indent=2,ensure_ascii=False),encoding='utf-8')
    if detail:path.with_suffix('.detail.json').write_text(json.dumps(detail,indent=2,ensure_ascii=False),encoding='utf-8')
    print(path.stem,len(products),[(x['name'],x['inventory'],x['price']) for x in products.values()] if len(products)<10 else '')
