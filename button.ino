#ifdef ESP_USE_BUTTON

bool brightDirection;
static bool startButtonHolding = false;                     // флаг: кнопка удерживается для изменения яркости/скорости/масштаба лампы кнопкой

#ifdef BUTTON_PAUSE_AFTER_TURN_ON
static bool breakButtonHolding = true;                      // флаг: кнопка была отпущена после включения лампы в режим Белый свет (удерживанием кнопки)
#endif

void buttonTick()
{
  if (!buttonEnabled)                                       // события кнопки не обрабатываются, если она заблокирована
  {
    return;
  }

  touch.tick();

  #ifdef BUTTON_PRESS_FEEDBACK
  if (touch.isPress())                                      // отклик на само касание - мгновенно, ещё до распознавания одиночного/двойного клика
  {
    buttonFeedbackAt = millis();
  }
  #endif //BUTTON_PRESS_FEEDBACK

  uint8_t clickCount = touch.hasClicks() ? touch.getClicks() : 0U;

  if (clickCount && serviceTextActive())                    // клик обрывает бегущую служебную строку (IP, время) и дальше срабатывает как обычно:
  {                                                         // при быстром переключении эффектов клики сливаются в серию из 5-6, и строка запускалась случайно
    serviceTextStop();
  }


  // кубик на лампе: один клик бросает его ещё раз (если включено на странице настроек), два клика возвращают к эффекту
  if (clickCount == 1U && diceActive() && !dawnFlag && (bool)db[kk::dice_click])
  {
    diceRoll((uint8_t)db[kk::dice_last]);
  }
  else if (clickCount == 2U && diceActive() && !dawnFlag)
  {
    diceExit();
  }


  // однократное нажатие
  else if (clickCount == 1U)
  {
    if (dawnFlag)
    {
      manualOff = true;
      dawnFlag = false;
      FastLED.setBrightness(modes[currentMode].Brightness);
      changePower();
    }
    else
    {
      ONflag = !ONflag;
      changePower();
    }
    settChanged = true;
    eepromTimeout = millis();
    loadingFlag = true;

    #if (USE_MQTT)
    if (espMode == 1U)
    {
      MqttManager::needToPublish = true;
    }
    #endif
  }


  // двухкратное нажатие
  else if (ONflag && clickCount == 2U)
  {
    #ifdef BUTTON_CHANGE_FAVORITES_MODES_ONLY
      uint8_t lastMode = currentMode;
      do {
        if (++currentMode >= MODE_AMOUNT) currentMode = 0;
      } while (FavoritesManager::FavoriteModes[currentMode] == 0 && currentMode != lastMode);
      if (currentMode == lastMode) // если ни один режим не добавлен в избранное, всё равно куда-нибудь переключимся
        if (++currentMode >= MODE_AMOUNT) currentMode = 0;
    #else
      if (++currentMode >= MODE_AMOUNT) currentMode = 0;
    #endif
    
    FastLED.setBrightness(modes[currentMode].Brightness);
    loadingFlag = true;
    settChanged = true;
    eepromTimeout = millis();

    #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      if (random_on && FavoritesManager::FavoritesRunning)
        selectedSettings = 1U;
    #endif //RANDOM_SETTINGS_IN_CYCLE_MODE

    #if (USE_MQTT)
    if (espMode == 1U)
    {
      MqttManager::needToPublish = true;
    }
    #endif
  }

  // двухкратное нажатие на выключенной лампе
  #ifdef BUTTON_CAN_SET_SLEEP_TIMER
  else if (clickCount == 2U)
  {
    // мигать об успехе операции лучше до вызова changePower(), иначе сперва мелькнут кадры текущего эффекта
    showWarning(CRGB::Blue, 2000U, 500U);                    // мигание синим цветом 2 секунды

    ONflag = true;
    changePower();
    settChanged = true;
    eepromTimeout = millis();

    TimerManager::TimeToFire = millis() + button_sleep_time * 60UL * 1000UL;
    TimerManager::TimerRunning = true;
  }
  #endif //BUTTON_CAN_SET_SLEEP_TIMER

  // трёхкратное нажатие
  else if (ONflag && clickCount == 3U)
  {
    #ifdef BUTTON_CHANGE_FAVORITES_MODES_ONLY
      uint8_t lastMode = currentMode;
      do {
        if (--currentMode >= MODE_AMOUNT) currentMode = MODE_AMOUNT - 1;
      } while (FavoritesManager::FavoriteModes[currentMode] == 0 && currentMode != lastMode);
      if (currentMode == lastMode) // если ни один режим не добавлен в избранное, всё равно куда-нибудь переключимся
        if (--currentMode >= MODE_AMOUNT) currentMode = MODE_AMOUNT - 1;
    #else
      if (--currentMode >= MODE_AMOUNT) currentMode = MODE_AMOUNT - 1;
    #endif
    
    FastLED.setBrightness(modes[currentMode].Brightness);
    loadingFlag = true;
    settChanged = true;
    eepromTimeout = millis();

    #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      if (random_on && FavoritesManager::FavoritesRunning)
        selectedSettings = 1U;
    #endif //RANDOM_SETTINGS_IN_CYCLE_MODE

    #if (USE_MQTT)
    if (espMode == 1U)
    {
      MqttManager::needToPublish = true;
    }
    #endif
  }


  // пятикратное нажатие
  else if (clickCount == 5U)                                     // вывод IP на лампу
  {
    if (espMode == 1U)
    {
      serviceTextStart(WiFi.localIP().toString().c_str(), CRGB::White, modes[currentMode].Brightness);
    }
  }


  // шестикратное нажатие
  else if (clickCount == 6U)                                     // вывод текущего времени бегущей строкой
  {
    printTime(thisTime, true, ONflag);
  }


  // кнопка только начала удерживаться
  if (touch.isHolded()) // пускай для выключенной лампы удержание кнопки включает белую лампу
  {
    brightDirection = !brightDirection;
    startButtonHolding = true;

    // служебные жесты подтверждаются удержанием после серии: при быстром переключении эффектов
    // клики сливаются в серии по 4-7, но удержания в конце у них нет
    switch (touch.getHoldClicks())
    {
      case 4U:                                              // обновление по воздуху
      {
        #ifdef OTA
        if (otaManager.RequestOtaUpdate())
        {
          ONflag = true;
          currentMode = EFF_MATRIX;                         // эффект Матрица - признак режима обновления
          changePower();
        }
        #endif
        break;
      }

      case 7U:                                              // смена режима WiFi: точка доступа или клиент, с перезагрузкой
      {
        #ifdef RESET_WIFI_ON_ESP_MODE_CHANGE
        if (espMode) resetWifiSettings();                   // сброс сохранённых SSID и пароля роутера
        #endif
        espMode = (espMode == 0U) ? 1U : 0U;
        Storage::SaveEspMode(&espMode);
        showWarning(CRGB::Red, 3000U, 500U);                // мигание красным 3 секунды перед перезагрузкой
        ESP.restart();
        break;
      }
    }
  }


  // кнопка нажата и удерживается
if (touch.isStep() && touch.getHoldClicks() < 3U)            // после 3 и более кликов удержание - подтверждение служебного жеста, а не регулировка
  if (ONflag
      #ifdef BUTTON_PAUSE_AFTER_TURN_ON
      && breakButtonHolding
      #endif
  )
  {
    switch (touch.getHoldClicks())
    {
      case 0U:                                              // просто удержание (до удержания кнопки кликов не было) - изменение яркости
      {
        uint8_t delta = modes[currentMode].Brightness < 10U // определение шага изменения яркости: при яркости [1..10] шаг = 1, при [11..16] шаг = 3, при [17..255] шаг = 15
          ? 1U
          : 5U;
        modes[currentMode].Brightness =
          constrain(brightDirection
            ? modes[currentMode].Brightness + delta
            : modes[currentMode].Brightness - delta,
          1, 255);
        FastLED.setBrightness(modes[currentMode].Brightness);

        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("Новое значение яркости: %d\n"), modes[currentMode].Brightness);
        #endif

        break;
      }

      case 1U:                                              // удержание после одного клика - изменение скорости
      {
        modes[currentMode].Speed = constrain(brightDirection ? modes[currentMode].Speed + 1 : modes[currentMode].Speed - 1, 1, 255);
        loadingFlag = true; // без перезапуска эффекта ничего и не увидишь

        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("Новое значение скорости: %d\n"), modes[currentMode].Speed);
        #endif

        break;
      }

      case 2U:                                              // удержание после двух кликов - изменение масштаба
      {
        modes[currentMode].Scale = constrain(brightDirection ? modes[currentMode].Scale + 1 : modes[currentMode].Scale - 1, 1, 100);
        loadingFlag = true; // без перезапуска эффекта ничего и не увидишь

        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("Новое значение масштаба: %d\n"), modes[currentMode].Scale);
        #endif

        break;
      }

      default:
        break;
    }

    settChanged = true;
    eepromTimeout = millis();
  }
  else
  #ifdef BUTTON_PAUSE_AFTER_TURN_ON
  if (breakButtonHolding)
  #endif
  {
    #ifdef BUTTON_PAUSE_AFTER_TURN_ON
    breakButtonHolding = false;
    #endif
   
    currentMode = EFF_WHITE_COLOR;
    ONflag = true;
    changePower();
    settChanged = true;
    eepromTimeout = millis();
  }

  // кнопка отпущена после удерживания
  if (ONflag && !touch.isHold() && startButtonHolding)      // кнопка отпущена после удерживания, нужно отправить MQTT сообщение об изменении яркости лампы
  {
    #ifdef BUTTON_PAUSE_AFTER_TURN_ON
    breakButtonHolding = true;
    #endif

    startButtonHolding = false;
    loadingFlag = true;

    #if (USE_MQTT)
    if (espMode == 1U)
    {
      MqttManager::needToPublish = true;
    }
    #endif
    
    
  }
}
#endif
