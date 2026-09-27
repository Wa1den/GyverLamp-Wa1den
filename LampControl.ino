// LampControl — единый слой команд управления лампой.
//
// Все каналы управления (веб-интерфейс Settings, MQTT, кнопка) вызывают эти функции,
// а не меняют состояние лампы напрямую — одна точка правды для всех каналов.
//
// Функции работают с глобалами прошивки: ONflag, currentMode, modes[],
// loadingFlag, settChanged/eepromTimeout (отложенное сохранение), dawnFlag/manualOff
// (рассвет) — и выставляют флаг публикации состояния в MQTT.

// пометить настройки изменёнными: перезапустить эффект, взвести отложенное сохранение, запросить публикацию состояния в MQTT
void updateSets()
{
      loadingFlag = true;
      settChanged = true;
      eepromTimeout = millis();

      #if (USE_MQTT)
      if (espMode == 1U)
      {
        MqttManager::needToPublish = true;
      }
      #endif
}

// включить/выключить лампу; во время работающего "рассвета" любая команда питания гасит рассвет
void lampSetPower(bool on)
{
  if (dawnFlag)
  {
    manualOff = true;
    dawnFlag = false;
    FastLED.setBrightness(modes[currentMode].Brightness);
    changePower();
    return;
  }

  if (ONflag == on)                                         // состояние не меняется - не перезапускать эффект и не мигать яркостью
  {
    return;
  }

  ONflag = on;
  updateSets();
  changePower();
}

// установить текущий эффект (0..MODE_AMOUNT-1)
void lampSetEffect(uint8_t effectId)
{
  if (effectId >= MODE_AMOUNT)                              // защита от несуществующего номера эффекта
  {
    effectId = MODE_AMOUNT - 1U;
  }

  Storage::SaveModesSettings(&currentMode, modes);          // сохранение настроек эффектов перед переключением
  currentMode = effectId;
  updateSets();

  #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
  if (random_on && FavoritesManager::FavoritesRunning)
  {
    selectedSettings = 1U;
  }
  #endif //RANDOM_SETTINGS_IN_CYCLE_MODE

  FastLED.setBrightness(modes[currentMode].Brightness);
}

// установить яркость текущего эффекта (1..255)
void lampSetBrightness(uint8_t value)
{
  modes[currentMode].Brightness = constrain(value, 1, 255);
  FastLED.setBrightness(modes[currentMode].Brightness);
  // без loadingFlag: перезапуск эффекта при изменении яркости не нужен
  settChanged = true;
  eepromTimeout = millis();

  #if (USE_MQTT)
  if (espMode == 1U)
  {
    MqttManager::needToPublish = true;
  }
  #endif
}

// установить скорость текущего эффекта (1..255)
void lampSetSpeed(uint8_t value)
{
  modes[currentMode].Speed = value;
  updateSets();
}

// установить масштаб текущего эффекта (1..100 у большинства эффектов)
void lampSetScale(uint8_t value)
{
  modes[currentMode].Scale = value;
  updateSets();
}

// включить/выключить режим Цикл (автоматическая смена избранных эффектов)
void lampSetFavoritesRunning(bool on)
{
  FavoritesManager::FavoritesRunning = on ? 1U : 0U;
  FavoritesManager::nextModeAt = 0UL;                       // сброс времени следующего переключения (переключение начнётся заново)
  updateSets();
}

// добавить/убрать эффект из списка режима Цикл
void lampSetFavoriteMode(uint8_t effectId, bool enabled)
{
  if (effectId >= MODE_AMOUNT)
  {
    return;
  }

  FavoritesManager::FavoriteModes[effectId] = enabled ? 1U : 0U;
  #ifdef USE_SHUFFLE_FAVORITES
  shuffleCurrentIndex = MODE_AMOUNT;                        // список изменился - очередь показа перемешается заново
  #endif
  updateSets();
}

// запросить публикацию состояния лампы в MQTT (для изменений, не затрагивающих настройки эффектов)
void mqttRequestPublish()
{
  #if (USE_MQTT)
  if (espMode == 1U)
  {
    MqttManager::needToPublish = true;
  }
  #endif
}

