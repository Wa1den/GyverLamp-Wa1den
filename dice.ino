// Бросок кубиков (раздел «Кубики» на странице настроек).
//
// Кнопка кубика запускает анимацию броска вместо эффекта. Результат держится заданное время,
// при нуле - пока не вернуться к эффекту. Затем лампа возвращается к эффекту или выключается,
// если до броска была выключена. У каждого вида кубика своя анимация:
//   1d2   монетка подлетает, переворачивается и падает нужной стороной;
//   1d6   кубик с точками катится вокруг лампы и замедляется;
//   остальные  разноцветный шестиугольник катится вокруг лампы, а число стоит на месте
//         и сменяется всё реже; в конце шестиугольник останавливается под числом.
// На 1d20 выпавшие 20 и 1 отмечаются фоном: разноцветными искрами и красным мерцанием.
//
// Цифры числа стоят столбиком сверху вниз: число занимает 5 колонок, и его копия помещается на
// противоположной стороне лампы. Копию вместе с анимацией включает опция «Дублировать на обратной
// стороне», чтобы результат видели сидящие с разных сторон. Две цифры идут крупным шрифтом, три
// (только 100 на 1d100) - цифрами 3x5, у единицы при этом срезана верхняя точка: иначе не хватает строк.
//
// Пока кубик на лампе, два клика кнопкой возвращают к эффекту. С включённой опцией
// «Повторный бросок кнопкой лампы» один клик бросает тот же кубик ещё раз, без неё
// выключает лампу, как обычно.
//
// Результат берётся из аппаратного генератора случайных чисел ESP8266 (RANDOM_REG32).

#define DICE_FRAME_MS       (20U)
#define DICE_TYPES          (8U)
#define DICE_SEQ_LENGTH     (14U)                           // промежуточные значения анимации, последнее - результат
#define DICE_SIDE_SHIFT     (WIDTH / 2U)                    // сдвиг копии на противоположную сторону лампы
#define DICE_HEX_RADIUS     (4.0F)                          // радиус шестиугольника: от вершины до вершины 8 колонок; с копией на обратной стороне на полколонки меньше
#define DICE_HEX_LAPS       (5U)                            // сколько кругов проходит шестиугольник
#define DICE_CUBE_TURNS     (8U)                            // сколько раз кубик перекатывается; путь 8 граней кратен половине окружности, и кубик останавливается на своей стороне или напротив
#define DICE_COIN_FLIPS     (8U)                            // полуоборотов монетки; добавляется ещё один, если она должна упасть другой стороной
#define DICE_DIGITS_Y       (HEIGHT / 2U - 3U)              // нижняя строка однозначного числа
#define DICE_FACE_Y         (HEIGHT / 2U - 4U)              // нижняя строка грани кубика и монетки

static const uint8_t diceSides[DICE_TYPES] PROGMEM = {2U, 4U, 6U, 8U, 10U, 12U, 20U, 100U};

// точки на гранях кубика: сетка 3x3, бит - строка * 3 + столбец
static const uint16_t dicePips[6] PROGMEM = {0x010, 0x101, 0x111, 0x145, 0x155, 0x16D};

// цифры 1, 2 и 0 шрифтом 3x5 для монетки и числа 100, строки сверху вниз, старший из трёх битов - левый столбец
static const uint8_t diceSmallDigits[3][5] PROGMEM = {
  {0b010, 0b110, 0b010, 0b010, 0b111},
  {0b110, 0b001, 0b010, 0b100, 0b111},
  {0b111, 0b101, 0b101, 0b101, 0b111}
};

enum DiceState : uint8_t
{
  DICE_IDLE,                                                // кубика нет, работает эффект
  DICE_ROLLING,
  DICE_RESULT
};

static DiceState diceState = DICE_IDLE;
static uint8_t diceType = 0U;                               // индекс в diceSides
static uint8_t diceSeq[DICE_SEQ_LENGTH];
static uint8_t diceCoinStart = 0U;                          // сторона монетки на старте: 0 - единица, 1 - двойка
static uint32_t diceStartAt = 0U;
static uint32_t diceDurationMs = 0U;
static uint32_t diceResultAt = 0U;
static bool diceWasOn = false;                              // лампа была включена до броска
static uint8_t diceMode = 0U;                               // эффект, поверх которого показан кубик
static bool diceShown = false;                              // был хотя бы один бросок - поле «Результат» показывает его

