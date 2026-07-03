// Веб-интерфейс настроек лампы (библиотека Settings, вариант SettingsGyverWS:
// синхронный вебсервер GyverHTTP + WebSocket на порту 81, страница доступна
// по IP лампы на порту 80; обработка запросов выполняется в loop()).
//
// Виджеты с ключами kk::* читают и пишут значения напрямую в базу настроек (Storage.h).
// Виджеты состояния лампы (питание/эффект/яркость/...) показывают текущие значения
// глобалов и применяют изменения через слой LampControl (LampControl.ino).

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
#define UI_ID_BTN_ENABLED  ("ui_btn_en"_h)
#define UI_ID_ESP_MODE     ("ui_espmode"_h)
#define UI_ID_MQTT_APPLY   ("ui_mqtt_app"_h)
#define UI_ID_SET_TIME     ("ui_time"_h)
#define UI_ID_FX_RESET     ("ui_fx_rst"_h)
#define UI_ID_WIFI_RESET   ("ui_wifi_rst"_h)
#define UI_ID_REBOOT       ("ui_reboot"_h)

static uint16_t uiSleepMinutes = 30U;                       // значение поля "минут" для таймера выключения (подставляется из button_sleep_time при старте)

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
    sets::Group g(b, "Цикл эффектов");

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
      for (uint8_t i = 0; i < MODE_AMOUNT; i++)
      {
        bool selected = FavoritesManager::FavoriteModes[i] != 0;
        if (b.Switch(UI_ID_FAV_MODE(i), getEffectName(i), &selected))
        {
          lampSetFavoriteMode(i, selected);
        }
      }
    }
  }

  // --- БУДИЛЬНИК (РАССВЕТ) -------------------
  {
    sets::Menu m(b, "Будильник (рассвет)");

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

  // --- WIFI ----------------------------------
  {
    sets::Group g(b, "WiFi");
    b.Input(kk::wifi_ssid, "Имя сети (SSID)");
    b.Pass(kk::wifi_pass, "Пароль");

    if (b.Button(kk::wifi_connect, "Подключить"))
    {
      pendingWifiConnect = true;                            // подключение выполнится в loop (wifiTick), а не в контексте асинхронного вебсервера
    }

    uint8_t mode = espMode;
    if (b.Select(UI_ID_ESP_MODE, "Режим работы", "Точка доступа;Клиент (через роутер)", &mode))
    {
      if (mode != espMode)
      {
        espMode = mode;
        Storage::SaveEspMode(&espMode);
        pendingRestart = true;                              // смена режима применяется перезагрузкой (как семикратный клик кнопкой)
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
      b.Label("Топик команд", MqttManager::getTopicInput());
      b.Label("Топик состояния", MqttManager::getTopicOutput());
    }

    if (b.Button(UI_ID_MQTT_APPLY, "Применить (перезагрузка)"))
    {
      pendingRestart = true;                                // новые параметры брокера применяются при старте
    }
  }
  #endif //USE_MQTT

  // --- СЛУЖЕБНОЕ -----------------------------
  {
    sets::Group g(b, "Служебное");

    b.Label("IP адрес", WiFiConnector.connected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString());

    #if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
    char timeBuf[9];
    getFormattedTime(timeBuf);
    b.Label("Время лампы", timeBuf);
    #ifdef USE_NTP
    b.Label("Синхронизация времени", timeSynched ? (ntpServerAddressResolved ? "выполнена (NTP)" : "выполнена (вручную)") : "не выполнена");
    #else
    b.Label("Синхронизация времени", timeSynched ? "выполнена (вручную)" : "не выполнена");
    #endif //USE_NTP
    #endif //#if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)

    #ifdef USE_MANUAL_TIME_SETTING
    uint32_t unixTime = (uint32_t)getCurrentLocalTime();    // в поле подставляется текущее время лампы
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
  }
}

void settingsSetup()
{
  #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
  uiSleepMinutes = button_sleep_time;                       // последнее использованное время таймера - в поле веб-интерфейса
  #endif

  sett.begin();                                             // запускается после WiFiConnector.connect, иначе не подхватится captive DNS
  sett.onBuild(settingsBuild);
}

void settingsTick()
{
  sett.tick();
}
