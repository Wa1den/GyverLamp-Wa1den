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
//   Обратный отсчёт    меню
//   Кубики             меню
//   Бегущая строка     группа без заголовка
//   Таймер выключения  группа
//   Настройки          меню: сведения, время, сбросы; разделы Сеть (WiFi, точка доступа, Wake-on-LAN),
//                      MQTT, Автояркость, Оборудование, Журнал

SettingsGyverWS sett("GyverLamp", &db);

// Правка вёрстки страницы. Библиотека добавляет на страницу поле css у каждого класса из custom.js.
// В исходной вёрстке подпись виджета не переносится, и на узком экране или при увеличенном масштабе
// длинная подпись выталкивает переключатель или значение за край строки. Здесь подпись и значение
// переносятся по словам, а правая часть строки не сжимается и занимает не больше 60% ширины
static const char uiCustomJs[] PROGMEM = R"js(class LampLayout {
static css = `
.widget_row{height:unset;min-height:32px}
.widget_row label{white-space:normal}
.widget_row>:last-child:not(:first-child){flex-shrink:0;max-width:60%}
.widget_row .value{white-space:normal;overflow-wrap:break-word;text-align:right}
`;
})js";

// стабильные id виджетов, не привязанных к базе настроек (0xFA00xx - зона id избранных эффектов)
#define UI_ID_POWER        ("ui_pwr"_h)
#define UI_ID_EFFECT       ("ui_eff"_h)
#define UI_ID_EFF_PREV     ("ui_eff_prev"_h)
#define UI_ID_EFF_NEXT     ("ui_eff_next"_h)
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
#define UI_ID_CD_LEFT      ("ui_cd_left"_h)
#define UI_ID_CD_MIN       ("ui_cd_min"_h)
#define UI_ID_CD_SEC       ("ui_cd_sec"_h)
#define UI_ID_CD_START     ("ui_cd_go"_h)
#define UI_ID_CD_PAUSE     ("ui_cd_pause"_h)
#define UI_ID_CD_STOP      ("ui_cd_stop"_h)
#define UI_ID_DICE(i)      (0xD1CE00UL + (i))
#define UI_ID_DICE_RESULT  ("ui_dice_res"_h)
#define UI_ID_DICE_EXIT    ("ui_dice_exit"_h)
#define UI_ID_TIMER_STATE  ("ui_tmr_state"_h)
#define UI_ID_TIMER_MIN    ("ui_tmr_min"_h)
#define UI_ID_TIMER_START  ("ui_tmr_go"_h)
#define UI_ID_TIMER_STOP   ("ui_tmr_off"_h)
#define UI_ID_TEXT         ("ui_text"_h)
#define UI_ID_TEXT_IP      ("ui_text_ip"_h)
#define UI_ID_BTN_ENABLED  ("ui_btn_en"_h)
#define UI_ID_BTN_HOLD(i)  (0xB7B000UL + (i))
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
#define UI_ID_LAMP_TIME    ("ui_lamp_time"_h)
#define UI_ID_SYNC_STATE   ("ui_sync_state"_h)
#define UI_ID_TZ_OFFSET    ("ui_tz"_h)
#define UI_ID_TZ_DST       ("ui_tz_dst"_h)
#define UI_ID_LOG          ("ui_log"_h)
#define UI_ID_FX_RESET     ("ui_fx_rst"_h)
#define UI_ID_WIFI_RESET   ("ui_wifi_rst"_h)
#define UI_ID_FX_RESET_OK  ("ui_fx_rst_ok"_h)
#define UI_ID_WIFI_RESET_OK ("ui_wifi_rst_ok"_h)
#define UI_ID_REBOOT       ("ui_reboot"_h)

static uint16_t uiSleepMinutes = 30U;                       // значение поля "минут" для таймера выключения (подставляется из button_sleep_time при старте)

