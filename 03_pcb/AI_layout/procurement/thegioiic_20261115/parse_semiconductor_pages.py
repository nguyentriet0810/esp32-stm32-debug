import json
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
base = Path(__file__).parent
sources = base / "sources"

def walk(obj):
    if isinstance(obj, dict):
        if {"id", "name", "slug", "price"}.issubset(obj):
            yield obj
        for value in obj.values():
            yield from walk(value)
    elif isinstance(obj, list):
        for value in obj:
            yield from walk(value)

for path in sources.glob("*.html"):
    html = path.read_text(encoding="utf-8-sig")
    stream = ""
    for chunk in re.findall(r"self\.__next_f\.push\((.*?)\)</script>", html, re.S):
        try:
            value = json.loads(chunk)
            if len(value) > 1 and isinstance(value[1], str):
                stream += value[1]
        except json.JSONDecodeError:
            pass
    path.with_suffix(".rsc.txt").write_text(stream, encoding="utf-8")
    products = {}
    for line in stream.splitlines():
        try:
            value = json.loads(line.split(":", 1)[1])
            for obj in walk(value):
                products[obj["id"]] = obj
        except (json.JSONDecodeError, IndexError):
            pass
    path.with_suffix(".products.json").write_text(json.dumps(list(products.values()), ensure_ascii=False, indent=2), encoding="utf-8")
    print(path.stem, len(products))
    for obj in products.values():
        if re.search(r"AMS1117-3\.3|3904|BAT54|SS34|SMAJ5|USBLC6|AO3401", obj["name"], re.I):
            print(json.dumps({k: obj.get(k) for k in ["id", "name", "slug", "manufacturer_code", "price", "inventory", "minimum_order_quantity", "multiples", "sheetfile"]}, ensure_ascii=False))
