#pragma once

// Текст custom.js для страницы настроек (SettingsUI.ino). Лежит в заголовке, а не в .ino:
// генератор прототипов Arduino принимает методы JavaScript в строке за функции C++.

// Правка вёрстки страницы. Библиотека добавляет на страницу поле css у каждого класса из custom.js.
// В исходной вёрстке подпись виджета не переносится, и на узком экране или при увеличенном масштабе
// длинная подпись выталкивает переключатель или значение за край строки. Здесь подпись и значение
// переносятся по словам, а правая часть строки не сжимается и занимает не больше 60% ширины.
// Вкладки делят ширину поровну, а не прижимаются к краям, и скругляются как кнопки (штатно 15px против 10px).
//
// LampUpdate - проверка новых релизов на GitHub. Запрос делает браузер, а не лампа: HTTPS до GitHub
// требует 20-30 КБ кучи, у лампы столько свободно не бывает. Проверка - не чаще раза в сутки на браузер,
// найденная версия запоминается в браузере и показывается строкой над вкладками, пока лампу не обновят.
// Библиотека подставляет свои имена (WidgetBase, popup, EL и др.) во весь текст класса, поэтому
// в строках класса этих сочетаний нет
static const char uiCustomJs[] PROGMEM = R"js(class LampLayout {
static css = `
.widget_row{height:unset;min-height:32px}
.widget_row label{white-space:normal}
.widget_row>:last-child:not(:first-child){flex-shrink:0;max-width:60%}
.widget_row .value{white-space:normal;overflow-wrap:break-word;text-align:right}
.tab{flex:1 1 0;text-align:center}
.tabs>.tab{border-radius:10px}
`;
}
class LampUpdate extends WidgetBase {
constructor(data) {
  super(data, false);
  this.$root.style.display = "none";
  this.ver = data.ver;
  this.show(localStorage.getItem("lamp_upd_tag"), localStorage.getItem("lamp_upd_url"), false);
  if (!data.check || Date.now() - Number(localStorage.getItem("lamp_upd_at") || 0) < 86400000) return;
  const stop = new AbortController();
  setTimeout(() => stop.abort(), 8000);
  fetch("https://api.github.com/repos/" + data.repo + "/releases/latest", { signal: stop.signal })
    .then((r) => (r.ok ? r.json() : null))
    .then((rel) => {
      if (!rel || !rel.tag_name) return;
      localStorage.setItem("lamp_upd_at", Date.now());
      localStorage.setItem("lamp_upd_tag", rel.tag_name);
      localStorage.setItem("lamp_upd_url", rel.html_url);
      this.show(rel.tag_name, rel.html_url, true);
    })
    .catch(() => {});
}
newer(tag) {
  const a = String(tag).replace(/^v/, "").split(/[.-]/).map(Number);
  const b = String(this.ver).split(/[.-]/).map(Number);
  for (let i = 0; i < 3; i++) {
    if ((a[i] || 0) !== (b[i] || 0)) return (a[i] || 0) > (b[i] || 0);
  }
  return false;
}
show(tag, url, notify) {
  if (!tag || !this.newer(tag)) return;
  const v = tag.replace(/^v/, "");
  this.$root.style.display = "";
  this.$root.innerHTML = `<div class="widget_row"><label class="widget_label">Доступна версия ${v}</label>` +
    `<a class="value" style="color:var(--accent)" target="_blank" href="${url}">Скачать</a></div>`;
  if (notify) popup("Вышла версия " + v, false);
}
})js";
