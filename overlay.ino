// Картинка поверх эффекта: обратный отсчёт или кубики. На лампе одна такая картинка, новая сменяет
// прежнюю. После неё лампа возвращается к эффекту, а если была выключена до первой из них - гаснет.
// Выключение лампы или смена эффекта убирают картинку. Служебная строка (IP, время) идёт поверх
// картинки, а мигание-предупреждение - поверх всего, в том числе на выключенной лампе.

static CRGB warnColor;
static uint32_t warnStartAt = 0U;
static uint32_t warnDuration = 0U;
static uint16_t warnHalfPeriod = 500U;
static uint8_t warnPhase = 0xFFU;                           // 0 - темнота, 1 - вспышка; 0xFF - мигания нет

// мигание цветом, не останавливая лампу; первая половина периода - темнота
void warningStart(CRGB color, uint32_t duration, uint16_t blinkHalfPeriod)
{
  warnColor = color;
  warnStartAt = millis();
  warnDuration = duration;
  warnHalfPeriod = blinkHalfPeriod;
  warnPhase = 2U;                                           // первый кадр рисуется сразу

  #if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)
  digitalWrite(MOSFET_PIN, MOSFET_LEVEL);
  #endif
}

static bool warningTick()
{
  if (warnPhase == 0xFFU)
  {
    return false;
  }

  uint32_t elapsed = millis() - warnStartAt;
  if (elapsed > warnDuration)
  {
    warnPhase = 0xFFU;
    FastLED.setBrightness(modes[currentMode].Brightness);
    loadingFlag = true;
    if (!ONflag)
    {
      ledsClear();
      ledsShow();
    }
    #if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)
    digitalWrite(MOSFET_PIN, ONflag ? MOSFET_LEVEL : !MOSFET_LEVEL);
    #endif
    return false;
  }

  uint8_t phase = (elapsed / warnHalfPeriod) & 0x01U;
  uint8_t brightness = phase ? WARNING_BRIGHTNESS : 0U;
  if (phase != warnPhase || FastLED.getBrightness() != brightness) // яркость меняет и включение лампы (changePower) во время мигания
  {
    warnPhase = phase;
    fillAll(warnColor);
    FastLED.setBrightness(brightness);
    ledsShow();
  }
  return true;
}

static uint8_t overlayKind = OVERLAY_NONE;
static bool overlayWasOn = false;
static uint8_t overlayMode = 0U;                            // эффект, поверх которого показана картинка

uint8_t overlayCurrent()
{
  return overlayKind;
}

void overlayBegin(uint8_t kind)
{
  if (overlayKind == OVERLAY_NONE)
  {
    overlayWasOn = ONflag;
  }
  overlayKind = kind;
  overlayMode = currentMode;
  ONflag = true;                                            // выключенная лампа включается сразу на картинке, без разгорания эффекта
  loadingFlag = true;                                       // первый кадр рисуется сразу
  mqttRequestPublish();
}

void overlayEnd()
{
  if (overlayKind == OVERLAY_NONE)
  {
    return;
  }

  overlayKind = OVERLAY_NONE;
  FastLED.setBrightness(modes[currentMode].Brightness);
  loadingFlag = true;
  if (!overlayWasOn && ONflag)
  {
    ONflag = false;
    changePower();
  }
  mqttRequestPublish();
}

// кадр служебной строки или картинки вместо эффекта; false - кадр за эффектом
bool overlayTick()
{
  if (warningTick())
  {
    return true;
  }

  if (serviceTextActive())
  {
    serviceTextTick();
    return true;
  }

  if (overlayKind == OVERLAY_NONE)
  {
    return false;
  }

  if (!ONflag || currentMode != overlayMode)
  {
    overlayKind = OVERLAY_NONE;
    FastLED.setBrightness(modes[currentMode].Brightness);
    loadingFlag = true;
    return true;
  }

  if (overlayKind == OVERLAY_COUNTDOWN)
  {
    countdownTick();
  }
  else
  {
    diceTick();
  }
  return true;
}