// разблокировать/заблокировать кнопку на лампе
void lampSetButtonEnabled(bool enabled)
{
  buttonEnabled = enabled;
  Storage::SaveButtonEnabled(&buttonEnabled);
  mqttRequestPublish();
}

// завести/выключить будильник дня недели (day: 0 - понедельник .. 6 - воскресенье; minutes - время от начала суток)
void lampSetAlarm(uint8_t day, bool state, uint16_t minutes)
{
  if (day >= 7U)
  {
    return;
  }

  alarms[day].State = state;
  alarms[day].Time = min(minutes, (uint16_t)1439U);
  Storage::SaveAlarmsSettings(&day, alarms);
  mqttRequestPublish();
}

// установить опцию "рассвет за ... минут" (индекс в списке dawnOffsets: 0 - 5 минут .. 8 - 60 минут)
void lampSetDawnMode(uint8_t mode)
{
  dawnMode = constrain(mode, 0, 8);
  Storage::SaveDawnMode(&dawnMode);
  mqttRequestPublish();
}

// взвести таймер выключения лампы через указанное количество минут (0 - отключить таймер)
void lampSetSleepTimer(uint16_t minutes)
{
  if (minutes == 0U)
  {
    lampClearSleepTimer();
    return;
  }

  #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
  button_sleep_time = constrain(minutes, 1, 255);           // запоминаем последнее время для быстрого взвода двойным кликом кнопки
  Storage::Save_button_sleep_time(&button_sleep_time);
  #endif //#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)

  TimerManager::TimeToFire = millis() + minutes * 60UL * 1000UL;
  TimerManager::TimerRunning = true;
  TimerManager::TimerHasFired = false;
  mqttRequestPublish();
}

// отключить таймер выключения
void lampClearSleepTimer()
{
  TimerManager::TimerRunning = false;
  TimerManager::TimeToFire = 0ULL;
  mqttRequestPublish();
}

// установить текст эффекта Бегущая строка (с сохранением в хранилище настроек)
void lampSetRunningText(const char* text)
{
  if (text == NULL || strlen(text) == 0)
  {
    return;
  }

  strncpy(TextTicker, text, CMD_BUFFER_SIZE);
  TextTicker[CMD_BUFFER_SIZE] = '\0';
  db.set(kk::running_text, (const char*)TextTicker);       // без приведения массив char[] попадает в шаблонный конструктор AnyType как двоичные данные,
                                                            // и в строковую ячейку записывается пустая строка

  if (currentMode == EFF_TEXT)                              // если бегущая строка сейчас на экране - перезапустить эффект с новым текстом
  {
    loadingFlag = true;
  }
  mqttRequestPublish();
}

// вкл/выкл режима "Писать текущий IP" для эффекта Бегущая строка.
// Поле "Текст" при этом не трогается: выключил галку - вернулся заданный текст.
void lampSetRunningTextShowIp(bool showIp)
{
  db.set(kk::run_text_ip, showIp);

  if (currentMode == EFF_TEXT)                              // бегущая строка на экране - перезапустить с новым источником текста
  {
    loadingFlag = true;
  }
  mqttRequestPublish();
}

#ifdef USE_MANUAL_TIME_SETTING
// ручная установка времени лампы (unix-время по UTC, например из виджета даты/времени веб-интерфейса)
void lampSetManualTime(uint32_t utcUnixTime)
{
  manualTimeShift = localTimeZone.toLocal((time_t)utcUnixTime) - millis() / 1000UL;

  #ifdef GET_TIME_FROM_PHONE
  phoneTimeLastSync = manualTimeShift + millis() / 1000UL;
  #endif
  #ifdef WARNING_IF_NO_TIME
  noTimeClear();
  #endif
  timeSynched = true;
  #if defined(PHONE_N_MANUAL_TIME_PRIORITY) && defined(USE_NTP)
  stillUseNTP = false;
  #endif
}
#endif //USE_MANUAL_TIME_SETTING

