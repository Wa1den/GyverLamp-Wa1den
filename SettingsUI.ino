// Веб-интерфейс настроек лампы (библиотека Settings, вариант SettingsGyverWS:
// синхронный вебсервер GyverHTTP + WebSocket на порту 81, страница доступна
// по IP лампы на порту 80; обработка запросов выполняется в loop()).
//
// Виджеты с ключами kk::* читают и пишут значения напрямую в базу настроек (Storage.h).
// Виджеты состояния лампы (питание/эффект/яркость/...) показывают текущие значения
// глобалов и применяют изменения через слой LampControl (LampControl.ino).
//
// Структура страницы. sets::Group - блок прямо на главной, sets::Menu - отдельная
// страница со стрелкой "назад". Содержимое всех разделов приходит в браузер одним
// пакетом сборки и переключается на стороне браузера; лампе уходит только уведомление
// об открытии раздела (b.enterMenu()), на нём построена ленивая сборка списка эффектов:
//
//   Лампа              группа, всё что трогают каждый день
//   Цикл эффектов      меню > Эффекты в цикле (ленивое, см. favListVisible)
//   Будильник          меню
//   Таймер выключения  группа
//   Бегущая строка     группа
//   Кнопка             группа
//   Автояркость        меню
//   Сеть               меню: WiFi, точка доступа, MQTT, Wake-on-LAN
//   Служебное          меню > Журнал

SettingsGyverWS sett("GyverLamp", &db);

// стабильные id виджетов, не привязанных к базе настроек (0xFA00xx - зона id избранных эффектов)
#define UI_ID_POWER        ("ui_pwr"_h)
#define UI_ID_EFFECT       ("ui_eff"_h)
#define UI_ID_BRIGHTNESS   ("ui_bri"_h)
#define UI_ID_SPEED        ("ui_spd"_h)
#define UI_ID_SCALE        ("ui_sca"_h)
#define UI_ID_FAV_ON       ("ui_fav_on"_h)
#define UI_ID_FAV_INTERVAL ("ui_fav_int"_h)
#define UI_ID_FAV_DISP     ("ui_fav_dsp"_h)
#define UI_ID_FAV_SAVED    ("ui_fav_sav"_h)
#define UI_ID_FAV_RANDOM   ("ui_fav_rnd"_h)
#define UI_ID_FAV_MODE(i)  (0xFA0000UL + (i))
#define UI_ID_ALARM_ON(i)  (0xA1A000UL + (i))
#define UI_ID_ALARM_T(i)   (0xA1B000UL + (i))
#define UI_ID_DAWN_MODE    ("ui_dawn"_h)
#define UI_ID_TIMER_MIN    ("ui_tmr_min"_h)
#define UI_ID_TIMER_START  ("ui_tmr_go"_h)
#define UI_ID_TIMER_STOP   ("ui_tmr_off"_h)
#define UI_ID_TEXT         ("ui_text"_h)
#define UI_ID_TEXT_IP      ("ui_text_ip"_h)
#define UI_ID_BTN_ENABLED  ("ui_btn_en"_h)
#define UI_ID_ESP_MODE     ("ui_espmode"_h)
#define UI_ID_AP_APPLY     ("ui_ap_app"_h)
#define UI_ID_HOST_APPLY   ("ui_host_app"_h)
#define UI_ID_MQTT_APPLY   ("ui_mqtt_app"_h)
#define UI_ID_WOL_WAKE     ("ui_wol_wake"_h)
#define UI_ID_AB_RAW       ("ui_ab_raw"_h)
#define UI_ID_AB_SET_DARK  ("ui_ab_sdrk"_h)
#define UI_ID_AB_SET_LIGHT ("ui_ab_slgt"_h)
#define UI_ID_AB_FACTOR    ("ui_ab_fct"_h)
#define UI_ID_SET_TIME     ("ui_time"_h)
#define UI_ID_NTP_SYNC     ("ui_ntp_sync"_h)
#define UI_ID_LOG          ("ui_log"_h)
#define UI_ID_FX_RESET     ("ui_fx_rst"_h)
#define UI_ID_WIFI_RESET   ("ui_wifi_rst"_h)
#define UI_ID_REBOOT       ("ui_reboot"_h)

