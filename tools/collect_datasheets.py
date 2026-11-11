from pathlib import Path
from urllib.request import urlopen, Request
from urllib.parse import urljoin, urlsplit, unquote
from concurrent.futures import ThreadPoolExecutor
import hashlib, json, shutil, zipfile
from bs4 import BeautifulSoup
import fitz

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'datasheet'
DEST.mkdir(exist_ok=True)
PAGE = 'https://www.waveshare.net/wiki/RP2350B-Plus-W'
jobs = [
 ('01_chip/RP2350_Datasheet.pdf', 'https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008373-DS-3-rp2350-datasheet.pdf'),
 ('01_chip/Hardware_Design_with_RP2350.pdf', 'https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008280-DS-2-hardware-design-with-rp2350.pdf'),
 ('01_chip/RP2350B_Minimal_KiCad.zip', 'https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-010329-CA-1-RP2350B%20Minimal%20KiCAD.zip'),
 ('02_board/RP2350B-Plus-W_Schematic.pdf', 'https://files.waveshare.com/wiki/RP2350B-Plus-W/RP2350B-Plus-W.pdf'),
 ('03_examples/RP2350B-Plus-W_Official_Demo.zip','https://www.waveshare.net/w/upload/3/3e/RP2350B-Plus-W.zip'),
 ('03_examples/Waveshare_RP2350B_Arduino_Board.zip','https://www.waveshare.net/w/upload/8/8b/WAVESHARE-RP2350B-Board.zip'),
 ('04_reference/Pico2W_Datasheet.pdf','https://pip-assets.raspberrypi.com/categories/1088-raspberry-pi-pico-2-w/documents/RP-008304-DS-2-pico-2-w-datasheet.pdf'),
 ('04_reference/Getting_Started_with_Pico2.pdf','https://www.waveshare.net/w/upload/7/7f/Getting-started-with-pico-2.pdf'),
 ('04_reference/Pico_C_SDK.pdf','https://www.waveshare.net/w/upload/1/1f/Raspberry-pi-pico-2-c-sdk.pdf'),
 ('04_reference/Pico_Python_SDK.pdf','https://www.waveshare.net/w/upload/7/7c/Raspberry-pi-pico-2-python-sdk.pdf'),
]
soup = BeautifulSoup((ROOT/'tools/page-0.txt').read_bytes(), 'html.parser')
content = soup.select_one('.mw-parser-output') or soup
for i,img in enumerate(content.find_all('img')):
    u=urljoin(PAGE,img.get('src',''))
    if not img.get('src'): continue
    ext=Path(unquote(urlsplit(u).path)).suffix.lower()
    if ext not in ('.png','.jpg','.jpeg','.gif','.svg'): continue
    rel=f'02_board/wiki_assets/{i:02d}{ext}'
    jobs.append((rel,u))
    img['src']='wiki_assets/'+Path(rel).name
    for key in ['srcset','loading']: img.attrs.pop(key,None)
for tag in content.find_all(['a','link']):
    if tag.get('href'): tag['href']=urljoin(PAGE,tag['href'])
(DEST/'02_board').mkdir(exist_ok=True)
(DEST/'02_board/Waveshare_Wiki_zh.html').write_text('<!doctype html><html><head><meta charset="utf-8"><title>RP2350B-Plus-W 官方资料</title><style>body{max-width:1100px;margin:30px auto;font-family:Arial,"Microsoft YaHei",sans-serif;line-height:1.7}img{max-width:100%;height:auto}table{border-collapse:collapse}td,th{border:1px solid #aaa;padding:7px}</style></head><body><p>来源：<a href="'+PAGE+'">Waveshare 官方中文 Wiki</a>；保存日期：2026-11-01（America/Denver）。附件链接可能需要联网。</p>'+str(content)+'</body></html>',encoding='utf-8')
(DEST/'02_board/Waveshare_Wiki_zh.txt').write_text(content.get_text('\n',strip=True),encoding='utf-8')

def fetch(job):
    rel,u=job; p=DEST/rel; p.parent.mkdir(parents=True,exist_ok=True)
    try:
        with urlopen(Request(u,headers={'User-Agent':'Mozilla/5.0'}),timeout=60) as response: data=response.read()
        if p.suffix=='.pdf' and not data.startswith(b'%PDF'): raise ValueError('Not a PDF')
        if p.suffix=='.zip' and not data.startswith(b'PK'): raise ValueError('Not a ZIP')
        p.write_bytes(data)
        meta={'file':rel,'source':u,'download_date':'2026-11-01','timezone':'America/Denver','bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'status':'ok'}
        if p.suffix=='.pdf':
            with fitz.open(p) as doc: meta.update(pages=len(doc),title=doc.metadata.get('title',''))
        if p.suffix=='.zip':
            with zipfile.ZipFile(p) as z:
                bad=z.testzip()
                if bad: raise ValueError('ZIP CRC failure: '+bad)
                meta['entries']=len(z.namelist())
                p.with_suffix('.contents.txt').write_text('\n'.join(z.namelist()),encoding='utf-8')
        print('OK',rel,len(data),flush=True)
        return meta
    except Exception as e:
        print('FAILED',rel,str(e),flush=True)
        return {'file':rel,'source':u,'status':'failed','error':str(e)}

with ThreadPoolExecutor(max_workers=6) as pool: records=list(pool.map(fetch,jobs))
(DEST/'sources.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
(DEST/'05_blink_and_backup').mkdir(exist_ok=True)
for source,name in [(ROOT/'main.py','main.py'),(ROOT/'original-firmware.uf2','original-firmware.uf2'),(ROOT/'tools/pico2w-micropython.uf2','RPI_PICO2_W_MicroPython_v1.29.0.uf2')]:
    shutil.copy2(source,DEST/'05_blink_and_backup'/name)
with fitz.open(DEST/'02_board/RP2350B-Plus-W_Schematic.pdf') as doc:
    doc[0].get_pixmap(matrix=fitz.Matrix(1.5,1.5)).save(DEST/'02_board/Schematic_preview.png')
print('DONE',len(records),'files;',sum(r['status']=='failed' for r in records),'failed',flush=True)