#if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
// часовые пояса для выбора в веб-интерфейсе: смещение от UTC в минутах
static const int16_t timezoneOffsets[] PROGMEM = {
  -720, -660, -600, -570, -540, -480, -420, -360, -300, -240, -210, -180, -120, -60,
  0, 60, 120, 180, 210, 240, 270, 300, 330, 345, 360, 390, 420, 480, 525, 540, 570,
  600, 630, 660, 720, 765, 780, 840
};
#define TIMEZONE_COUNT        (sizeof(timezoneOffsets) / sizeof(timezoneOffsets[0]))

// список поясов строкой для виджета выбора: "UTC-12;...;UTC+5:30;..."
String timezoneList()
{
  String list;
  for (uint8_t i = 0U; i < TIMEZONE_COUNT; i++)
  {
    int16_t offset = (int16_t)pgm_read_word(&timezoneOffsets[i]);
    uint16_t absOffset = abs(offset);
    char item[12];
    if (absOffset % 60U)
    {
      sprintf_P(item, PSTR("UTC%c%u:%02u"), offset < 0 ? '-' : '+', absOffset / 60U, absOffset % 60U);
    }
    else
    {
      sprintf_P(item, PSTR("UTC%c%u"), offset < 0 ? '-' : '+', absOffset / 60U);
    }
    if (i)
    {
      list += ';';
    }
    list += item;
  }
  return list;
}

// номер пояса в списке по смещению; смещение не из списка (файл настроек испорчен) даёт UTC+0
uint8_t timezoneIndex(int16_t offset)
{
  for (uint8_t i = 0U; i < TIMEZONE_COUNT; i++)
  {
    if ((int16_t)pgm_read_word(&timezoneOffsets[i]) == offset)
    {
      return i;
    }
  }
  return timezoneIndex(0);
}

int16_t timezoneOffset(uint8_t index)
{
  return (int16_t)pgm_read_word(&timezoneOffsets[index < TIMEZONE_COUNT ? index : timezoneIndex(0)]);
}

// правило перехода для Timezone: воскресенье заданной недели месяца, час - по местному времени до перехода
static TimeChangeRule timezoneRule(uint8_t week, uint8_t month, int16_t hour, int16_t offset)
{
  TimeChangeRule rule;
  rule.abbrev[0] = '\0';
  rule.week = week;
  rule.dow = dow_t::Sun;
  rule.month = month;
  rule.hour = (uint8_t)constrain(hour, 0, 23);
  rule.offset = offset;
  return rule;
}

// правила часового пояса из настроек - в объект localTimeZone, через который идут все пересчёты UTC и местного времени
void timezoneApply()
{
  int16_t offset = timezoneOffset(timezoneIndex(db[kk::tz_offset].toInt()));
  TimeChangeRule stdRule = timezoneRule(week_t::Last, month_t::Oct, 1, offset);
  TimeChangeRule dstRule = stdRule;                         // одинаковые правила - без перехода на летнее время

  switch ((uint8_t)db[kk::tz_dst])
  {
    case 1U:                                                // Европа: последние воскресенья марта и октября, в 01:00 UTC
      dstRule = timezoneRule(week_t::Last, month_t::Mar, (60 + offset) / 60, offset + 60);
      stdRule = timezoneRule(week_t::Last, month_t::Oct, (120 + offset) / 60, offset);
      break;
    case 2U:                                                // США и Канада: второе воскресенье марта и первое ноября, в 02:00 местного времени
      dstRule = timezoneRule(week_t::Second, month_t::Mar, 2, offset + 60);
      stdRule = timezoneRule(week_t::First, month_t::Nov, 2, offset);
      break;
  }

  localTimeZone.setRules(dstRule, stdRule);
}

