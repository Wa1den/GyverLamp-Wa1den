// Кадры эффектов: вызов эффекта, выбранного в реестре (EffectsRegistry.h), с периодом кадра из реестра.

uint32_t effTimer;

void effectsTick()
{
  if (!dawnFlag)
  {
    if (overlayTick())                                      // мигание, служебная строка, обратный отсчёт или кубики вместо эффекта
    {
      return;
    }

    if (ONflag && millis() - effTimer >= effectFramePeriod(currentMode, modes[currentMode].Speed))
    {
      effTimer = millis();
      bool frameRedrawn = loadingFlag;                      // запоминаем до вызова эффекта: эффект сбрасывает loadingFlag после перерисовки
      switch (currentMode)
      {
        EFFECT_LIST(EFFECT_CASE)
      }
      #ifdef WARNING_IF_NO_TIME_ON_EFFECTS_TOO
        if (!timeSynched)
          noTimeWarning();
      #endif

      // Белый свет и Цвет - статичные картинки: лента обновляется при изменениях
      // (перерисовка или смена яркости) и раз в секунду для самолечения от возможных
      // помех, а не 20 раз в секунду впустую, как остальные (динамичные) эффекты.
      // Кадры с компьютера выводит сам эффект по приходу кадра.
      static uint8_t lastShownBrightness = 0U;
      static uint32_t lastShowTime = 0U;
      if ((currentMode != EFF_WHITE_COLOR && currentMode != EFF_COLOR && currentMode != EFF_DDP) || frameRedrawn ||
          lastShownBrightness != FastLED.getBrightness() || millis() - lastShowTime >= 1000U)
      {
        lastShownBrightness = FastLED.getBrightness();
        lastShowTime = millis();
        ledsShow();
      }
    }
    #ifdef WARNING_IF_NO_TIME
    else if (!timeSynched && !ONflag && !((uint8_t)millis())){
      noTimeWarningShow();
    }
    #endif
  }
}

void changePower()
{
  if (ONflag)
  {
    effectsTick();
    for (uint8_t i = 0U; i < modes[currentMode].Brightness; i = constrain(i + 8, 0, modes[currentMode].Brightness))
    {
      FastLED.setBrightness(i);
      delay(1);
      ledsShow();
    }
    FastLED.setBrightness(modes[currentMode].Brightness);
    delay(2);
    ledsShow();
  }
  else
  {
    effectsTick();
    for (uint8_t i = FastLED.getBrightness(); i > 0; i = i > 8U ? i - 8U : 0U) // с текущей яркости: после картинки она не яркость эффекта
    {
      FastLED.setBrightness(i);
      delay(1);
      ledsShow();
    }
    ledsClear();
    delay(2);
    ledsShow();
  }

  #if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)          // установка сигнала в пин, управляющий MOSFET транзистором, соответственно состоянию вкл/выкл матрицы
  digitalWrite(MOSFET_PIN, ONflag ? MOSFET_LEVEL : !MOSFET_LEVEL);
  #endif

  db.set(kk::lamp_on, ONflag);                              // немедленное сохранение вкл/выкл (файл запишется тикером БД в течение 10 сек) - для восстановления состояния после OTA

  TimerManager::TimerRunning = false;
  TimerManager::TimerHasFired = false;
  TimerManager::TimeToFire = 0U;
  #ifdef AUTOMATIC_OFF_TIME      
    if (ONflag){
      TimerManager::TimerRunning = true;
      TimerManager::TimeToFire = millis() + AUTOMATIC_OFF_TIME;
    }
  #endif    
  
  if (FavoritesManager::UseSavedFavoritesRunning == 0U)     // если выбрана опция Сохранять состояние (вкл/выкл) "избранного", то ни выключение модуля, ни выключение матрицы не сбрасывают текущее состояние (вкл/выкл) "избранного"
  {
      FavoritesManager::TurnFavoritesOff();
  }

  #if (USE_MQTT)
  if (espMode == 1U)
  {
    MqttManager::needToPublish = true;
  }
  #endif
}

#ifdef WARNING_IF_NO_TIME
void noTimeWarning(){
  for (uint8_t i = 0; i < WIDTH; i++) leds[XY(i, 0U)] = CRGB::Black;
  uint8_t z = millis() / 1000U;
  leds[XY(z % WIDTH , 0U)] = espMode ? CRGB::Red : CRGB::Blue; // время не получено: красным в режиме клиента WiFi, синим в режиме точки доступа
  leds[XY((z + WIDTH / 2U) % WIDTH , 0U)] = espMode ? CRGB::Red : CRGB::Blue;
}
void noTimeWarningShow(){
  noTimeWarning();
  FastLED.setBrightness(WARNING_IF_NO_TIME);
  ledsShow();
}
void noTimeClear(){
  if (!timeSynched){ 
    for (uint8_t i = 0; i < WIDTH; i++) 
       leds[XY(i, 0U)] = CRGB::Black; 
    ledsShow();
  }
}
#endif //WARNING_IF_NO_TIME
