# Правка скрипта страницы Settings, который лежит в src/web/settings.h сжатым gzip массивом.
# Запускать из папки libs/Settings после обновления библиотеки: python web_patch.py
#
# Spinner: число между кнопками "-" и "+" открывает окно ввода, как у Number. Штатно спиннер
# удаляет поле ввода, созданное InputWidget, и ставит вместо него надпись без обработчика.

import gzip
import re

PATH = 'src/web/settings.h'

SPINNER_OLD = ('constructor(t){super(t);let e=this.$out.parentNode;this.$out.remove(),'
               'e.appendChild(EL.make("div",{children:[{tag:"span",class:"spin_btn",text:"-",click:()=>this.change(-1)},'
               '{tag:"span",class:"value active",style:"margin: 0 5px",$:"out"},'
               '{tag:"span",class:"spin_btn",text:"+",click:()=>this.change(1)}]})),this.update(t.value)}')
SPINNER_NEW = ('constructor(t){super(t);let e=this.$out.parentNode,o=this.$out;o.remove(),o.style.margin="0 5px";'
               'let d=EL.make("div",{children:[{tag:"span",class:"spin_btn",text:"-",click:()=>this.change(-1)},'
               '{tag:"span",class:"spin_btn",text:"+",click:()=>this.change(1)}]});'
               'd.insertBefore(o,d.lastChild),e.appendChild(d),this.update(t.value)}')

src = open(PATH, encoding='utf-8', newline='').read()
m = re.search(r'(const uint8_t settings_script_gz\[\] PROGMEM = \{\n)(.*?)(\n\};)', src, re.S)
js = gzip.decompress(bytes(int(x, 16) for x in re.findall(r'0x[0-9a-f]{2}', m.group(2)))).decode('utf-8')

if SPINNER_NEW in js:
    print('уже исправлено')
    raise SystemExit
assert js.count(SPINNER_OLD) == 1, 'код Spinner в новой версии библиотеки другой - правку нужно переписать'
js = js.replace(SPINNER_OLD, SPINNER_NEW)

data = gzip.compress(js.encode('utf-8'), compresslevel=9, mtime=0)
rows = ['    ' + ''.join('0x%02x, ' % b for b in data[i:i + 24]) for i in range(0, len(data), 24)]
rows[-1] = rows[-1].rstrip(', ')
src = src[:m.start(2)] + '\n'.join(rows) + src[m.end(2):]
src = re.sub(r'script: \d+ bytes', 'script: %d bytes' % len(data), src)
open(PATH, 'w', encoding='utf-8', newline='').write(src)
print('готово, скрипт %d байт' % len(data))