// Список эффектов для режима Цикл - это MODE_AMOUNT переключателей с названиями, он заметно
// утяжеляет пакет страницы, а нужен редко. Поэтому в сборку он попадает только после того,
// как пользователь открыл соответствующее меню, и убирается снова, когда страницу закрыли
// (пока страница открыта, список из меню не пропадает)
static bool favListVisible = false;                         // строить ли список в текущей сборке страницы
static bool pendingFavReload = false;                       // запрошено перестроение страницы, чтобы показать список

// оставшееся время обратного отсчёта для поля "Осталось" - одна и та же строка в сборке страницы и в живом обновлении
static String uiCountdownText()
{
  char buf[8];
  countdownText(buf);
  String text = buf;
  if (countdownPaused())
  {
    text += F(" (пауза)");
  }
  return text;
}

// состояние таймера выключения - одна строка в сборке страницы и в живом обновлении
static String uiTimerText()
{
  if (!TimerManager::TimerRunning)
  {
    return F("отключен");
  }
  int32_t left = max((int32_t)(TimerManager::TimeToFire - millis()), (int32_t)0);
  return String(F("осталось ")) + (left / 60000L + 1) + F(" мин");
}

static const char* const uiDayNames[7] = {"Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота", "Воскресенье"};

// действия жестов кнопки по порядку ButtonAction (Types.h); удержанию доступны ещё регулировки и служебные действия
static const char uiButtonClickActions[] PROGMEM = "ничего;вкл/выкл;следующий эффект;предыдущий эффект;белый свет;"
  "таймер выключения;Цикл вкл/выкл;показать IP;показать время;бросить кубик;обратный отсчёт: старт/пауза";
static const char uiButtonHoldActions[] PROGMEM = "ничего;вкл/выкл;следующий эффект;предыдущий эффект;белый свет;"
  "таймер выключения;Цикл вкл/выкл;показать IP;показать время;бросить кубик;обратный отсчёт: старт/пауза;"
  "яркость;скорость;масштаб;обновление по воздуху;смена режима WiFi с перезагрузкой";

// «3 клика», «2 клика и удержание», «Удержание»
static String uiClicksLabel(uint8_t clicks, bool hold)
{
  if (clicks == 0U)
  {
    return F("Удержание");
  }
  String label(clicks);
  label += clicks == 1U ? F(" клик") : clicks < 5U ? F(" клика") : F(" кликов");
  if (hold)
  {
    label += F(" и удержание");
  }
  return label;
}

static size_t uiConfirmPending = 0U;                        // id окна подтверждения, которое нужно открыть на странице
static bool uiTimeRefresh = false;                          // сменили часовой пояс: обновить поля времени в «Настройках»

// значение поля ручной установки - текущее время лампы, если оно известно. Виджет показывает
// время с поправкой на часовой пояс браузера, поэтому ему передаётся UTC (иначе время задваивает пояс)
static uint32_t uiManualTimeValue()
{
  return timeSynched ? (uint32_t)getCurrentUtcTime() : 0U;
}

