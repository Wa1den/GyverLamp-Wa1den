// служебные функции

// ================== ОБОРУДОВАНИЕ ==================
// Подключение матрицы, порядок цветов и лимит тока задаются на странице настроек (Служебное > Оборудование)
// и применяются на ходу. Размер матрицы (WIDTH, HEIGHT) остаётся в Config.h: от него зависят размеры
// массивов во всех эффектах.

static uint8_t hwMatrixConn = 0U;                           // угол подключения и направление ленты, см. hwMatrixConnections в SettingsUI.ino
static bool hwMatrixParallel = false;                       // разводка ленты: зигзаг или параллельная
static uint8_t hwWire[3] = {1U, 0U, 2U};                    // каналы RgbColor(R, G, B) для NeoGrbFeature в порядке hw_color_order
static uint16_t hwCurrentLimit = 2000U;                     // мА, 0 - без лимита

// порядок цветов ленты: какой канал кадра (0 - R, 1 - G, 2 - B) уходит по проводу первым, вторым, третьим
static const uint8_t hwColorOrders[6][3] PROGMEM = {
  {0U, 1U, 2U}, {0U, 2U, 1U}, {1U, 0U, 2U}, {1U, 2U, 0U}, {2U, 0U, 1U}, {2U, 1U, 0U}  // RGB, RBG, GRB, GBR, BRG, BGR
};

// настройки оборудования из базы - в переменные, которыми пользуются XY и ledsShow
void hwApply()
{
  hwMatrixConn = (uint8_t)db[kk::hw_matrix_conn] % 8U;
  hwMatrixParallel = (bool)db[kk::hw_matrix_parallel];
  hwCurrentLimit = (uint16_t)db[kk::hw_current_limit];

  // NeoGrbFeature отправляет по проводу байты (G, R, B) из RgbColor(R, G, B), поэтому первый байт провода
  // передаётся вторым параметром, второй - первым
  uint8_t order = (uint8_t)db[kk::hw_color_order] % 6U;
  hwWire[0] = pgm_read_byte(&hwColorOrders[order][1]);
  hwWire[1] = pgm_read_byte(&hwColorOrders[order][0]);
  hwWire[2] = pgm_read_byte(&hwColorOrders[order][2]);
  loadingFlag = true;                                       // эффекты, которые рисуют кадр один раз, перерисуются
}

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
  if (hwCurrentLimit)
  {
    brightness = calculate_max_brightness_for_power_mW(leds, NUM_LEDS, brightness, 5UL * hwCurrentLimit); // снижение яркости по лимиту тока (5 В * мА)
  }

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
      RgbColor color(channel[hwWire[0]], channel[hwWire[1]], channel[hwWire[2]]);
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

// номер пикселя в ленте по координатам (x - по окружности лампы, y - снизу вверх).
// Подключение hwMatrixConn: угол, из которого идёт лента, и направление первого ряда -
// 0 левый нижний вправо, 1 левый нижний вверх, 2 левый верхний вправо, 3 левый верхний вниз,
// 4 правый верхний влево, 5 правый верхний вниз, 6 правый нижний влево, 7 правый нижний вверх.
// tx - позиция вдоль ряда ленты, ty - номер ряда, w - длина ряда
uint16_t XY(uint8_t x, uint8_t y)
{
  uint8_t tx, ty, w;
  switch (hwMatrixConn)
  {
    case 1:  w = HEIGHT; tx = y;              ty = x;              break;
    case 2:  w = WIDTH;  tx = x;              ty = HEIGHT - y - 1; break;
    case 3:  w = HEIGHT; tx = HEIGHT - y - 1; ty = x;              break;
    case 4:  w = WIDTH;  tx = WIDTH - x - 1;  ty = HEIGHT - y - 1; break;
    case 5:  w = HEIGHT; tx = HEIGHT - y - 1; ty = WIDTH - x - 1;  break;
    case 6:  w = WIDTH;  tx = WIDTH - x - 1;  ty = y;              break;
    case 7:  w = HEIGHT; tx = y;              ty = WIDTH - x - 1;  break;
    default: w = WIDTH;  tx = x;              ty = y;              break;
  }
  if (!(ty & 0x01) || hwMatrixParallel)                     // чётные ряды зигзага и все ряды параллельной разводки идут вперёд
  {
    return ty * w + tx;
  }
  return ty * w + w - tx - 1;                               // нечётные ряды зигзага - в обратную сторону
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
