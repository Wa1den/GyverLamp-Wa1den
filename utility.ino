// служебные функции

// ================== ВЫВОД НА ЛЕНТУ ==================
// Кадр выводится через аппаратный UART1 (NeoPixelBus), а не битбангингом FastLED:
// WiFi на ESP8266 использует NMI-прерывания, которые нельзя запретить, и они портили
// кадры при программном выводе (мигание первого пикселя зелёным/красным).
// Аппаратному UART прерывания безразличны. FastLED остаётся для всей математики
// эффектов (leds[], палитры, глобальная яркость).

// вывод кадра leds[] на ленту с применением глобальной яркости и лимита по току;
// если итоговый кадр не отличается от уже показанного, передача пропускается.
// сюда же подмешивается световая волна-отклик на касание кнопки (BUTTON_PRESS_FEEDBACK):
// оверлей применяется к копии цветов и не трогает leds[] - состояние эффектов не портится
void ledsShow()
{
  uint8_t brightness = FastLED.getBrightness();
  #ifdef USE_AUTO_BRIGHTNESS
  if (autoBriFactor != 255U && !dawnFlag)                   // автояркость масштабирует яркость эффекта; рассвет-будильник не приглушается
  {
    brightness = scale8(brightness, autoBriFactor);
  }
  #endif //USE_AUTO_BRIGHTNESS
  #if (CURRENT_LIMIT > 0)
  brightness = calculate_max_brightness_for_power_mW(leds, NUM_LEDS, brightness, 5UL * CURRENT_LIMIT); // автоматическое снижение яркости по лимиту тока (5В * CURRENT_LIMIT мА)
  #endif

  #ifdef BUTTON_PRESS_FEEDBACK
  // анимация "нажатия": световая полоса продавливается сверху вниз с разгоном и
  // торможением у нижней точки (ease-in-out), затем отпускается - трогается медленно
  // и ускоряется к вылету наверх (квадратичная кривая)
  uint8_t feedbackGlow[HEIGHT] = {0};                       // добавка белого свечения по строкам
  uint32_t fbElapsed = millis() - buttonFeedbackAt;
  if (buttonFeedbackAt != 0U && fbElapsed < BUTTON_PRESS_FEEDBACK)
  {
    const uint8_t waveDepth = 5U;                           // глубина "продавливания" в строках от верхнего края
    const uint16_t pressTime = BUTTON_PRESS_FEEDBACK * 3U / 5U; // 60% времени - нажатие, 40% - отпускание
    uint8_t wavePos;
    if (fbElapsed < pressTime)
    {
      uint8_t u = (uint32_t)fbElapsed * 255U / pressTime;
      wavePos = (uint16_t)waveDepth * ease8InOutQuad(u) / 255U;
    }
    else
    {
      uint8_t u = (uint32_t)(fbElapsed - pressTime) * 255U / (BUTTON_PRESS_FEEDBACK - pressTime);
      wavePos = (uint16_t)waveDepth * (255U - scale8(u, u)) / 255U; // u^2: медленно от нижней точки, быстро к вылету
    }

    for (uint8_t y = 0U; y < HEIGHT; y++)
    {
      uint8_t rowFromTop = HEIGHT - 1U - y;
      uint8_t dist = (rowFromTop > wavePos) ? rowFromTop - wavePos : wavePos - rowFromTop;
      if (dist < 3U)
      {
        feedbackGlow[y] = 140U - dist * 45U;
      }
    }
  }
  #endif //BUTTON_PRESS_FEEDBACK

  bool frameChanged = false;
  for (uint8_t y = 0U; y < HEIGHT; y++)
  {
    for (uint8_t x = 0U; x < WIDTH; x++)
    {
      uint16_t i = XY(x, y);
      CRGB c = leds[i];
      #ifdef BUTTON_PRESS_FEEDBACK
      if (feedbackGlow[y] > 0U)
      {
        c.r = qadd8(c.r, feedbackGlow[y]);
        c.g = qadd8(c.g, feedbackGlow[y]);
        c.b = qadd8(c.b, feedbackGlow[y]);
      }
      #endif //BUTTON_PRESS_FEEDBACK
      uint8_t channel[3] = {scale8(c.r, brightness), scale8(c.g, brightness), scale8(c.b, brightness)};
      RgbColor color(channel[COLOR_WIRE_1], channel[COLOR_WIRE_0], channel[COLOR_WIRE_2]);
      if (ledStrip.GetPixelColor(i) != color)
      {
        ledStrip.SetPixelColor(i, color);
        frameChanged = true;
      }
    }
  }

  if (frameChanged)
  {
    ledStrip.Show();
  }
}

// тикер анимации отклика на касание кнопки: обеспечивает кадры волны, когда эффект
// не перерисовывается сам (выключенная лампа, статичные Белый свет/Цвет)
void ledsFeedbackTick()
{
  #ifdef BUTTON_PRESS_FEEDBACK
  static uint32_t lastFeedbackShow = 0U;
  if (buttonFeedbackAt != 0U &&
      millis() - buttonFeedbackAt < BUTTON_PRESS_FEEDBACK + 60U &&          // +60 мс: финальный кадр уже без волны, чтобы она не "зависла"
      millis() - lastFeedbackShow >= 25U)
  {
    lastFeedbackShow = millis();
    if (!ONflag)
    {
      uint8_t savedBrightness = FastLED.getBrightness();
      FastLED.setBrightness(BRIGHTNESS);                    // на выключенной лампе (яркость может быть 0 после гашения) волна показывается на стандартной яркости
      ledsShow();
      FastLED.setBrightness(savedBrightness);
    }
    else
    {
      ledsShow();
    }
  }
  #endif //BUTTON_PRESS_FEEDBACK
}

