// Картинка поверх эффекта: обратный отсчёт или кубики. На лампе одна такая картинка, новая сменяет
// прежнюю. После неё лампа возвращается к эффекту, а если была выключена до первой из них - гаснет.
// Выключение лампы или смена эффекта убирают картинку. Служебная строка (IP, время) идёт поверх всего.

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
