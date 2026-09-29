#pragma once

/*
 * Storage.h — хранилище настроек лампы в файле на LittleFS (библиотека GyverDB).
 *
 * Устройство:
 *  - все настройки лежат в базе GyverDBFile в файле /lamp.db;
 *  - скалярные настройки (режим работы, текущий эффект, будильник и т.д.) — отдельные ключи;
 *  - массивы (настройки всех эффектов, будильники, избранное) — бинарные блобы целиком;
 *  - ключи kk::* используются и здесь, и виджетами веб-интерфейса Settings (SettingsUI.ino),
 *    поэтому виджеты сами читают/пишут значения из этой же базы;
 *  - запись на флеш отложенная: изменения копятся в RAM, файл переписывается
 *    через 10 секунд после последнего изменения (тикер GyverDBFile в Storage::Tick()).
 */

#include <LittleFS.h>
#include <GyverDBFile.h>
#include "Constants.h"
#include "Types.h"

GyverDBFile db(&LittleFS, "/lamp.db");                      // база данных настроек (файл на LittleFS)

DB_KEYS(kk,
    // WiFi
    wifi_ssid,                                              // имя WiFi сети роутера
    wifi_pass,                                              // пароль WiFi сети роутера
    wifi_connect,                                           // id кнопки "Подключить" в веб-интерфейсе (в БД не хранится)
    wifi_last_ssid,                                         // SSID последней успешно подключённой сети (для показа IP бегущей строкой при смене сети)
    ap_name,                                                // имя собственной точки доступа лампы (пусто - AP_NAME из Config.h)
    ap_pass,                                                // пароль собственной точки доступа (пусто - открытая сеть)
    host_name,                                              // имя лампы в локальной сети (пусто - HOST_NAME из Config.h)
    ui_pass,                                                // пароль вкладки «Настройки» и обновления прошивки со страницы; пусто - без пароля
    esp_mode,                                               // режим работы лампы: 0 - точка доступа, 1 - клиент WiFi (подключение к роутеру)

    // Лампа
    lamp_on,                                                // состояние лампы (вкл/выкл)
    current_mode,                                           // номер текущего эффекта
    button_enabled,                                         // признак "кнопка разблокирована"
    dawn_mode,                                              // время до "рассвета" (номер опции в списке)
    rnd_cycle_on,                                           // вкл/выкл случайных настроек эффектов в режиме Цикл (ключи в DB_KEYS - глобальные имена, поэтому имя не совпадает с переменной random_on)
    btn_sleep_time,                                         // время таймера сна, устанавливаемого двойным кликом кнопки, минуты (имя не совпадает с переменной button_sleep_time)
    running_text,                                           // текст эффекта Бегущая строка
    run_text_ip,                                            // вкл/выкл "Писать текущий IP" в эффекте Бегущая строка

    // Оборудование (Настройки > Оборудование)
    hw_matrix_conn,                                         // угол подключения и направление ленты: 0-7, см. hwMatrixConnections
    hw_matrix_parallel,                                     // разводка ленты: false - зигзаг, true - параллельная
    hw_color_order,                                         // порядок цветов ленты: 0-5, см. hwColorOrders
    hw_current_limit,                                       // лимит тока ленты, мА; 0 - без лимита
    hw_button,                                              // кнопка: 0 - нет, 1 - сенсорная, 2 - механическая
    hw_power_restore,                                       // после подачи питания включаться, если лампа была включена

    // Кнопка (Настройки > Кнопка): действие каждого жеста, ButtonAction из Types.h
    btn_fav_only,                                           // следующий и предыдущий эффект - только среди отмеченных для Цикла
    btn_on1, btn_on2, btn_on3, btn_on4, btn_on5, btn_on6, btn_on7,          // 1-7 кликов на включённой лампе
    btn_off1, btn_off2, btn_off3, btn_off4, btn_off5, btn_off6, btn_off7,   // 1-7 кликов на выключенной лампе
    btn_hold0, btn_hold1, btn_hold2, btn_hold3, btn_hold4, btn_hold5, btn_hold6, btn_hold7, // удержание после 0-7 кликов

    // Обратный отсчёт
    cd_seconds,                                             // интервал, секунды (1-5999, до 99:59)
    cd_bri,                                                 // яркость цифр
    cd_hue,                                                 // до 4.0: оттенок цифр 0-255, переносится в cd_color
    cd_color,                                               // цвет цифр, 0xRRGGBB; вспышки - противоположного оттенка
    cd_rot,                                                 // положение цифр по окружности лампы, колонка 0-15
    cd_mirror,                                              // последние 9 секунд цифра повторяется на противоположной стороне лампы

    // Кубики
    dice_bri,                                               // яркость
    dice_speed,                                             // скорость анимации броска: от 4 с на 1 до 0.9 с на 255
    dice_hue,                                               // до 4.0: оттенок 0-255, переносится в dice_color
    dice_color,                                             // цвет, 0xRRGGBB
    dice_rot,                                               // положение результата по окружности лампы, колонка 0-15
    dice_hold,                                              // сколько секунд держится результат, 0 - пока не вернуться к эффекту
    dice_click,                                             // клик кнопкой лампы бросает кубик ещё раз, пока он на лампе
    dice_mirror,                                            // копия результата и анимации на противоположной стороне лампы
    dice_last,                                              // последний брошенный кубик (индекс в diceSides)

    // Автояркость
    ab_on,                                                  // вкл/выкл автояркости по датчику освещённости
    ab_min_bri,                                             // минимальная яркость в темноте, % (5-100)
    ab_dark,                                                // калибровка: значение A0 в темноте
    ab_light,                                               // калибровка: значение A0 при свете (если меньше ab_dark - шкала автоматически инвертируется)

    // Избранное (режим Цикл)
    ntp_host,                                               // адрес NTP сервера (сервера точного времени)
    tz_offset,                                              // часовой пояс: смещение от UTC в минутах
    tz_dst,                                                 // переход на летнее время: 0 - нет, 1 - европейские правила, 2 - США и Канада
    wol_mac,                                                // MAC-адрес компьютера для Wake-on-LAN
    wol_ext_on,                                             // вкл/выкл слежения за дополнительным WOL-топиком
    wol_ext_topic,                                          // дополнительный WOL-топик (произвольный, вне дерева топиков лампы)

    fav_running,                                            // вкл/выкл режима избранных эффектов
    fav_interval,                                           // интервал смены эффектов (секунды)
    fav_dispersion,                                         // случайный разброс интервала (секунды)
    fav_use_saved,                                          // использовать ли сохранённое состояние вкл/выкл после перезагрузки

    // Массивы (бинарные блобы)
    modes_blob,                                             // настройки всех эффектов: MODE_AMOUNT x {яркость, скорость, масштаб}
    alarms_blob,                                            // будильники: 7 x {вкл/выкл, время в минутах от начала суток}
    fav_modes_blob,                                         // флаги "эффект добавлен в избранное": MODE_AMOUNT x {0/1}

    // MQTT (редактируется через веб-интерфейс; используется с этапа MQTT)
    mqtt_enabled,                                           // вкл/выкл MQTT клиента
    mqtt_host,                                              // адрес MQTT брокера
    mqtt_port,                                              // порт MQTT брокера
    mqtt_user,                                              // пользователь MQTT брокера
    mqtt_pass                                               // пароль пользователя MQTT брокера
);