bool diceActive()
{
  return diceState != DICE_IDLE;
}

bool diceRolling()
{
  return diceState == DICE_ROLLING;
}

uint8_t diceSidesOf(uint8_t type)
{
  return pgm_read_byte(&diceSides[type < DICE_TYPES ? type : 0U]);
}

static uint8_t diceRandom(uint8_t sides)
{
  return RANDOM_REG32 % sides + 1U;
}

void diceRoll(uint8_t type)
{
  if (type >= DICE_TYPES)
  {
    return;
  }

  if (diceState == DICE_IDLE)
  {
    diceWasOn = countdownActive() ? countdownAbort() : ONflag; // кубик сменяет обратный отсчёт и наследует, была ли лампа включена до него
  }
  diceMode = currentMode;
  ONflag = true;
  diceType = type;
  db.set(kk::dice_last, type);

  // промежуточные значения без повторов подряд, чтобы смена числа была видна
  uint8_t sides = diceSidesOf(type);
  diceSeq[DICE_SEQ_LENGTH - 1U] = diceRandom(sides);
  for (uint8_t i = 0U; i < DICE_SEQ_LENGTH - 1U; i++)
  {
    uint8_t value;
    do
    {
      value = diceRandom(sides);
    } while (sides > 2U && ((i && value == diceSeq[i - 1U]) || (i == DICE_SEQ_LENGTH - 2U && value == diceSeq[DICE_SEQ_LENGTH - 1U])));
    diceSeq[i] = value;
  }
  diceCoinStart = RANDOM_REG32 & 0x01;

  diceDurationMs = 4000U - (uint16_t)(uint8_t)db[kk::dice_speed] * 12U; // от 4 с на скорости 1 до 0.9 с на скорости 255
  diceStartAt = millis();
  diceState = DICE_ROLLING;
  diceShown = true;
  loadingFlag = true;
  mqttRequestPublish();
}

// возврат к эффекту, поверх которого показан кубик
void diceExit()
{
  if (diceState == DICE_IDLE)
  {
    return;
  }

  diceState = DICE_IDLE;
  FastLED.setBrightness(modes[currentMode].Brightness);
  loadingFlag = true;
  if (!diceWasOn && ONflag)
  {
    ONflag = false;
    changePower();
  }
  mqttRequestPublish();
}

// кубик убирается без возврата к эффекту, потому что лампу занимает обратный отсчёт;
// возвращает, была ли лампа включена до кубика
bool diceAbort()
{
  diceState = DICE_IDLE;
  return diceWasOn;
}

// результат для поля «Результат» на странице настроек
String diceText()
{
  if (!diceShown)
  {
    return F("-");
  }
  String text = F("1d");
  text += diceSidesOf(diceType);
  text += F(": ");
  if (diceState == DICE_ROLLING)
  {
    text += F("бросок...");
  }
  else
  {
    text += diceSeq[DICE_SEQ_LENGTH - 1U];
  }
  return text;
}

static float diceEaseOut(float t)
{
  float r = 1.0F - t;
  return 1.0F - r * r * r;
}

static void dicePixel(int16_t x, int16_t y, CRGB color)
{
  if (y >= 0 && y < (int16_t)HEIGHT)
  {
    drawPixelXY((uint8_t)(((x % (int16_t)WIDTH) + WIDTH) % WIDTH), y, color);
  }
}

// крупная цифра шрифтом 5x8 бегущей строки, y - нижняя строка; строки за пределами матрицы отбрасываются
static void diceBigDigit(int16_t x, int16_t y, uint8_t digit, CRGB color)
{
  for (uint8_t i = 0U; i < 5U; i++)
  {
    uint8_t column = pgm_read_byte(&fontHEX['0' + digit - ' '][i]);
    for (uint8_t j = 0U; j < 7U; j++)
    {
      if (column & (1U << j))                               // младший бит - верхний пиксель
      {
        dicePixel(x + i, y + 6 - j, color);
      }
    }
  }
}

