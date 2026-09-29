# Правка скрипта страницы Settings, который лежит в src/web/settings.h сжатым gzip массивом.
# Запускать из папки libs/Settings после обновления библиотеки: python web_patch.py
#
# Spinner: число между кнопками "-" и "+" открывает окно ввода, как у Number. Штатно спиннер
# удаляет поле ввода, созданное InputWidget, и ставит вместо него надпись без обработчика.
#
# Ключ входа: цвет и окно ввода пароля обновляются при каждой сборке страницы. Штатно только при
# первой, и после установки пароля ключ не работал до перезагрузки страницы в браузере.
#
# Скрипт и стили отдаются с долгим кэшированием, а index.html ссылается на них с меткой сборки
# (script.js?метка). Метка пересчитывается от содержимого, иначе браузер оставит старый скрипт.

import gzip
import hashlib
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


def array_re(name):
    return re.compile(r'(const uint8_t ' + name + r'\[\] PROGMEM = \{\n)(.*?)(\n\};)', re.S)


def read(name):
    m = array_re(name).search(src)
    return gzip.decompress(bytes(int(x, 16) for x in re.findall(r'0x[0-9a-f]{2}', m.group(2)))).decode('utf-8')


def write(name, text, size_label):
    global src
    m = array_re(name).search(src)
    data = gzip.compress(text.encode('utf-8'), compresslevel=9, mtime=0)
    rows = ['    ' + ''.join('0x%02x, ' % b for b in data[i:i + 24]) for i in range(0, len(data), 24)]
    rows[-1] = rows[-1].rstrip(', ')
    src = src[:m.start(2)] + '\n'.join(rows) + src[m.end(2):]
    src = re.sub(size_label + r': \d+ bytes', '%s: %d bytes' % (size_label, len(data)), src)


AUTH_OLD = 'this.authF||(this.authF=!0,"granted"in s?'
AUTH_NEW = '(this.authF=!0,"granted"in s?'

PATCHES = [('Spinner', SPINNER_OLD, SPINNER_NEW), ('ключ входа', AUTH_OLD, AUTH_NEW)]

js = read('settings_script_gz')
changed = False
for name, old, new in PATCHES:
    if old in js:
        assert js.count(old) == 1, name
        js = js.replace(old, new)
        changed = True
        print(name, '- исправлено')
    elif new in js:
        print(name, '- уже исправлено')
    else:
        raise SystemExit(name + ': код в новой версии библиотеки другой - правку нужно переписать')
if changed:
    write('settings_script_gz', js, 'script')

stamp = hashlib.sha1((js + read('settings_style_gz')).encode('utf-8')).hexdigest()[:20]
index = read('settings_index_gz')
new_index = re.sub(r'(script\.js|style\.css)\?[0-9a-f]+', r'\1?' + stamp, index)
if new_index != index:
    write('settings_index_gz', new_index, 'index')
    print('метка сборки:', stamp)

open(PATH, 'w', encoding='utf-8', newline='').write(src)