// ключи действий жестов кнопки: клики на включённой и выключенной лампе, удержание после 0-7 кликов
static const size_t buttonClickKeys[2][7] = {
  {kk::btn_on1, kk::btn_on2, kk::btn_on3, kk::btn_on4, kk::btn_on5, kk::btn_on6, kk::btn_on7},
  {kk::btn_off1, kk::btn_off2, kk::btn_off3, kk::btn_off4, kk::btn_off5, kk::btn_off6, kk::btn_off7}
};
static const size_t buttonHoldKeys[8] = {
  kk::btn_hold0, kk::btn_hold1, kk::btn_hold2, kk::btn_hold3, kk::btn_hold4, kk::btn_hold5, kk::btn_hold6, kk::btn_hold7
};

#define AP_PASS_MIN_LENGTH    (8U)                          // WiFi не принимает пароль точки доступа короче восьми символов: с более коротким паролем softAP не стартует и лампа остаётся без сети
#define HOST_NAME_MAX_LENGTH  (32U)                         // WiFi.hostname() не принимает имя длиннее 32 символов

// Имя и пароль точки доступа лампы: значения из веб-интерфейса, а если имя не задано или
// пароль оказался короче восьми символов - значения из Config.h. Пустой пароль означает
// сеть без пароля, это допустимо.
inline String apName()
{
  String name = (String)db[kk::ap_name];
  return name.length() ? name : String(AP_NAME);
}