// цифра 3x5 из diceSmallDigits, top - верхняя строка; строки за пределами матрицы отбрасываются
static void diceSmallDigit(int16_t left, int16_t top, uint8_t index, CRGB color)
{
  for (uint8_t row = 0U; row < 5U; row++)
  {
    uint8_t bits = pgm_read_byte(&diceSmallDigits[index][row]);
    for (uint8_t col = 0U; col < 3U; col++)
    {
      if (bits & (0b100 >> col))
      {
        dicePixel(left + col, top - row, color);
      }
    }
  }
}

// число с центром в колонке center, цифры столбиком сверху вниз
static void diceNumber(uint8_t value, int16_t center, CRGB color)
{
  if (value < 10U)
  {
    diceBigDigit(center - 2, DICE_DIGITS_Y, value, color);
  }
  else if (value < 100U)
  {
    diceBigDigit(center - 2, HEIGHT / 2U, value / 10U, color);
    diceBigDigit(center - 2, 0, value % 10U, color);
  }
  else                                                      // 100: верхняя строка единицы выходит за матрицу
  {
    diceSmallDigit(center - 1, HEIGHT, 0U, color);
    diceSmallDigit(center - 1, HEIGHT - 6, 2U, color);
    diceSmallDigit(center - 1, 4, 2U, color);
  }
}

// сколько копий рисовать: одну или две на противоположных сторонах лампы
static uint8_t diceCopies()
{
  return (bool)db[kk::dice_mirror] ? 2U : 1U;
}

// шестиугольник с центром (cx, cy), повёрнутый на angle. Пиксель внутри, если лежит по внутреннюю сторону
// всех шести граней; номер грани, дальше всех от которой выходит пиксель, задаёт сектор и его оттенок,
// поэтому поворот виден по цветным секторам. Край на полпикселя сглаживается по расстоянию до грани
static void diceHexagon(float cx, float cy, float angle, float radius, uint8_t hue)
{
  const float apothem = radius * 0.866F;
  float nx[6], ny[6];
  for (uint8_t k = 0U; k < 6U; k++)
  {
    float a = angle + PI / 6.0F + k * PI / 3.0F;          // нормали граней; при angle = 0 нижняя грань горизонтальна
    nx[k] = cosf(a);
    ny[k] = sinf(a);
  }

  int16_t x0 = (int16_t)floorf(cx);
  int16_t y0 = (int16_t)floorf(cy);
  for (int16_t x = x0 - 4; x <= x0 + 5; x++)
  {
    for (int16_t y = y0 - 4; y <= y0 + 5; y++)
    {
      float rx = x - cx;
      float ry = y - cy;
      float dist = -100.0F;
      uint8_t sector = 0U;
      for (uint8_t k = 0U; k < 6U; k++)
      {
        float d = rx * nx[k] + ry * ny[k];
        if (d > dist)
        {
          dist = d;
          sector = k;
        }
      }
      float coverage = apothem - dist + 0.5F;
      if (coverage <= 0.0F)
      {
        continue;
      }
      uint8_t value = (dist > apothem - 1.0F) ? 200U : 90U; // край ярче заливки
      if (coverage < 1.0F)
      {
        value = value * coverage;
      }
      dicePixel(x, y, CHSV(hue + sector * 21U, 255U, value));
    }
  }
}

// шестиугольник катится по окружности лампы, число стоит на месте и сменяется всё реже.
// Поворот - 60 градусов на каждую пройденную сторону, число сторон округлено до целого,
// чтобы шестиугольник остановился на грани; проскальзывание на долю стороны за пять кругов не видно
static void diceHexRoll(float t, uint8_t center, uint8_t hue, float radius)
{
  float eased = diceEaseOut(t);
  float distance = eased * DICE_HEX_LAPS * WIDTH;
  uint8_t sides = (uint8_t)(DICE_HEX_LAPS * WIDTH / radius + 0.5F);
  float angle = -eased * sides * PI / 3.0F;                 // вправо - по часовой стрелке
  diceHexagon(center + distance, HEIGHT / 2U, angle, radius, hue);

  uint8_t value = diceSeq[(uint8_t)(eased * (DICE_SEQ_LENGTH - 1U) + 0.5F)];
  if (t < 1.0F)
  {
    value %= 10U;                                           // пока шестиугольник катится - одна последняя цифра по центру: смена однозначных и двузначных чисел дёргала бы картинку
  }
  CRGB color = CHSV(hue, 40U, 255U);                        // почти белые цифры читаются поверх любого сектора
  diceNumber(value, center, color);
}

