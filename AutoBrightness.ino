// Автояркость по датчику освещённости на A0 (единственный аналоговый вход ESP8266).
//
// Датчик опрашивается 10 раз в секунду с экспоненциальным сглаживанием (~1 сек),
// из уровня освещённости считается коэффициент autoBriFactor, которым ledsShow
// МАСШТАБИРУЕТ текущую яркость эффекта (пропорции яркости между эффектами
// сохраняются): в темноте лампа тускнеет до "мин. яркости", на свету работает
// в полную силу. Рассвет-будильник коэффициенту не подчиняется (см. ledsShow).
//
// На лампе без датчика модуль безвреден: функция выключена по умолчанию,
// а сырое значение A0 и уровень D5 показываются на странице настроек -
// по ним же удобно выяснить, куда распаян датчик (накрыть его рукой и
// смотреть, какое число реагирует).

#ifdef USE_AUTO_BRIGHTNESS

uint8_t autoBriFactor = 255U;                               // текущий коэффициент яркости (255 - без ослабления); применяется в ledsShow
uint16_t autoLightRaw = 0U;                                 // сырое значение A0 для диагностики на странице настроек

void autoBrightnessTick()
{
  static uint32_t lastReadTime = 0U;
  static uint16_t filtered = 0U;
  static bool firstRead = true;

  if (millis() - lastReadTime < 100U)
  {
    return;
  }
  lastReadTime = millis();

  autoLightRaw = analogRead(A0);                            // читается всегда (и при выключенной функции) - для диагностики

  if (!(bool)db[kk::ab_on])
  {
    autoBriFactor = 255U;
    firstRead = true;
    return;
  }

  uint16_t level = (bool)db[kk::ab_invert] ? (1023U - autoLightRaw) : autoLightRaw;
  if (firstRead)
  {
    firstRead = false;
    filtered = level;
  }
  filtered = filtered - (filtered >> 3) + (level >> 3);     // экспоненциальное сглаживание, постоянная времени ~1 сек

  // чувствительность задаёт диапазон датчика, на котором достигается полная яркость:
  // выше чувствительность - меньше света нужно для максимума
  uint16_t range = map((uint8_t)db[kk::ab_sens], 1, 100, 1023, 64);
  uint8_t minFactor = (uint16_t)(uint8_t)db[kk::ab_min_bri] * 255U / 100U;
  autoBriFactor = minFactor + (uint32_t)constrain(filtered, 0U, range) * (255U - minFactor) / range;
}

#else //USE_AUTO_BRIGHTNESS

void autoBrightnessTick() {}

#endif //USE_AUTO_BRIGHTNESS
