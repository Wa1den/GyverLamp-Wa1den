// LampControl — единый слой команд управления лампой.
//
// Все каналы управления (веб-интерфейс Settings, MQTT, кнопка) вызывают эти функции,
// а не меняют состояние лампы напрямую — одна точка правды вместо трёх независимых
// путей, как было со старым текстовым парсером команд (parsing.ino).
//
// Функции работают с существующими глобалами прошивки: ONflag, currentMode, modes[],
// loadingFlag, settChanged/eepromTimeout (отложенное сохранение), dawnFlag/manualOff
// (рассвет) — и выставляют флаг публикации состояния в MQTT.

// пометить настройки изменёнными: перезапустить эффект, взвести отложенное сохранение, запросить публикацию состояния в MQTT
// (перенесено из parsing.ino без изменений)
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

// включить/выключить лампу; во время работающего "рассвета" любая команда питания гасит рассвет (как P_ON/P_OFF раньше)
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
  if (effectId >= MODE_AMOUNT)                              // старый парсер не проверял диапазон и уходил в "чёрный" несуществующий режим
  {
    effectId = MODE_AMOUNT - 1U;
  }

  Storage::SaveModesSettings(&currentMode, modes);          // сохранение настроек эффектов перед переключением (как в старом обработчике EFF)
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
  // без loadingFlag: перезапуск эффекта при изменении яркости не нужен (как в старом обработчике BRI)
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

// разблокировать/заблокировать кнопку на лампе (как BTN ON/OFF раньше)
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
  button_sleep_time = constrain(minutes, 1, 255);           // запоминаем последнее время для быстрого взвода двойным кликом кнопки (как TMR_SET раньше)
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

  strncpy(TextTicker, text, MAX_UDP_BUFFER_SIZE);
  TextTicker[MAX_UDP_BUFFER_SIZE] = '\0';
  db.set(kk::running_text, TextTicker);

  if (currentMode == EFF_TEXT)                              // если бегущая строка сейчас на экране - перезапустить эффект с новым текстом
  {
    loadingFlag = true;
  }
  mqttRequestPublish();
}

#ifdef USE_MANUAL_TIME_SETTING
// ручная установка времени лампы (unix-время по UTC, например из виджета даты/времени веб-интерфейса)
void lampSetManualTime(uint32_t utcUnixTime)
{
  #ifdef USE_NTP
  manualTimeShift = localTimeZone.toLocal((time_t)utcUnixTime) - millis() / 1000UL;
  #else
  manualTimeShift = (time_t)utcUnixTime + LOCAL_OFFSET * 60UL - millis() / 1000UL;
  #endif

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

// отложенные действия, запрошенные из веб-интерфейса (нельзя выполнять из контекста асинхронного вебсервера)
void handlePendingActions()
{
  if (pendingWifiReset)
  {
    pendingWifiReset = false;
    resetWifiSettings();
    LOG.println(F("Настройки WiFi сброшены (запрос из веб-интерфейса)"));
  }

  if (pendingRestart)
  {
    pendingRestart = false;
    LOG.println(F("Перезагрузка (запрос из веб-интерфейса)..."));
    db.update();                                            // запись несохранённых настроек перед перезагрузкой
    delay(100);
    ESP.restart();
  }
}