// грань кубика size x size, сжатая по ширине до width колонок при повороте; яркость фона растёт
// с шириной - грань поворачивается к свету. Грань 8x8 - с точками 2x2, грань 7x7 (при копии на
// обратной стороне, чтобы между копиями оставался зазор) - с точками в один пиксель: точки 2x2
// с промежутками в 7 колонок не укладываются
static void diceFace(int16_t left, uint8_t width, uint8_t value, uint8_t hue, uint8_t size)
{
  uint16_t pips = pgm_read_word(&dicePips[value - 1U]);
  CRGB face = CHSV(hue, 255U, 30U + width * 6U);
  CRGB pip = CHSV(hue, 60U, 255U);
  for (uint8_t c = 0U; c < width; c++)
  {
    uint8_t sx = c * size / width;
    for (uint8_t sy = 0U; sy < size; sy++)
    {
      bool isPip;
      if (size == 8U)
      {
        isPip = (sx % 3U != 2U) && (sy % 3U != 2U) && (pips & (1U << ((sy / 3U) * 3U + sx / 3U)));
      }
      else
      {
        isPip = (sx & 0x01) && (sy & 0x01) && sx < 6U && sy < 6U && (pips & (1U << ((sy / 2U) * 3U + sx / 2U)));
      }
      dicePixel(left + c, DICE_FACE_Y + size - 1 - sy, isPip ? pip : face);
    }
  }
}

// кубик катится вправо без проскальзывания: за четверть оборота центр сдвигается на ширину грани,
// видимая грань сжимается, а слева разворачивается следующая
static void diceCube(float t, uint8_t center, uint8_t hue, uint8_t size)
{
  float turns = diceEaseOut(t) * DICE_CUBE_TURNS;
  uint8_t k = (uint8_t)turns;
  float phi = (turns - k) * HALF_PI;
  uint8_t current = DICE_SEQ_LENGTH - 1U - DICE_CUBE_TURNS + k;
  uint8_t wCurrent = (uint8_t)(size * cosf(phi) + 0.5F);
  uint8_t wNext = (k < DICE_CUBE_TURNS) ? (uint8_t)(size * sinf(phi) + 0.5F) : 0U;
  int16_t left = center + (int16_t)(turns * size) - (wCurrent + wNext) / 2;

  if (wNext)
  {
    diceFace(left, wNext, (diceSeq[current + 1U] - 1U) % 6U + 1U, hue, size);
  }
  if (wCurrent)
  {
    diceFace(left + wNext, wCurrent, (diceSeq[current] - 1U) % 6U + 1U, hue, size);
  }
}

// монетка подлетает по дуге и переворачивается с постоянной скоростью; видимая высота - косинус угла
static void diceCoin(float t, uint8_t center, uint8_t hue)
{
  uint8_t result = diceSeq[DICE_SEQ_LENGTH - 1U] - 1U;
  uint8_t halfTurns = DICE_COIN_FLIPS + (diceCoinStart ^ result);
  float angle = t * halfTurns;                              // в полуоборотах
  uint8_t side = diceCoinStart ^ ((uint8_t)(angle + 0.5F) & 0x01);
  uint8_t height = (uint8_t)(8.0F * fabsf(cosf(angle * PI)) + 0.5F);
  if (height == 0U)
  {
    height = 1U;                                            // ребро монетки
  }
  int16_t bottom = DICE_FACE_Y + (int16_t)(16.0F * t * (1.0F - t)) + (8 - height) / 2;

  static const uint8_t discWidth[8] = {4U, 6U, 8U, 8U, 8U, 8U, 6U, 4U};
  CRGB color = side ? (CRGB)CHSV(hue, 150U, 200U) : (CRGB)CHSV(hue, 255U, 255U);
  for (uint8_t r = 0U; r < height; r++)
  {
    uint8_t sy = r * 8U / height;                           // строка исходной картинки монетки, 0 - нижняя
    for (uint8_t c = 0U; c < 8U; c++)
    {
      if (abs(2 * (int8_t)c - 7) >= discWidth[sy])
      {
        continue;
      }
      bool digit = false;
      if (sy >= 2U && sy <= 6U && c >= 2U && c <= 4U)
      {
        digit = pgm_read_byte(&diceSmallDigits[side][6U - sy]) & (0b100 >> (c - 2U));
      }
      dicePixel(center - 4 + c, bottom + r, digit ? CRGB::Black : color);
    }
  }
}

