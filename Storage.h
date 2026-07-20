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
    esp_mode,                                               // режим работы лампы: 0 - точка доступа, 1 - клиент WiFi (подключение к роутеру)

    // Лампа
    lamp_on,                                                // состояние лампы (вкл/выкл)
    current_mode,                                           // номер текущего эффекта
    button_enabled,                                         // признак "кнопка разблокирована"
    dawn_mode,                                              // время до "рассвета" (номер опции в списке)
    rnd_cycle_on,                                           // вкл/выкл случайных настроек эффектов в режиме Цикл (ключи в DB_KEYS - глобальные имена, поэтому имя не совпадает с переменной random_on)
    btn_sleep_time,                                         // время таймера сна, устанавливаемого двойным кликом кнопки, минуты (имя не совпадает с переменной button_sleep_time)
    running_text,                                           // текст эффекта Бегущая строка

    // Автояркость
    ab_on,                                                  // вкл/выкл автояркости по датчику освещённости
    ab_min_bri,                                             // минимальная яркость в темноте, % (5-100)
    ab_dark,                                                // калибровка: значение A0 в темноте
    ab_light,                                               // калибровка: значение A0 при свете (если меньше ab_dark - шкала автоматически инвертируется)

    // Избранное (режим Цикл)
    ntp_host,                                               // адрес NTP сервера (сервера точного времени)
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

#define STORAGE_WRITE_DELAY   (30000UL)                     // отсрочка записи настроек эффектов после последнего изменения (чтобы не изнашивать флеш при регулировке ползунками)

class Storage
{
  public:
    static void InitSettings(ModeType modes[], AlarmType alarms[], uint8_t* espMode, bool* onFlag, uint8_t* dawnMode, uint8_t* currentMode, bool* buttonEnabled
      #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      , uint8_t* random_on
      #endif //ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
      , uint8_t* button_sleep_time
      #endif //#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
      , void (*readFavoritesSettings)(), void (*saveFavoritesSettings)(), void (*restoreDefaultSettings)())
    {
      LittleFS.begin();
      db.begin();                                           // чтение базы настроек из файла (или создание пустого файла при первом запуске)

      bool firstRun = !db.has(kk::current_mode);

      restoreDefaultSettings();                             // заполнение modes[] настройками эффектов по умолчанию (перекроются данными из БД ниже, если они там есть)

      // создание ячеек с начальными значениями (db.init записывает значение, только если ячейки ещё нет)
      db.init(kk::wifi_ssid, "");
      db.init(kk::wifi_pass, "");
      db.init(kk::esp_mode, (uint8_t)ESP_MODE);
      db.init(kk::lamp_on, false);
      db.init(kk::dawn_mode, (uint8_t)0);
      db.init(kk::current_mode, (uint8_t)0);
      db.init(kk::button_enabled, true);
      #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      db.init(kk::rnd_cycle_on, (uint8_t)RANDOM_SETTINGS_IN_CYCLE_MODE);
      #endif //RANDOM_SETTINGS_IN_CYCLE_MODE
      #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
      db.init(kk::btn_sleep_time, (uint8_t)1);
      #endif //#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
      db.init(kk::running_text, RUNNING_TEXT_DEFAULT);
      #ifdef USE_AUTO_BRIGHTNESS
      db.init(kk::ab_on, false);
      db.init(kk::ab_min_bri, (uint8_t)20);
      db.init(kk::ab_dark, (uint16_t)0);
      db.init(kk::ab_light, (uint16_t)1023);
      #endif //USE_AUTO_BRIGHTNESS
      #ifdef USE_NTP
      db.init(kk::ntp_host, NTP_ADDRESS);
      #endif //USE_NTP
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
      else                                                  // количество эффектов изменилось после обновления прошивки - остаются настройки по умолчанию
      {
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
      #ifdef DONT_TURN_ON_AFTER_SHUTDOWN
      // после подачи питания лампа стартует выключенной, но после намеренной программной
      // перезагрузки (OTA, кнопка "Перезагрузка", смена режима WiFi) состояние восстанавливается
      *onFlag = (ESP.getResetReason() == F("Software/System restart")) ? (bool)db[kk::lamp_on] : false;
      #else
      *onFlag = (bool)db[kk::lamp_on];
      #endif
      *dawnMode = (uint8_t)db[kk::dawn_mode];
      *currentMode = (uint8_t)db[kk::current_mode];
      if (*buttonEnabled) *buttonEnabled = (bool)db[kk::button_enabled]; // если кнопка уже заблокирована при старте (BUTTON_LOCK_ON_START), сохранённое значение не разблокирует её
      #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      *random_on = (uint8_t)db[kk::rnd_cycle_on];
      #endif //#ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
      *button_sleep_time = (uint8_t)db[kk::btn_sleep_time];
      #endif //#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)

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

    #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
    static void Save_button_sleep_time(uint8_t* button_sleep_time)
    {
      db.set(kk::btn_sleep_time, *button_sleep_time);
    }
    #endif //#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)

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