void settingsBuild(sets::Builder& b)
{
  // --- ЛАМПА ---------------------------------
  {
    sets::Group g(b, "Лампа");

    {
      sets::Buttons btns(b);                                // переключение по кругу, как двойной и тройной клик кнопкой
      if (b.Button(UI_ID_EFF_PREV, "Предыдущий"))
      {
        lampSetEffect((currentMode + MODE_AMOUNT - 1U) % MODE_AMOUNT);
        b.reload();                                         // список и ползунки должны подтянуть новый эффект
      }
      if (b.Button(UI_ID_EFF_NEXT, "Следующий"))
      {
        lampSetEffect((currentMode + 1U) % MODE_AMOUNT);
        b.reload();
      }
    }

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

    String speedLabel = effectSpeedLabel(currentMode);      // подписи ползунков - из реестра эффектов; пустая - эффект ползунок не использует
    if (speedLabel.length())
    {
      uint8_t speed = modes[currentMode].Speed;
      if (b.Slider(UI_ID_SPEED, speedLabel, 1, 255, 1, "", &speed))
      {
        lampSetSpeed(speed);
      }
    }

    String scaleLabel = effectScaleLabel(currentMode);
    if (scaleLabel.length())
    {
      uint8_t scale = modes[currentMode].Scale;
      if (b.Slider(UI_ID_SCALE, scaleLabel, 1, 100, 1, "", &scale))
      {
        lampSetScale(scale);
      }
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

  // --- ОБРАТНЫЙ ОТСЧЁТ ----------------------
  {
    sets::Menu page(b, "Обратный отсчёт");

    uint16_t interval = (uint16_t)db[kk::cd_seconds];      // в базе интервал хранится секундами, на странице - минутами и секундами в одной строке
    uint16_t intervalMin = interval / 60U;
    uint16_t intervalSec = interval % 60U;
    {
      sets::Row row(b);
      bool changed = b.Spinner(UI_ID_CD_MIN, "Минуты", 0, 99, 1, &intervalMin);
      changed |= b.Spinner(UI_ID_CD_SEC, "Секунды", 0, 59, 1, &intervalSec);
      if (changed)
      {
        interval = constrain(intervalMin, 0U, 99U) * 60U + constrain(intervalSec, 0U, 59U);
        db.set(kk::cd_seconds, (uint16_t)max(interval, (uint16_t)1U)); // нулевой интервал отсчитывается как одна секунда
      }
    }
    b.Label(UI_ID_CD_LEFT, "Осталось", uiCountdownText()); // обновляется на открытой странице, см. settingsSyncTick
    b.Slider(kk::cd_bri, "Яркость", 1, 255, 1);
    b.Color(kk::cd_color, "Цвет");
    b.Slider(kk::cd_rot, "Поворот", 0, WIDTH - 1, 1);

    {
      sets::Buttons btns(b);
      if (b.Button(UI_ID_CD_START, "Старт"))
      {
        countdownStart();
      }
      if (b.Button(UI_ID_CD_PAUSE, "Пауза"))
      {
        countdownPause();
      }
      if (b.Button(UI_ID_CD_STOP, "Стоп"))
      {
        countdownStop();
      }
    }
  }

  // --- КУБИКИ -------------------------------
  {
    sets::Menu page(b, "Кубики");

    for (uint8_t row = 0U; row < 4U; row++)                 // восемь кубиков в два столбца
    {
      sets::Buttons btns(b);
      for (uint8_t col = 0U; col < 2U; col++)
      {
        uint8_t type = row * 2U + col;
        String name = F("1d");
        name += diceSidesOf(type);
        if (b.Button(UI_ID_DICE(type), name))
        {
          diceRoll(type);
        }
      }
    }

    b.Label(UI_ID_DICE_RESULT, "Результат", diceText());   // обновляется на открытой странице, см. settingsSyncTick
    b.Slider(kk::dice_bri, "Яркость", 1, 255, 1);
    b.Slider(kk::dice_speed, "Скорость анимации", 1, 255, 1);
    b.Color(kk::dice_color, "Цвет");
    b.Slider(kk::dice_rot, "Поворот", 0, WIDTH - 1, 1);
    b.Slider(kk::dice_hold, "Показ результата, с (0 - до возврата)", 0, 120, 1);
    b.Switch(kk::dice_mirror, "Дублировать на обратной стороне");
    b.Switch(kk::dice_click, "Повторный бросок кнопкой лампы");
    if (b.Button(UI_ID_DICE_EXIT, "Вернуться к эффекту"))
    {
      diceExit();
    }
  }

  // --- БЕГУЩАЯ СТРОКА ------------------------
  {
    sets::Group g(b);                                       // одно поле, заголовок группы не нужен

    String text = TextTicker;
    if (b.Input(UI_ID_TEXT, "Бегущая строка", &text))
    {
      lampSetRunningText(text.c_str());
    }
  }

  // --- ТАЙМЕР ВЫКЛЮЧЕНИЯ ---------------------
  {
    sets::Group g(b, "Таймер выключения");

    b.Label(UI_ID_TIMER_STATE, "Состояние", uiTimerText()); // обновляется на открытой странице, см. settingsSyncTick

    b.Spinner(UI_ID_TIMER_MIN, "Минут", 1, 255, 1, &uiSleepMinutes);

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

  // --- НАСТРОЙКИ -----------------------------
  {
    sets::Menu page(b, "Настройки");                        // отдельная страница: сведения о лампе, время, сбросы, разделы настроек и Журнал

    b.Label("Прошивка", FIRMWARE_TITLE);                      // см. Version.h

    b.Label("IP адрес", WiFiConnector.connected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
    b.LabelNum("Свободная память, байт", ESP.getFreeHeap());

    char timeBuf[9];
    getFormattedTime(timeBuf);
    b.Label(UI_ID_LAMP_TIME, "Время лампы", timeBuf);      // поля времени обновляются на открытой странице, см. settingsSyncTick
    b.Label(UI_ID_SYNC_STATE, "Синхронизация времени", clockSyncState());

    uint8_t tzIndex = timezoneIndex(db[kk::tz_offset].toInt());
    if (b.Select(UI_ID_TZ_OFFSET, "Часовой пояс", timezoneList(), &tzIndex))
    {
      lampSetTimezone(timezoneOffset(tzIndex), db[kk::tz_dst]);
      uiTimeRefresh = true;
    }
    uint8_t tzDst = db[kk::tz_dst];
    if (b.Select(UI_ID_TZ_DST, "Переход на летнее время", "нет;Европа;США и Канада", &tzDst))
    {
      lampSetTimezone(db[kk::tz_offset].toInt(), tzDst);
      uiTimeRefresh = true;
    }
    b.Input(kk::ntp_host, "NTP сервер");
    if (b.Button(UI_ID_NTP_SYNC, "Синхронизировать время"))
    {
      clockForceSync();                                     // применяет и новый адрес сервера; ответ приходит в фоне, результат - в Журнале
    }

    uint32_t unixTime = uiManualTimeValue();
    if (b.DateTime(UI_ID_SET_TIME, "Установить время вручную", &unixTime))
    {
      if (unixTime > 0)
      {
        lampSetManualTime(unixTime);
      }
    }

    {
      sets::Buttons btns(b);
      if (b.Button(UI_ID_FX_RESET, "Сброс эффектов"))
      {
        uiConfirmPending = UI_ID_FX_RESET_OK;               // окно подтверждения открывается из settingsTick
      }
      if (b.Button(UI_ID_WIFI_RESET, "Сброс WiFi"))
      {
        uiConfirmPending = UI_ID_WIFI_RESET_OK;
      }
      if (b.Button(UI_ID_REBOOT, "Перезагрузка"))
      {
        pendingRestart = true;
      }
    }

    bool confirmed = false;
    if (b.Confirm(UI_ID_FX_RESET_OK, "Вернуть настройки всех эффектов к значениям по умолчанию?", &confirmed) && confirmed)
    {
      restoreSettings();
      updateSets();
      b.reload();                                           // ползунки должны подтянуть новые значения
    }
    if (b.Confirm(UI_ID_WIFI_RESET_OK, "Забыть сеть роутера и вернуть имя и пароль точки доступа к начальным?", &confirmed) && confirmed)
    {
      pendingWifiReset = true;
    }

    // --- СЕТЬ ----------------------------------
    {
      sets::Menu page(b, "Сеть");                             // WiFi, точка доступа, Wake-on-LAN

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

        bool showIp = (bool)db[kk::run_text_ip];               // бегущая строка показывает адрес лампы; её текст при этом не затирается
        if (b.Switch(UI_ID_TEXT_IP, "Бегущая строка показывает IP", &showIp))
        {
          lampSetRunningTextShowIp(showIp);
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

    // --- MQTT ----------------------------------
    #if (USE_MQTT)
    {
      sets::Menu page(b, "MQTT");                           // брокер, топики
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

    {
      sets::Menu m(b, "Оборудование");                      // задаётся один раз после прошивки, применяется сразу
      if (b.Select(kk::hw_matrix_conn, "Начало ленты",
                   F("левый нижний угол, вправо;левый нижний угол, вверх;левый верхний угол, вправо;левый верхний угол, вниз;"
                     "правый верхний угол, влево;правый верхний угол, вниз;правый нижний угол, влево;правый нижний угол, вверх")))
      {
        hwApply();
      }
      if (b.Select(kk::hw_matrix_parallel, "Ряды ленты", "зигзагом;параллельно"))
      {
        hwApply();
      }
      if (b.Select(kk::hw_color_order, "Порядок цветов", "RGB;RBG;GRB;GBR;BRG;BGR"))
      {
        hwApply();
      }
      if (b.Spinner(kk::hw_current_limit, "Лимит тока, мА (0 - без лимита)", 0, 10000, 100))
      {
        hwApply();
      }
      b.Switch(kk::hw_power_restore, "Включаться после подачи питания");
    }

    #ifdef ESP_USE_BUTTON
    {
      sets::Menu m(b, "Кнопка");                            // действия жестов применяются сразу, кнопка читает их при каждом жесте
      if (b.Select(kk::hw_button, "Кнопка", "нет;сенсорная;механическая"))
      {
        buttonApply();
      }
      bool enabled = buttonEnabled;
      if (b.Switch(UI_ID_BTN_ENABLED, "Кнопка разблокирована", &enabled))
      {
        lampSetButtonEnabled(enabled);
      }
      b.Switch(kk::btn_fav_only, "Листать только эффекты Цикла");

      for (uint8_t lampOff = 0U; lampOff < 2U; lampOff++)
      {
        sets::Group g(b, lampOff ? "Клики на выключенной лампе" : "Клики на включённой лампе");
        for (uint8_t i = 0U; i < 7U; i++)
        {
          b.Select(buttonClickKeys[lampOff][i], uiClicksLabel(i + 1U, false), FPSTR(uiButtonClickActions));
        }
      }

      {
        sets::Group g(b, "Удержание");
        for (uint8_t i = 0U; i < 8U; i++)
        {
          // в списке удержания действия идут подряд, а в настройке у действий только для удержания свои номера (Types.h)
          uint8_t action = db[buttonHoldKeys[i]];
          uint8_t index = action >= BTN_HOLD_ONLY ? action - BTN_HOLD_ONLY + BTN_CLICK_END : action;
          if (b.Select(UI_ID_BTN_HOLD(i), uiClicksLabel(i, true), FPSTR(uiButtonHoldActions), &index))
          {
            db.set(buttonHoldKeys[i], (uint8_t)(index >= BTN_CLICK_END ? index - BTN_CLICK_END + BTN_HOLD_ONLY : index));
          }
        }
      }
    }
    #endif

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

  // защита от лавины: одна и та же стадия пишется не чаще раза в 10 секунд, число пропущенных повторов
  // добавляется в её следующую запись. Запись в Журнал вызывает отправку в браузер, медленная отправка
  // даёт новую запись, и при зависшем клиенте такие строки за несколько секунд вытесняли бы из журнала всё остальное
  static const uint32_t REPEAT_MS = 10000UL;
  static const char* lastStage = nullptr;
  static uint32_t lastLogMs = 0U;
  static uint16_t suppressed = 0U;
  uint32_t now = millis();
  if (stage == lastStage && now - lastLogMs < REPEAT_MS)
  {
    suppressed++;
    return;
  }
  uint16_t repeats = (stage == lastStage) ? suppressed : 0U;
  lastStage = stage;
  lastLogMs = now;
  suppressed = 0U;

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
  if (repeats)
  {
    uiLog.printf_P(PSTR(", повторов с прошлой записи: %u"), repeats);
  }
  uiLog.println();
}

// отключения клиентов вебсокета по сбою: зависший клиент оборван или не отвечал на пинг.
// По адресу видно, чьё устройство держало соединение, по числу активных - остались ли другие открытые страницы
static void uiProfileEvent(const char* event, uint8_t num, uint32_t ip, uint8_t active)
{
  uiLog.printf_P(PSTR("Веб: клиент %u (%s) %s (активных клиентов %u)"), num, IPAddress(ip).toString().c_str(), event, active);
  uiLog.println();
}
#endif //UI_PROFILE_MS

void settingsSetup()
{
  #ifdef UI_PROFILE_MS
  sets::onProfile(uiProfileStage);                          // до sett.begin(), чтобы попал и запуск сервера
  sets::onProfileEvent(uiProfileEvent);
  #endif

  uiSleepMinutes = button_sleep_time;                       // последнее использованное время таймера - в поле веб-интерфейса

  sett.begin(true, hostName().c_str());                     // запускается после WiFiConnector.connect, иначе не подхватится captive DNS.
                                                            // второй аргумент - имя для mDNS, по нему лампа отвечает на <имя>.local;
                                                            // вызывать можно до подключения к роутеру: MDNS.begin ставит колбэк
                                                            // lwIP и перезапускает ответчик, когда интерфейс поднимается
  sett.onBuild(settingsBuild);
  sett.setCustom(uiCustomJs, sizeof(uiCustomJs) - 1);      // браузер скачивает custom.js один раз и перезагружает страницу, дальше берёт его из localStorage
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

  if (uiConfirmPending)
  {
    sett.updater().confirm(uiConfirmPending);
    uiConfirmPending = 0U;
  }
  if (clockNotice)                                          // результат синхронизации по кнопке - всплывающим сообщением
  {
    sett.updater().notice(clockNotice);
    clockNotice = nullptr;
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

  // новые записи журнала отправляются в открытую страницу не чаще раза в 3 секунды: журнал уходит целиком
  // (1.2 КБ) при каждой новой строке, и при медленном клиенте частая отправка вызывает подвисания.
  // Флаг изменений сбрасывает только отправка, поэтому отложенные записи не теряются
  static uint32_t lastLogPush = 0U;
  if (millis() - lastLogPush >= 3000U && uiLog._changed())
  {
    lastLogPush = millis();
    sett.updater().update(UI_ID_LOG, static_cast<sets::Logger&>(uiLog)); // приведение к базовому типу, иначе побеждает шаблонная перегрузка update(id, T) по значению
  }

  // поле "Осталось" обратного отсчёта: оставшееся время, пауза или выбранный интервал
  static String lastCountdownText;
  String countdownLeft = uiCountdownText();
  if (countdownLeft != lastCountdownText)
  {
    lastCountdownText = countdownLeft;
    sett.updater().update(UI_ID_CD_LEFT, countdownLeft);
  }

  static String lastTimerText;                              // таймер выключения: запуск, отключение, оставшиеся минуты
  String timerText = uiTimerText();
  if (timerText != lastTimerText)
  {
    lastTimerText = timerText;
    sett.updater().update(UI_ID_TIMER_STATE, timerText);
  }

  static String lastDiceText;                               // поле "Результат" кубиков: бросок идёт или его итог
  String diceResult = diceText();
  if (diceResult != lastDiceText)
  {
    lastDiceText = diceResult;
    sett.updater().update(UI_ID_DICE_RESULT, diceResult);
  }

  // поля времени в «Настройках». Часы обновляются каждую секунду, статус синхронизации и поле ручной установки -
  // после каждой установки часов (NTP или вручную) и смены часового пояса. Поле ручной установки каждую
  // секунду не обновляется, чтобы не менять значение, пока его вводят
  char timeBuf[9];
  getFormattedTime(timeBuf);
  static uint16_t lastClockSetCount = 0U;
  if (clockSetCount != lastClockSetCount || uiTimeRefresh)
  {
    lastClockSetCount = clockSetCount;
    uiTimeRefresh = false;
    sett.updater()
        .update(UI_ID_LAMP_TIME, timeBuf)
        .update(UI_ID_SYNC_STATE, clockSyncState())
        .update(UI_ID_SET_TIME, uiManualTimeValue());
  }
  else
  {
    sett.updater().update(UI_ID_LAMP_TIME, timeBuf);
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