// вызывается из effectsTick вместо эффекта, пока diceActive()
void diceTick()
{
  if (!ONflag || currentMode != diceMode)                   // лампу выключили или сменили эффект - кубик убирается
  {
    diceState = DICE_IDLE;
    FastLED.setBrightness(modes[currentMode].Brightness);
    loadingFlag = true;
    return;
  }

  static uint32_t lastFrame = 0U;
  if (millis() - lastFrame < DICE_FRAME_MS)
  {
    return;
  }
  lastFrame = millis();

  uint8_t sides = diceSidesOf(diceType);
  uint8_t result = diceSeq[DICE_SEQ_LENGTH - 1U];
  uint8_t hue = (uint8_t)db[kk::dice_hue];
  uint8_t center = (uint8_t)db[kk::dice_rot] % WIDTH;
  uint8_t bri = (uint8_t)db[kk::dice_bri];

  float t = 1.0F;
  if (diceState == DICE_ROLLING)
  {
    uint32_t elapsed = millis() - diceStartAt;
    if (elapsed >= diceDurationMs)
    {
      diceState = DICE_RESULT;
      diceResultAt = millis();
      loadingFlag = true;
    }
    else
    {
      t = (float)elapsed / diceDurationMs;
    }
  }

  bool critical = false;
  if (diceState == DICE_RESULT)
  {
    uint16_t hold = (uint16_t)db[kk::dice_hold];
    if (hold && millis() - diceResultAt >= hold * 1000UL)
    {
      diceExit();
      return;
    }

    critical = sides == 20U && (result == 20U || result == 1U);
    static uint8_t lastHue, lastCenter, lastBri;             // без фона картинка результата неподвижна - лента обновляется только при смене настроек
    if (!critical && !loadingFlag && hue == lastHue && center == lastCenter && bri == lastBri)
    {
      return;
    }
    lastHue = hue;
    lastCenter = center;
    lastBri = bri;
  }
  loadingFlag = false;

  if (critical && result == 20U)                            // 20 на 1d20: разноцветные искры
  {
    nscale8(leds, NUM_LEDS, 200U);
    for (uint8_t i = 0U; i < 2U; i++)
    {
      leds[XY(random8(WIDTH), random8(HEIGHT))] = CHSV(random8(), 200U, 120U);
    }
  }
  else if (critical)                                        // 1 на 1d20: красное мерцание
  {
    fillAll(CHSV(0U, 255U, beatsin8(40U, 10U, 70U)));
  }
  else
  {
    ledsClear();
  }

  uint8_t copies = diceCopies();                            // с копией фигуры на колонку уже, иначе две копии по 8 колонок смыкаются в сплошное кольцо
  for (uint8_t side = 0U; side < copies; side++)
  {
    uint8_t sideCenter = center + side * DICE_SIDE_SHIFT;
    switch (sides)
    {
      case 2U:  diceCoin(t, sideCenter, hue); break;
      case 6U:  diceCube(t, sideCenter, hue, copies > 1U ? 7U : 8U); break;
      default:  diceHexRoll(t, sideCenter, hue, copies > 1U ? DICE_HEX_RADIUS - 0.5F : DICE_HEX_RADIUS); break;
    }
  }

  FastLED.setBrightness(bri);
  ledsShow();
}