inline String apPass()
{
  String pass = (String)db[kk::ap_pass];
  return (pass.length() == 0U || pass.length() >= AP_PASS_MIN_LENGTH) ? pass : String(AP_PASS);
}

// Имя лампы в локальной сети: лампа передаёт его роутеру в DHCP-запросе и отвечает по mDNS
// на <имя>.local. В имени хоста допустимы только латинские буквы, цифры и дефис, причём дефис
// не может быть первым или последним символом - остального WiFi.hostname() не принимает, и
// лампа осталась бы под именем ESP_XXXXXX. Поэтому введённое значение приводится к этому
// набору, а пустой результат заменяется значением из Config.h.
inline String hostName()
{
  String name = (String)db[kk::host_name];
  String result;

  for (uint16_t i = 0U; i < name.length() && result.length() < HOST_NAME_MAX_LENGTH; i++)
  {
    char c = name[i];
    if (c >= 'A' && c <= 'Z')
    {
      c += 'a' - 'A';
    }
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '-' && result.length()))
    {
      result += c;
    }
  }

  while (result.length() && result[result.length() - 1U] == '-')
  {
    result.remove(result.length() - 1U);
  }

  return result.length() ? result : String(HOST_NAME);
}

// оттенок в цвет 0xRRGGBB для виджета Color
inline uint32_t hueToRgb(uint8_t hue)
{
  CRGB c = CHSV(hue, 255U, 255U);
  return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

#define STORAGE_WRITE_DELAY   (30000UL)                     // отсрочка записи настроек эффектов после последнего изменения (чтобы не изнашивать флеш при регулировке ползунками)

class Storage
{
  public:
    static void InitSettings(ModeType modes[], AlarmType alarms[], uint8_t* espMode, bool* onFlag, uint8_t* dawnMode, uint8_t* currentMode, bool* buttonEnabled
      #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      , uint8_t* random_on
      #endif //ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      , uint8_t* button_sleep_time
      , void (*readFavoritesSettings)(), void (*saveFavoritesSettings)(), void (*restoreDefaultSettings)())
    {
      LittleFS.begin();
      db.begin();                                           // чтение базы настроек из файла (или создание пустого файла при первом запуске)

      bool firstRun = !db.has(kk::current_mode);

      restoreDefaultSettings();                             // заполнение modes[] настройками эффектов по умолчанию (перекроются данными из БД ниже, если они там есть)

      // создание ячеек с начальными значениями (db.init записывает значение, только если ячейки ещё нет)
      db.init(kk::wifi_ssid, "");
      db.init(kk::wifi_pass, "");
      db.init(kk::wifi_last_ssid, "");
      db.init(kk::ap_name, AP_NAME);
      db.init(kk::ap_pass, AP_PASS);
      db.init(kk::host_name, HOST_NAME);
      db.init(kk::ui_pass, "");
      db.init(kk::esp_mode, (uint8_t)ESP_MODE);
      db.init(kk::lamp_on, false);
      db.init(kk::dawn_mode, (uint8_t)0);
      db.init(kk::current_mode, (uint8_t)0);
      db.init(kk::button_enabled, true);
      #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      db.init(kk::rnd_cycle_on, (uint8_t)RANDOM_SETTINGS_IN_CYCLE_MODE);
      #endif //RANDOM_SETTINGS_IN_CYCLE_MODE
      db.init(kk::btn_sleep_time, (uint8_t)1);
      db.init(kk::running_text, RUNNING_TEXT_DEFAULT);
      db.init(kk::run_text_ip, false);
      db.init(kk::hw_matrix_conn, (uint8_t)0);
      db.init(kk::hw_matrix_parallel, false);
      db.init(kk::hw_color_order, (uint8_t)2);              // GRB - порядок WS2812B
      db.init(kk::hw_current_limit, (uint16_t)2000);
      db.init(kk::hw_button, (uint8_t)1);
      db.init(kk::hw_power_restore, false);
      db.init(kk::btn_fav_only, false);
      const uint8_t clickDefaults[2][7] = {
        {BTN_POWER, BTN_NEXT, BTN_PREV, BTN_NONE, BTN_IP, BTN_TIME, BTN_NONE},
        {BTN_POWER, BTN_SLEEP, BTN_NONE, BTN_NONE, BTN_IP, BTN_TIME, BTN_NONE}
      };
      const uint8_t holdDefaults[8] = {BTN_BRIGHTNESS, BTN_SPEED, BTN_SCALE, BTN_NONE, BTN_OTA, BTN_NONE, BTN_NONE, BTN_WIFI};
      for (uint8_t i = 0U; i < 8U; i++)
      {
        if (i < 7U)
        {
          db.init(buttonClickKeys[0][i], clickDefaults[0][i]);
          db.init(buttonClickKeys[1][i], clickDefaults[1][i]);
        }
        db.init(buttonHoldKeys[i], holdDefaults[i]);
      }
      db.init(kk::cd_seconds, (uint16_t)60);
      db.init(kk::cd_bri, (uint8_t)40);
      db.init(kk::cd_hue, (uint8_t)0);
      db.init(kk::cd_rot, (uint8_t)0);
      db.init(kk::cd_mirror, false);
      db.init(kk::dice_bri, (uint8_t)40);
      db.init(kk::dice_speed, (uint8_t)128);
      db.init(kk::dice_hue, (uint8_t)0);
      db.init(kk::cd_color, hueToRgb((uint8_t)db[kk::cd_hue])); // при обновлении с 3.x цвет берётся из прежнего оттенка
      db.init(kk::dice_color, hueToRgb((uint8_t)db[kk::dice_hue]));
      db.init(kk::dice_rot, (uint8_t)0);
      db.init(kk::dice_hold, (uint16_t)10);
      db.init(kk::dice_click, false);
      db.init(kk::dice_mirror, true);
      db.init(kk::dice_last, (uint8_t)6);
      #ifdef USE_AUTO_BRIGHTNESS
      db.init(kk::ab_on, false);
      db.init(kk::ab_min_bri, (uint8_t)20);
      db.init(kk::ab_dark, (uint16_t)0);
      db.init(kk::ab_light, (uint16_t)1023);
      #endif //USE_AUTO_BRIGHTNESS
      db.init(kk::ntp_host, NTP_ADDRESS);
      db.init(kk::tz_offset, (int16_t)TIMEZONE_OFFSET_DEFAULT);
      db.init(kk::tz_dst, (uint8_t)TIMEZONE_DST_DEFAULT);
      db.init(kk::wol_mac, "");
      db.init(kk::wol_ext_on, false);
      db.init(kk::wol_ext_topic, "");
      #if USE_MQTT
      db.init(kk::mqtt_enabled, true);
      db.init(kk::mqtt_host, MQTT_DEFAULT_HOST);
      db.init(kk::mqtt_port, (uint16_t)MQTT_DEFAULT_PORT);
      db.init(kk::mqtt_user, MQTT_DEFAULT_USER);
      db.init(kk::mqtt_pass, MQTT_DEFAULT_PASS);
      #endif //USE_MQTT
      db.init(kk::modes_blob, gdb::AnyType((const void*)modes, sizeof(ModeType) * MODE_AMOUNT));
      db.init(kk::alarms_blob, gdb::AnyType((const void*)alarms, sizeof(AlarmType) * 7));

      if (firstRun)
      {
        saveFavoritesSettings();                            // первоначальная запись настроек Избранного (значения по умолчанию из статических полей FavoritesManager)
      }

      // инициализация настроек лампы значениями из БД
      gdb::Entry modesEntry = db.get(kk::modes_blob);
      if (modesEntry.size() == sizeof(ModeType) * MODE_AMOUNT)
      {
        modesEntry.writeBytes(modes);
      }
      else                                                  // количество эффектов изменилось после обновления прошивки
      {
        if (modesEntry.buffer() && modesEntry.size() < sizeof(ModeType) * MODE_AMOUNT && modesEntry.size() % sizeof(ModeType) == 0)
        {
          memcpy(modes, modesEntry.buffer(), modesEntry.size()); // новые эффекты добавляются в конец списка: настройки прежних сохраняются, новые получают значения по умолчанию
        }
        db.set(kk::modes_blob, gdb::AnyType((const void*)modes, sizeof(ModeType) * MODE_AMOUNT));
      }

      gdb::Entry alarmsEntry = db.get(kk::alarms_blob);
      if (alarmsEntry.size() == sizeof(AlarmType) * 7)
      {
        alarmsEntry.writeBytes(alarms);
      }
      else
      {
        db.set(kk::alarms_blob, gdb::AnyType((const void*)alarms, sizeof(AlarmType) * 7));
      }

      readFavoritesSettings();

      *espMode = (uint8_t)db[kk::esp_mode];
      // без опции «Включаться после подачи питания» лампа стартует выключенной, но после намеренной программной
      // перезагрузки (OTA, кнопка "Перезагрузка", смена режима WiFi) состояние восстанавливается
      bool softRestart = ESP.getResetReason() == F("Software/System restart");
      *onFlag = (softRestart || (bool)db[kk::hw_power_restore]) ? (bool)db[kk::lamp_on] : false;
      *dawnMode = (uint8_t)db[kk::dawn_mode];
      *currentMode = (uint8_t)db[kk::current_mode];
      if (*buttonEnabled) *buttonEnabled = (bool)db[kk::button_enabled]; // если кнопка уже заблокирована при старте (BUTTON_LOCK_ON_START), сохранённое значение не разблокирует её
      #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      *random_on = (uint8_t)db[kk::rnd_cycle_on];
      #endif //#ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      *button_sleep_time = (uint8_t)db[kk::btn_sleep_time];

      db.update();                                          // немедленная запись файла, если что-то инициализировалось
    }

    // тикер отложенной записи; вызывать в каждом цикле loop()
    static void Tick()
    {
      db.tick();
    }

    #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
    static void Save_random_on(uint8_t* random_on)
    {
      db.set(kk::rnd_cycle_on, *random_on);
    }
    #endif //RANDOM_SETTINGS_IN_CYCLE_MODE

    static void Save_button_sleep_time(uint8_t* button_sleep_time)
    {
      db.set(kk::btn_sleep_time, *button_sleep_time);
    }

    static void SaveModesSettings(uint8_t* currentMode, ModeType modes[])
    {
      (void)currentMode;                                    // настройки эффектов хранятся одним блобом, записывается весь массив
      db.set(kk::modes_blob, gdb::AnyType((const void*)modes, sizeof(ModeType) * MODE_AMOUNT));
    }

    // отложенная запись изменённых настроек; вызывается в каждом цикле loop()
    static void HandleTick(bool* settChanged, uint32_t* eepromTimeout, bool* onFlag, uint8_t* currentMode, ModeType modes[], void (*saveFavoritesSettings)())
    {
      if (*settChanged && millis() - *eepromTimeout > STORAGE_WRITE_DELAY)
      {
        *settChanged = false;
        *eepromTimeout = millis();
        db.set(kk::lamp_on, *onFlag);                     // сохраняется всегда: нужно для восстановления состояния после OTA/перезагрузки
        db.set(kk::modes_blob, gdb::AnyType((const void*)modes, sizeof(ModeType) * MODE_AMOUNT));
        db.set(kk::current_mode, *currentMode);
        saveFavoritesSettings();
      }

      db.tick();                                            // запись файла на флеш, если данные в БД менялись
    }

    static void SaveAlarmsSettings(uint8_t* alarmNumber, AlarmType alarms[])
    {
      (void)alarmNumber;                                    // будильники хранятся одним блобом, записывается весь массив
      db.set(kk::alarms_blob, gdb::AnyType((const void*)alarms, sizeof(AlarmType) * 7));
    }

    static void SaveEspMode(uint8_t* espMode)
    {
      db.set(kk::esp_mode, *espMode);
      db.update();                                          // espMode сохраняется перед перезагрузкой - файл нужно записать немедленно
    }

    static void SaveDawnMode(uint8_t* dawnMode)
    {
      db.set(kk::dawn_mode, *dawnMode);
    }

    static void SaveButtonEnabled(bool* buttonEnabled)
    {
      db.set(kk::button_enabled, *buttonEnabled);
    }
};
