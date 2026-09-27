// Обратный отсчёт (раздел «Обратный отсчёт» на странице настроек).
//
// Пока отсчёт идёт или стоит на паузе, лампа показывает оставшееся время вместо эффекта.
// Пока осталось больше минуты, минуты стоят над секундами цифрами 3x5, в последнюю минуту
// остаются секунды шрифтом 5x8. В последние 5 секунд на смене каждой секунды фон вспыхивает
// цветом, противоположным цвету цифр, и каждая следующая вспышка ярче. По окончании фон
// вспыхивает пять раз на полную яркость, после чего лампа возвращается к эффекту или
// выключается, если до старта была выключена.
//
// Отсчёт прерывается без вспышек, если лампу выключили или сменили эффект - кнопкой,
// со страницы настроек, по MQTT или таймером выключения.

#define COUNTDOWN_FRAME_MS      (20U)                       // период кадра во время вспышек
#define COUNTDOWN_FLASH_RISE    (150U)                      // вспышка разгорается, мс
#define COUNTDOWN_FLASH_FALL    (250U)                      // и гаснет, мс
#define COUNTDOWN_FINAL_PERIOD  (500U)                      // период финальных вспышек, мс
#define COUNTDOWN_FINAL_FLASHES (5U)                        // сколько раз фон вспыхивает по окончании
#define COUNTDOWN_WARN_SECONDS  (5U)                        // сколько последних секунд отмечается вспышками

enum CountdownState : uint8_t
{
  CD_IDLE,                                                  // отсчёта нет, работает эффект
  CD_RUNNING,
  CD_PAUSED,                                                // цифры остаются на лампе, время стоит
  CD_FINAL                                                  // время вышло, идут финальные вспышки
};

static CountdownState cdState = CD_IDLE;
static uint32_t cdEndAt = 0U;                               // millis() окончания отсчёта
static uint32_t cdRemainMs = 0U;                            // остаток на момент паузы
static uint32_t cdFinalAt = 0U;                             // millis() начала финальных вспышек
static bool cdWasOn = false;                                // лампа была включена до старта
static uint8_t cdMode = 0U;                                 // эффект, поверх которого идёт отсчёт

bool countdownActive()
{
  return cdState != CD_IDLE;
}

bool countdownPaused()
{
  return cdState == CD_PAUSED;
}

uint32_t countdownRemainMs()
{
  if (cdState == CD_RUNNING)
  {
    int32_t remain = (int32_t)(cdEndAt - millis());
    return remain > 0 ? (uint32_t)remain : 0U;
  }
  return cdState == CD_PAUSED ? cdRemainMs : 0U;
}

// старт с полного интервала; на паузе - продолжение с того же места
void countdownStart()
{
  if (cdState == CD_PAUSED)
  {
    cdEndAt = millis() + cdRemainMs;
    cdState = CD_RUNNING;
    return;
  }

  if (cdState == CD_IDLE)
  {
    cdWasOn = ONflag;
  }
  cdMode = currentMode;
  ONflag = true;                                            // выключенная лампа включается сразу на цифрах, без разгорания эффекта
  cdEndAt = millis() + (uint32_t)(uint16_t)db[kk::cd_seconds] * 1000UL;
  cdState = CD_RUNNING;
  loadingFlag = true;                                       // первый кадр отсчёта рисуется сразу
  mqttRequestPublish();
}

void countdownPause()
{
  if (cdState == CD_RUNNING)
  {
    cdRemainMs = countdownRemainMs();
    cdState = CD_PAUSED;
  }
}

// возврат к эффекту, поверх которого шёл отсчёт
void countdownStop()
{
  if (cdState == CD_IDLE)
  {
    return;
  }

  cdState = CD_IDLE;
  FastLED.setBrightness(modes[currentMode].Brightness);
  loadingFlag = true;
  if (!cdWasOn && ONflag)
  {
    ONflag = false;
    changePower();
  }
  mqttRequestPublish();
}

// яркость вспышки через t мс от её начала: разгорается с нарастающим темпом и гаснет с убывающим
static uint8_t countdownFlash(uint32_t t, uint8_t peak)
{
  uint8_t k;
  if (t < COUNTDOWN_FLASH_RISE)
  {
    k = t * 255U / COUNTDOWN_FLASH_RISE;
  }
  else if (t < COUNTDOWN_FLASH_RISE + COUNTDOWN_FLASH_FALL)
  {
    k = 255U - (t - COUNTDOWN_FLASH_RISE) * 255U / COUNTDOWN_FLASH_FALL;
  }
  else
  {
    return 0U;
  }
  return scale8(peak, scale8(k, k));
}