// смена часового пояса из веб-интерфейса. Момент времени сохраняется: резервное время
// (manualTimeShift) хранится местным, поэтому его пересчитывают под новый пояс
void lampSetTimezone(int16_t offset, uint8_t dst)
{
  #if defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
  time_t utcNow = localTimeZone.toUTC(millis() / 1000UL + manualTimeShift);
  #endif

  db.set(kk::tz_offset, offset);
  db.set(kk::tz_dst, dst);
  timezoneApply();

  #if defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
  time_t newShift = localTimeZone.toLocal(utcNow) - millis() / 1000UL;
  #ifdef GET_TIME_FROM_PHONE
  phoneTimeLastSync += newShift - manualTimeShift;
  #endif
  manualTimeShift = newShift;
  #endif
}
#endif //#if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)

#ifdef USE_NTP
// принудительная синхронизация времени с NTP сервером (кнопка в веб-интерфейсе);
// применяет адрес сервера из хранилища настроек, поэтому работает и как "сменить сервер без перезагрузки"
void lampForceNtpSync()
{
  ntpServerName = (String)db[kk::ntp_host];
  if (!ntpServerName.length())
  {
    ntpServerName = NTP_ADDRESS;
  }
  uiLog.printf_P(PSTR("NTP: синхронизация с %s...\n"), ntpServerName.c_str());

  ntpResetRetryInterval();                                  // ручной запрос - сбрасываем нарастающую паузу автоматических попыток
  ntpServerAddressResolved = false;
  resolveNtpServerAddress(ntpServerAddressResolved);        // резолвит имя из настроек и передаёт NTPClient уже IP; диагностика в журнал
  if (!ntpServerAddressResolved)
  {
    uiLog.println(F("NTP: сервер недоступен (ошибка DNS/нет интернета)"));
    return;
  }

  if (timeClient.forceUpdate())
  {
    timeSynched = true;
    #if defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
    manualTimeShift = localTimeZone.toLocal(timeClient.getEpochTime()) - millis() / 1000UL; // резервное время на случай отвалившегося NTP
    #endif
    #ifdef PHONE_N_MANUAL_TIME_PRIORITY
    stillUseNTP = false;
    #endif
    #ifdef WARNING_IF_NO_TIME
    noTimeClear();
    #endif
    char timeBuf[9];
    getFormattedTime(timeBuf);
    uiLog.printf_P(PSTR("NTP: время получено: %s\n"), timeBuf);
  }
  else
  {
    uiLog.println(F("NTP: сервер не ответил"));
  }
}
#endif //USE_NTP

// отложенные действия, запрошенные из веб-интерфейса (нельзя выполнять из контекста асинхронного вебсервера)
void handlePendingActions()
{
  if (pendingWifiReset)
  {
    pendingWifiReset = false;
    resetWifiSettings();
    LOG.println(F("Настройки WiFi сброшены (запрос из веб-интерфейса)"));
    uiLog.println(F("Настройки WiFi сброшены"));
  }

  #ifdef USE_NTP
  if (pendingNtpSync)
  {
    pendingNtpSync = false;
    lampForceNtpSync();
  }
  #endif //USE_NTP

  if (pendingWolWake)
  {
    pendingWolWake = false;
    wolWake(NULL);                                          // пробуждение компьютера по MAC из настроек
  }

  if (pendingShowIp)                                        // подключились к новой сети - показываем IP лампы бегущей строкой
  {
    pendingShowIp = false;
    WiFi.localIP().toString().toCharArray(TextTicker, sizeof(TextTicker)); // прямо в буфер строки, без сохранения в настройки (db running_text не трогаем)
    currentMode = EFF_TEXT;
    ONflag = true;
    loadingFlag = true;
    changePower();
    mqttRequestPublish();
  }

  #if (USE_MQTT)
  if (pendingWolResub)
  {
    pendingWolResub = false;
    MqttManager::applyWolExtSubscription();                 // применение изменённых настроек дополнительного WOL-топика
  }
  #endif //USE_MQTT

  if (pendingRestart)
  {
    pendingRestart = false;
    LOG.println(F("Перезагрузка (запрос из веб-интерфейса)..."));
    db.update();                                            // запись несохранённых настроек перед перезагрузкой
    delay(100);
    ESP.restart();
  }
}