// очистка кадра
void ledsClear()
{
  fill_solid(leds, NUM_LEDS, CRGB::Black);
}

// залить все
void fillAll(CRGB color)
{
  for (uint16_t i = 0; i < NUM_LEDS; i++)
    leds[i] = color;
}

// функция отрисовки точки по координатам X Y
#if (WIDTH > 127) || (HEIGHT > 127)
void drawPixelXY(int16_t x, int16_t y, CRGB color)
#else
void drawPixelXY(int8_t x, int8_t y, CRGB color)
#endif
{
  if (x < 0 || x > (WIDTH - 1) || y < 0 || y > (HEIGHT - 1)) return;
  leds[XY(x, y)] = color;
}

// функция получения цвета пикселя по его номеру
uint32_t getPixColor(uint16_t thisPixel)
{
  if (thisPixel >= NUM_LEDS) return 0;
  return (((uint32_t)leds[thisPixel].r << 16) | ((uint32_t)leds[thisPixel].g << 8 ) | (uint32_t)leds[thisPixel].b);
}


// функция получения цвета пикселя в матрице по его координатам
uint32_t getPixColorXY(uint8_t x, uint8_t y)
{
  return getPixColor(XY(x, y));
}

// ************* НАСТРОЙКА МАТРИЦЫ *****
#if (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 0)
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y y

#elif (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 1)
#define _WIDTH HEIGHT
#define THIS_X y
#define THIS_Y x

#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 0)
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y (HEIGHT - y - 1)

#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 3)
#define _WIDTH HEIGHT
#define THIS_X (HEIGHT - y - 1)
#define THIS_Y x

#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 2)
#define _WIDTH WIDTH
#define THIS_X (WIDTH - x - 1)
#define THIS_Y (HEIGHT - y - 1)

#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 3)
#define _WIDTH HEIGHT
#define THIS_X (HEIGHT - y - 1)
#define THIS_Y (WIDTH - x - 1)

#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 2)
#define _WIDTH WIDTH
#define THIS_X (WIDTH - x - 1)
#define THIS_Y y

#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 1)
#define _WIDTH HEIGHT
#define THIS_X y
#define THIS_Y (WIDTH - x - 1)

#else
!!!!!!!!!!!!!!!!!!!!!!!!!!!   смотрите инструкцию: https://alexgyver.ru/wp-content/uploads/2018/11/scheme3.jpg
!!!!!!!!!!!!!!!!!!!!!!!!!!!   такого сочетания CONNECTION_ANGLE и STRIP_DIRECTION не бывает
#define _WIDTH WIDTH
#define THIS_X x
#define THIS_Y y
#pragma message "Wrong matrix parameters! Set to default"

#endif

// получить номер пикселя в ленте по координатам
// библиотека FastLED тоже использует эту функцию
uint16_t XY(uint8_t x, uint8_t y)
{
  if (!(THIS_Y & 0x01) || MATRIX_TYPE)               // Even rows run forwards
    return (THIS_Y * _WIDTH + THIS_X);
  else                                                  
    return (THIS_Y * _WIDTH + _WIDTH - THIS_X - 1);  // Odd rows run backwards
}

// если у вас матрица необычной формы с зазорами/вырезами, либо просто маленькая, тогда вам придётся переписать функцию XY() под себя
// массив для переадресации можно сформировать на этом онлайн-сервисе: https://macetech.github.io/FastLED-XY-Map-Generator/
// или тут по-русски: https://firelamp.pp.ua/matrix_generator/

// ниже пример функции, когда у вас матрица 8х16, а вы хотите, чтобы эффекты рисовались, будто бы матрица 16х16 (рисуем по центру, а по бокам обрезано)
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  Х  Х  -  -  -  - 
//   -  -  -  -  Х  Х  Х  Х  Х  Х  9  8  -  -  -  - 
//   -  -  -  -  0  1  2  3  4  5  6  7  -  -  -  -

// было оставлено для совместимости с эффектами из старых прошивок


// восстановление настроек эффектов на настройки по умолчанию
void restoreSettings()
{
    for (uint8_t i = 0; i < MODE_AMOUNT; i++) {
      modes[i].Brightness = pgm_read_byte(&defaultSettings[i][0]);
      modes[i].Speed      = pgm_read_byte(&defaultSettings[i][1]);
      modes[i].Scale      = pgm_read_byte(&defaultSettings[i][2]);
    }
//  else                                              // иначе берём какие-то абстрактные
}

// неточный, зато более быстрый квадратный корень
float sqrt3(const float x)
{
  union
  {
    int i;
    float x;
  } u;

  u.x = x;
  u.i = (1<<29) + (u.i >> 1) - (1<<22);
  return u.x;
}