// цифра шрифтом 5x8 бегущей строки; столбцы переносятся через шов
static void countdownBigDigit(uint8_t x, uint8_t digit, CRGB color)
{
  for (uint8_t i = 0U; i < 5U; i++)
  {
    uint8_t column = pgm_read_byte(&fontHEX['0' + digit - ' '][i]);
    for (uint8_t j = 0U; j < 8U; j++)
    {
      if (column & (1U << j))                               // младший бит - верхний пиксель, цифры занимают биты 0-6
      {
        drawPixelXY((x + i) % WIDTH, HEIGHT / 2U + 3U - j, color);
      }
    }
  }
}

static void countdownDraw(uint16_t seconds, uint8_t flash)
{
  uint8_t hue = (uint8_t)db[kk::cd_hue];
  uint8_t center = (uint8_t)db[kk::cd_rot] % WIDTH;
  CRGB color = CHSV(hue, 255U, 255U);

  fillAll(flash ? (CRGB)CHSV(hue + 128U, 255U, flash) : CRGB::Black);

  if (seconds >= 60U)                                       // минуты над секундами, как в эффекте Часы
  {
    uint8_t left = (center + WIDTH - 3U) % WIDTH;
    uint8_t minutes = seconds / 60U;
    seconds %= 60U;
    drawDig3x5(left, HEIGHT / 2U + 1U, minutes / 10U, color);
    drawDig3x5((left + 4U) % WIDTH, HEIGHT / 2U + 1U, minutes % 10U, color);
    drawDig3x5(left, HEIGHT / 2U - 6U, seconds / 10U, color);
    drawDig3x5((left + 4U) % WIDTH, HEIGHT / 2U - 6U, seconds % 10U, color);
  }
  else if (seconds >= 10U)
  {
    uint8_t left = (center + WIDTH - 5U) % WIDTH;
    countdownBigDigit(left, seconds / 10U, color);
    countdownBigDigit((left + 6U) % WIDTH, seconds % 10U, color);
  }
  else
  {
    countdownBigDigit((center + WIDTH - 2U) % WIDTH, seconds, color);
  }
}

// вызывается из effectsTick вместо эффекта, пока countdownActive()
void countdownTick()
{
  if (!ONflag || currentMode != cdMode)                     // лампу выключили или сменили эффект - отсчёт отменяется
  {
    cdState = CD_IDLE;
    FastLED.setBrightness(modes[currentMode].Brightness);
    loadingFlag = true;
    return;
  }

  static uint32_t lastFrame = 0U;
  if (millis() - lastFrame < COUNTDOWN_FRAME_MS)
  {
    return;
  }
  lastFrame = millis();

  uint32_t remain = countdownRemainMs();
  if (cdState == CD_RUNNING && remain == 0U)
  {
    cdState = CD_FINAL;
    cdFinalAt = millis();
  }

  uint16_t seconds = (remain + 999U) / 1000U;               // на цифрах - целые секунды с округлением вверх, 0 появляется в момент окончания
  uint8_t flash = 0U;
  if (cdState == CD_FINAL)
  {
    uint32_t elapsed = millis() - cdFinalAt;
    if (elapsed >= COUNTDOWN_FINAL_FLASHES * COUNTDOWN_FINAL_PERIOD)
    {
      countdownStop();
      return;
    }
    flash = countdownFlash(elapsed % COUNTDOWN_FINAL_PERIOD, 255U);
  }
  else if (cdState == CD_RUNNING && seconds <= COUNTDOWN_WARN_SECONDS)
  {
    uint8_t peak = 255U * (COUNTDOWN_WARN_SECONDS + 1U - seconds) / COUNTDOWN_WARN_SECONDS; // от пятой части полной яркости на 5 с до полной на 1 с
    flash = countdownFlash(seconds * 1000UL - remain, peak);
  }

  // лента обновляется, только когда картинка изменилась: во время вспышки каждый кадр, иначе раз в секунду
  static uint16_t lastSeconds = 0xFFFFU;
  static uint8_t lastFlash, lastHue, lastRot, lastBri;
  uint8_t hue = (uint8_t)db[kk::cd_hue];
  uint8_t rot = (uint8_t)db[kk::cd_rot];
  uint8_t bri = (uint8_t)db[kk::cd_bri];
  if (!loadingFlag && seconds == lastSeconds && flash == lastFlash && hue == lastHue && rot == lastRot && bri == lastBri)
  {
    return;
  }
  lastSeconds = seconds;
  lastFlash = flash;
  lastHue = hue;
  lastRot = rot;
  lastBri = bri;
  loadingFlag = false;

  countdownDraw(seconds, flash);
  FastLED.setBrightness(bri);
  ledsShow();
}

// оставшееся время строкой мм:сс для страницы настроек; без отсчёта - выбранный интервал
void countdownText(char* buf)
{
  uint32_t seconds = countdownActive() ? (countdownRemainMs() + 999U) / 1000U : (uint16_t)db[kk::cd_seconds];
  sprintf_P(buf, PSTR("%02u:%02u"), (uint16_t)(seconds / 60U), (uint16_t)(seconds % 60U));
}