static uint16_t uiSleepMinutes = 30U;                       // значение поля "минут" для таймера выключения (подставляется из button_sleep_time при старте)

// Список эффектов для режима Цикл - это MODE_AMOUNT переключателей с названиями, он заметно
// утяжеляет пакет страницы, а нужен редко. Поэтому в сборку он попадает только после того,
// как пользователь открыл соответствующее меню, и убирается снова, когда страницу закрыли
// (пока страница открыта, список из меню не пропадает)
static bool favListVisible = false;                         // строить ли список в текущей сборке страницы
static bool pendingFavReload = false;                       // запрошено перестроение страницы, чтобы показать список

static const char* const uiDayNames[7] = {"Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота", "Воскресенье"};

void settingsBuild(sets::Builder& b)
{
  // --- ЛАМПА ---------------------------------
  {
    sets::Group g(b, "Лампа");

    bool power = ONflag;
    if (b.Switch(UI_ID_POWER, "Питание", &power))
    {
      lampSetPower(power);
    }

    uint8_t effect = currentMode;
    if (b.Select(UI_ID_EFFECT, "Эффект", FPSTR(effectNamesList), &effect))
    {
      lampSetEffect(effect);
      b.reload();                                           // перестроить страницу, чтобы ползунки подтянули яркость/скорость/масштаб нового эффекта
    }

    uint8_t brightness = modes[currentMode].Brightness;
    if (b.Slider(UI_ID_BRIGHTNESS, "Яркость", 1, 255, 1, "", &brightness))
    {
      lampSetBrightness(brightness);
    }

    uint8_t speed = modes[currentMode].Speed;
    if (b.Slider(UI_ID_SPEED, "Скорость", 1, 255, 1, "", &speed))
    {
      lampSetSpeed(speed);
    }

    uint8_t scale = modes[currentMode].Scale;
    if (b.Slider(UI_ID_SCALE, "Масштаб", 1, 100, 1, "", &scale))
    {
      lampSetScale(scale);
    }
  }

  // --- ЦИКЛ (АВТОМАТИЧЕСКАЯ СМЕНА ИЗБРАННЫХ ЭФФЕКТОВ) ---
  {
    sets::Menu page(b, "Цикл эффектов");                    // отдельная страница: настройки цикла нужны редко

    bool favOn = FavoritesManager::FavoritesRunning != 0;
    if (b.Switch(UI_ID_FAV_ON, "Включен", &favOn))
    {
      lampSetFavoritesRunning(favOn);
    }

    uint16_t interval = FavoritesManager::Interval;
    if (b.Number(UI_ID_FAV_INTERVAL, "Интервал смены, сек", &interval, 1, 65535))
    {
      FavoritesManager::Interval = interval;
      updateSets();
    }

    uint16_t dispersion = FavoritesManager::Dispersion;
    if (b.Number(UI_ID_FAV_DISP, "Случайный разброс, сек", &dispersion, 0, 65535))
    {
      FavoritesManager::Dispersion = dispersion;
      updateSets();
    }

    bool useSaved = FavoritesManager::UseSavedFavoritesRunning != 0;
    if (b.Switch(UI_ID_FAV_SAVED, "Помнить вкл/выкл после перезагрузки", &useSaved))
    {
      FavoritesManager::UseSavedFavoritesRunning = useSaved ? 1U : 0U;
      updateSets();
    }

    #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
    bool rndOn = random_on != 0;
    if (b.Switch(UI_ID_FAV_RANDOM, "Случайные настройки эффектов", &rndOn))
    {
      random_on = rndOn ? 1U : 0U;
      Storage::Save_random_on(&random_on);
    }
    #endif //RANDOM_SETTINGS_IN_CYCLE_MODE

    {
      sets::Menu m(b, "Эффекты в цикле");

      if (b.enterMenu() && !favListVisible)                 // пользователь открыл меню, а содержимого ещё нет в сборке -
      {                                                     // строим его и просим страницу обновиться
        favListVisible = true;
        pendingFavReload = true;
      }

      if (favListVisible)
      {
        for (uint8_t i = 0; i < MODE_AMOUNT; i++)
        {
          bool selected = FavoritesManager::FavoriteModes[i] != 0;
          if (b.Switch(UI_ID_FAV_MODE(i), getEffectName(i), &selected))
          {
            lampSetFavoriteMode(i, selected);
          }
        }
      }
      else
      {
        b.Label("Список", "загрузится при открытии");        // пустое меню вебморда не показывает вовсе, поэтому заглушка обязательна
      }
    }
  }

  // --- БУДИЛЬНИК (РАССВЕТ) -------------------
  {
    sets::Menu page(b, "Будильник (рассвет)");

    for (uint8_t i = 0; i < 7U; i++)
    {
      bool state = alarms[i].State;
      if (b.Switch(UI_ID_ALARM_ON(i), uiDayNames[i], &state))
      {
        lampSetAlarm(i, state, alarms[i].Time);
      }

      uint32_t seconds = alarms[i].Time * 60UL;             // виджет времени работает в секундах от начала суток, будильник - в минутах
      if (b.Time(UI_ID_ALARM_T(i), "Время", &seconds))
      {
        lampSetAlarm(i, alarms[i].State, seconds / 60UL);
      }
    }

    uint8_t dawn = dawnMode;
    if (b.Select(UI_ID_DAWN_MODE, "Рассвет начинается за",
                 "5 минут;10 минут;15 минут;20 минут;25 минут;30 минут;40 минут;50 минут;60 минут", &dawn))
    {
      lampSetDawnMode(dawn);
    }
  }

  // --- ТАЙМЕР ВЫКЛЮЧЕНИЯ ---------------------
  {
    sets::Group g(b, "Таймер выключения");

    if (TimerManager::TimerRunning)
    {
      b.LabelNum("Осталось, мин", (uint32_t)((TimerManager::TimeToFire - millis()) / 60000ULL) + 1U);
    }
    else
    {
      b.Label("Состояние", "отключен");
    }

    b.Number(UI_ID_TIMER_MIN, "Минут", &uiSleepMinutes, 1, 255);

    {
      sets::Buttons btns(b);
      if (b.Button(UI_ID_TIMER_START, "Запустить"))
      {
        lampSetSleepTimer(uiSleepMinutes);
      }
      if (b.Button(UI_ID_TIMER_STOP, "Отключить"))
      {
        lampClearSleepTimer();
      }
    }
  }

  // --- БЕГУЩАЯ СТРОКА ------------------------
  {
    sets::Group g(b, "Бегущая строка");

    bool showIp = (bool)db[kk::run_text_ip];                 // включено - строка показывает адрес лампы,
    if (b.Switch(UI_ID_TEXT_IP, "Писать текущий IP", &showIp)) // поле "Текст" при этом не используется и не затирается
    {
      lampSetRunningTextShowIp(showIp);
    }

    String text = TextTicker;
    if (b.Input(UI_ID_TEXT, "Текст", &text))
    {
      lampSetRunningText(text.c_str());
    }
  }

  // --- КНОПКА --------------------------------
  #ifdef ESP_USE_BUTTON
  {
    sets::Group g(b, "Кнопка");
    bool enabled = buttonEnabled;
    if (b.Switch(UI_ID_BTN_ENABLED, "Разблокирована", &enabled))
    {
      lampSetButtonEnabled(enabled);
    }
  }
  #endif //ESP_USE_BUTTON

  // --- АВТОЯРКОСТЬ ---------------------------
  #ifdef USE_AUTO_BRIGHTNESS
  {
    sets::Menu page(b, "Автояркость");                      // отдельная страница: настраивается один раз при калибровке
    b.Switch(kk::ab_on, "Использовать датчик освещённости");
    b.Slider(kk::ab_min_bri, "Мин. яркость в темноте, %", 5, 100, 1);

    // двухточечная калибровка под конкретный датчик: рабочий диапазон дешёвых модулей
    // занимает малую часть шкалы 0-1023, поэтому крайние точки запоминаются по факту
    {
      sets::Buttons btns(b);
      if (b.Button(UI_ID_AB_SET_DARK, "Запомнить темноту"))   // нажать, накрыв датчик
      {
        db.set(kk::ab_dark, autoLightRaw);
        uiLog.printf_P(PSTR("Автояркость: точка темноты = %u\n"), autoLightRaw);
        b.reload();
      }
      if (b.Button(UI_ID_AB_SET_LIGHT, "Запомнить свет"))     // нажать при обычном дневном освещении (не с фонариком)
      {
        db.set(kk::ab_light, autoLightRaw);
        uiLog.printf_P(PSTR("Автояркость: точка света = %u\n"), autoLightRaw);
        b.reload();
      }
    }
    b.Label("Точки калибровки (темнота/свет)", String((uint16_t)db[kk::ab_dark]) + " / " + String((uint16_t)db[kk::ab_light]));

    b.LabelNum(UI_ID_AB_RAW, "Датчик A0 (0-1023)", autoLightRaw);              // опрашивается только при включённой автояркости; накройте датчик рукой - число должно меняться
    b.LabelNum(UI_ID_AB_FACTOR, "Текущий коэффициент, %", (uint16_t)autoBriFactor * 100U / 255U);
  }
  #endif //USE_AUTO_BRIGHTNESS

  // --- СЕТЬ ----------------------------------
  {
    sets::Menu page(b, "Сеть");                             // WiFi, точка доступа, MQTT и Wake-on-LAN - на отдельной странице

    // --- WIFI ----------------------------------
    {
      sets::Group g(b, "WiFi");
      b.Input(kk::wifi_ssid, "Имя сети (SSID)");
      b.Pass(kk::wifi_pass, "Пароль");

      if (b.Button(kk::wifi_connect, "Подключить"))
      {
        pendingWifiConnect = true;                          // подключение выполнится в loop (wifiTick), а не в контексте асинхронного вебсервера
      }

      uint8_t mode = espMode;
      if (b.Select(UI_ID_ESP_MODE, "Режим работы", "Точка доступа;Клиент (через роутер)", &mode))
      {
        if (mode != espMode)
        {
          espMode = mode;
          Storage::SaveEspMode(&espMode);
          pendingRestart = true;                            // смена режима применяется перезагрузкой (как семикратный клик кнопкой)
        }
      }

      b.Input(kk::host_name, "Имя лампы в сети");

      String hostAddress = F("http://");                    // hostName() отбрасывает недопустимые символы, поэтому в ссылке виден адрес,
      hostAddress += hostName();                            // который лампа получит после перезагрузки, а не введённое в поле
      hostAddress += F(".local");

      // строка собирается вручную из классов библиотеки: готовый виджет ссылки показывает только
      // стрелку, а HTML-виджет с подписью уводит содержимое на строку ниже. Классы widget_row и
      // value дают тот же вид, что у соседних строк, а flex-wrap переносит адрес, если он не влез
      String hostLink = F("<div class=\"widget_row\" style=\"flex-wrap:wrap;height:unset;margin:-5px 0\">"
                          "<label class=\"widget_label\">Адрес лампы</label>"
                          "<a class=\"value\" style=\"color:var(--accent);flex-shrink:0\" target=\"_blank\" href=\"");
      hostLink += hostAddress;
      hostLink += F("\">");
      hostLink += hostAddress;
      hostLink += F("</a></div>");
      b.HTML("", hostLink);

      if (b.Button(UI_ID_HOST_APPLY, "Применить (перезагрузка)"))
      {
        pendingRestart = true;                              // имя уходит роутеру в DHCP-запросе при подключении, поэтому применяется при старте
      }
    }

    // --- ТОЧКА ДОСТУПА -------------------------
    {
      sets::Group g(b, "Точка доступа");
      b.Input(kk::ap_name, "Имя сети (SSID)");
      b.Pass(kk::ap_pass, "Пароль (8-63 символа, пусто - без пароля)");

      if (b.Button(UI_ID_AP_APPLY, "Применить (перезагрузка)"))
      {
        String apPassword = (String)db[kk::ap_pass];
        if (apPassword.length() && apPassword.length() < AP_PASS_MIN_LENGTH)  // с таким паролем точка доступа не поднимется, поэтому перезагружаться нельзя: лампа останется без сети
        {
          uiLog.println(F("Точка доступа: пароль короче 8 символов, изменения не применены"));
        }
        else
        {
          pendingRestart = true;                            // новое имя и пароль применяются при старте (текущее подключение к точке доступа в любом случае разрывается)
        }
      }
    }

    // --- MQTT ----------------------------------
    #if (USE_MQTT)
    {
      sets::Group g(b, "MQTT");
      b.Switch(kk::mqtt_enabled, "Включен");
      b.Input(kk::mqtt_host, "Адрес брокера");
      b.Number(kk::mqtt_port, "Порт");
      b.Input(kk::mqtt_user, "Пользователь");
      b.Pass(kk::mqtt_pass, "Пароль");

      if (MqttManager::getTopicInput().length())
      {
        // Paragraph вместо Label: топики длинные, в однострочный Label не влезают
        b.Paragraph("Топики", String(F("Команды: ")) + MqttManager::getTopicInput() +
                              String(F("\nСостояние: ")) + MqttManager::getTopicOutput());
      }

      if (b.Button(UI_ID_MQTT_APPLY, "Применить (перезагрузка)"))
      {
        pendingRestart = true;                              // новые параметры брокера применяются при старте
      }
    }
    #endif //USE_MQTT

    // --- WAKE-ON-LAN ---------------------------
    {
      sets::Group g(b, "Wake-on-LAN");
      b.Input(kk::wol_mac, "MAC компьютера");

      if (b.Button(UI_ID_WOL_WAKE, "Разбудить"))
      {
        pendingWolWake = true;                              // отправка выполнится в loop, результат - в Журнале
      }

      #if (USE_MQTT)
      if (b.Switch(kk::wol_ext_on, "Использовать дополнительный топик"))
      {
        pendingWolResub = true;                             // подписка обновится в loop
      }
      if (b.Input(kk::wol_ext_topic, "Дополнительный топик"))
      {
        pendingWolResub = true;
      }
      #endif //USE_MQTT
    }

  }

  // --- СЛУЖЕБНОЕ -----------------------------
  {
    sets::Menu page(b, "Служебное");                        // отдельная страница: информация, время, сбросы и Журнал

    b.Label("Прошивка", FIRMWARE_TITLE);                      // см. Version.h

    b.Label("IP адрес", WiFiConnector.connected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
    b.LabelNum("Свободная память, байт", ESP.getFreeHeap());

    #if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
    char timeBuf[9];
    getFormattedTime(timeBuf);
    b.Label("Время лампы", timeBuf);
    #ifdef USE_NTP
    b.Label("Синхронизация времени", timeSynched ? (ntpServerAddressResolved ? "выполнена (NTP)" : "выполнена (вручную)") : "не выполнена");
    b.Input(kk::ntp_host, "NTP сервер");
    if (b.Button(UI_ID_NTP_SYNC, "Синхронизировать время"))
    {
      pendingNtpSync = true;                                // синхронизация выполнится в loop (применит и новый адрес сервера), результат - в Журнале
    }
    #else
    b.Label("Синхронизация времени", timeSynched ? "выполнена (вручную)" : "не выполнена");
    #endif //USE_NTP
    #endif //#if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)

    #ifdef USE_MANUAL_TIME_SETTING
    // в поле подставляется текущее время лампы (если оно синхронизировано); виджет показывает
    // время с поправкой на часовой пояс браузера, поэтому ему передаётся UTC (иначе время задваивает пояс)
    uint32_t unixTime = 0;
    if (timeSynched)
    {
      #ifdef USE_NTP
      unixTime = (uint32_t)localTimeZone.toUTC(getCurrentLocalTime());
      #elif !defined(SUMMER_WINTER_TIME)
      unixTime = (uint32_t)getCurrentLocalTime() - LOCAL_OFFSET * 60UL;
      #else
      unixTime = (uint32_t)getCurrentLocalTime();
      #endif
    }
    if (b.DateTime(UI_ID_SET_TIME, "Установить время вручную", &unixTime))
    {
      if (unixTime > 0)
      {
        lampSetManualTime(unixTime);
        b.reload();                                         // обновить "Время лампы" и статус синхронизации
      }
    }
    #endif //USE_MANUAL_TIME_SETTING

    {
      sets::Buttons btns(b);
      if (b.Button(UI_ID_FX_RESET, "Сброс эффектов"))
      {
        restoreSettings();                                  // настройки всех эффектов на значения по умолчанию
        updateSets();
        b.reload();                                         // ползунки должны подтянуть новые значения
      }
      if (b.Button(UI_ID_WIFI_RESET, "Сброс WiFi"))
      {
        pendingWifiReset = true;
      }
      if (b.Button(UI_ID_REBOOT, "Перезагрузка"))
      {
        pendingRestart = true;
      }
    }

    {
      sets::Menu m(b, "Журнал");                            // вложенное меню - журнал скрыт, пока его не откроют
      b.Log(UI_ID_LOG, uiLog);
    }
  }
}

// Профилирование веб-интерфейса. Библиотека Settings пропатчена (libs/Settings/src/core/profile.h)
// и сообщает сюда длительность каждого этапа обработки запроса страницы. Пишем в Журнал всё,
// что заняло больше UI_PROFILE_MS: суммарный замер стадии "веб-интерфейс" в loop() показывает
// только факт подвисания, а эти записи - какой именно этап его вызвал.
#ifdef UI_PROFILE_MS

// действие страницы приходит уже хэшем, восстанавливаем имя для журнала
static const char* uiActionName(uint32_t hash)
{
  switch (hash)
  {
    case su::SH("load"):     return "load";                 // первая сборка страницы
    case su::SH("update"):   return "update";               // периодический опрос
    case su::SH("set"):      return "set";                  // изменение виджета
    case su::SH("click"):    return "click";                // нажатие кнопки
    case su::SH("menu"):     return "menu";                 // открытие вложенного меню
    case su::SH("fs"):       return "fs";                   // список файлов - его шлёт открытие бокового меню
    case su::SH("ping"):     return "ping";
    case su::SH("discover"): return "discover";
    case su::SH("unfocus"):  return "unfocus";              // страницу закрыли
    case su::SH("remove"):   return "remove";
    case su::SH("create"):   return "create";
  }
  return nullptr;
}

static void uiProfileStage(const char* stage, uint32_t ms, uint32_t arg)
{
  if (ms < UI_PROFILE_MS)
  {
    return;
  }

  static const char* lastStage = nullptr;                   // защита от лавины: одна и та же стадия пишется не чаще раза в секунду.
  static uint32_t lastLogMs = 0U;                           // запись в Журнал вызывает отправку в браузер, а медленная отправка - новую
  uint32_t now = millis();                                  // запись, и при затыке в сети журнал вытеснил бы сам себя
  if (stage == lastStage && now - lastLogMs < 1000UL)
  {
    return;
  }
  lastStage = stage;
  lastLogMs = now;

  uiLog.printf_P(PSTR("Веб: %s %u мс"), stage, ms);
  const char* action = uiActionName(arg);                   // стадия "запрос" передаёт хэш действия, остальные - размер данных
  if (action)
  {
    uiLog.printf_P(PSTR(" [%s]"), action);
  }
  else if (arg)
  {
    uiLog.printf_P(PSTR(" [%u Б]"), arg);
  }
  uiLog.printf_P(PSTR(" (память %u, блок %u, фрагм %u%%)"),
                 ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(), ESP.getHeapFragmentation());
  uiLog.println();
}
#endif //UI_PROFILE_MS

void settingsSetup()
{
  #ifdef UI_PROFILE_MS
  sets::onProfile(uiProfileStage);                          // до sett.begin(), чтобы попал и запуск сервера
  #endif

  #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
  uiSleepMinutes = button_sleep_time;                       // последнее использованное время таймера - в поле веб-интерфейса
  #endif

  sett.begin(true, hostName().c_str());                     // запускается после WiFiConnector.connect, иначе не подхватится captive DNS.
                                                            // второй аргумент - имя для mDNS, по нему лампа отвечает на <имя>.local;
                                                            // вызывать можно до подключения к роутеру: MDNS.begin ставит колбэк
                                                            // lwIP и перезапускает ответчик, когда интерфейс поднимается
  sett.onBuild(settingsBuild);
  sett.setUpdatePeriod(3000);                               // период опроса страницы браузером. В варианте с вебсокетом библиотека всё равно
                                                            // отдаёт браузеру 0: виджеты обновляются пушем по вебсокету, а не опросом.
                                                            // Признак "страница открыта" (от него зависят живые обновления и сборка списка
                                                            // эффектов Цикла) держит пинг страницы раз в 2.5 с - вдвое чаще FOCUS_TOUT (5000 мс)
  sett.setVersion(FIRMWARE_TITLE);                          // строка Firmware в инфо-панели веб-интерфейса (см. Version.h)
}

void settingsTick()
{
  sett.tick();

  if (pendingFavReload)                                     // показать список эффектов Цикла: перестраиваем страницу уже вместе с ним
  {
    pendingFavReload = false;
    sett.reload();
  }
  if (favListVisible && !sett.focused())
  {
    favListVisible = false;                                 // страницу закрыли - следующее её открытие снова будет лёгким
  }

  settingsSyncTick();
}

// живая синхронизация открытой страницы: если состояние лампы изменили другим каналом
// (MQTT, кнопка, режим Цикл, таймер), новые значения виджетов отправляются в браузер
// через WebSocket. Если страница не открыта, updater ничего не отправляет.
void settingsSyncTick()
{
  static uint32_t lastCheckTime = 0U;
  if (millis() - lastCheckTime < 1000U)                     // проверка раз в секунду
  {
    return;
  }
  lastCheckTime = millis();

  static bool lastPower = false;
  static uint8_t lastEffect = 255U;
  static uint8_t lastBrightness = 0U;
  static uint8_t lastSpeed = 0U;
  static uint8_t lastScale = 0U;
  static bool lastFavOn = false;

  bool favOn = FavoritesManager::FavoritesRunning != 0;
  if (lastPower != ONflag || lastEffect != currentMode ||
      lastBrightness != modes[currentMode].Brightness ||
      lastSpeed != modes[currentMode].Speed ||
      lastScale != modes[currentMode].Scale ||
      lastFavOn != favOn)
  {
    lastPower = ONflag;
    lastEffect = currentMode;
    lastBrightness = modes[currentMode].Brightness;
    lastSpeed = modes[currentMode].Speed;
    lastScale = modes[currentMode].Scale;
    lastFavOn = favOn;

    sett.updater()
        .update(UI_ID_POWER, ONflag)
        .update(UI_ID_EFFECT, currentMode)
        .update(UI_ID_BRIGHTNESS, modes[currentMode].Brightness)
        .update(UI_ID_SPEED, modes[currentMode].Speed)
        .update(UI_ID_SCALE, modes[currentMode].Scale)
        .update(UI_ID_FAV_ON, favOn);
  }

  if (uiLog._changed())                                     // новые записи журнала - в открытую страницу
  {
    sett.updater().update(UI_ID_LOG, static_cast<sets::Logger&>(uiLog)); // приведение к базовому типу, иначе побеждает шаблонная перегрузка update(id, T) по значению
  }

  #ifdef USE_AUTO_BRIGHTNESS
  static uint16_t lastAbRaw = 0xFFFFU;
  static uint8_t lastAbFactor = 0U;
  if (autoLightRaw != lastAbRaw || autoBriFactor != lastAbFactor) // диагностику автояркости шлём только при изменении (когда страница закрыта - ничего не отправляется)
  {
    lastAbRaw = autoLightRaw;
    lastAbFactor = autoBriFactor;
    sett.updater()
        .update(UI_ID_AB_RAW, autoLightRaw)
        .update(UI_ID_AB_FACTOR, (uint16_t)autoBriFactor * 100U / 255U);
  }
  #endif //USE_AUTO_BRIGHTNESS
}
