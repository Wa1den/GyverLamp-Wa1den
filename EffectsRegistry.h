#pragma once

// Реестр эффектов. Каждый эффект описан одной строкой EFFECT_LIST, из неё собираются номера EFF_*,
// количество MODE_AMOUNT, настройки по умолчанию, названия для страницы настроек, период кадра,
// подписи ползунков и вызов эффекта в effectsTick (effectTicker.ino).
//
// Новый эффект добавляется в конец списка: номер эффекта - его позиция, он же номер в MQTT-командах
// (EFFn) и в названии. При добавлении в конец настройки и отметки Цикла у прежних эффектов сохраняются.
//
// Поля: номер, вызов функции эффекта, название, яркость, Скорость, Масштаб по умолчанию,
// период кадра в мс (0 - кадр раз в 256 - Скорость мс), подписи Скорости и Масштаба на странице
// настроек (пустая - эффект этот ползунок не использует, и он скрыт). Масштаб у многих эффектов
// Gunner47 задаёт два параметра: одну из 9 палитр крупным шагом и второй параметр внутри каждых
// 11 значений - отсюда подписи вида «Палитра и количество».

#define EFFECT_LIST(X) \
  X(EFF_WHITE_COLOR,     whiteColorStripeRoutine(),         "0. Белый свет",   9, 207,  26, 50, "Оттенок и поворот", "Ширина полосы") \
  X(EFF_COLOR,           colorRoutine(),                    "1. Цвет",   9, 180,  99, 50, "Насыщенность", "Цвет") \
  X(EFF_COLORS,          colorsRoutine2(),                  "2. Смена цвета",  10, 252,  32, 50, "Скорость", "Шаг смены цвета") \
  X(EFF_MADNESS,         madnessNoiseRoutine(),             "3. Безумие",  11,  33,  58, 50, "Скорость", "Размер узора") \
  X(EFF_CLOUDS,          cloudsNoiseRoutine(),              "4. Облака",   8,   4,  34, 50, "Скорость", "Размер узора") \
  X(EFF_LAVA,            lavaNoiseRoutine(),                "5. Лава",   8,   9,  24, 50, "Скорость", "Размер узора") \
  X(EFF_PLASMA,          plasmaNoiseRoutine(),              "6. Плазма",  11,  19,  59, 50, "Скорость", "Размер узора") \
  X(EFF_RAINBOW,         rainbowNoiseRoutine(),             "7. Радуга 3D",  11,  13,  60, 50, "Скорость", "Размер узора") \
  X(EFF_RAINBOW_STRIPE,  rainbowStripeNoiseRoutine(),       "8. Павлин",  11,   5,  12, 50, "Скорость", "Размер узора") \
  X(EFF_ZEBRA,           zebraNoiseRoutine(),               "9. Зебра",   7,   8,  21, 50, "Скорость", "Размер узора") \
  X(EFF_FOREST,          forestNoiseRoutine(),              "10. Лес",   7,   8,  95, 50, "Скорость", "Размер узора") \
  X(EFF_OCEAN,           oceanNoiseRoutine(),               "11. Океан",   7,   6,  12, 50, "Скорость", "Размер узора") \
  X(EFF_BBALLS,          BBallsRoutine(),                   "12. Мячики",  24, 255,  26, 15, "Цвет и шлейф", "Количество") \
  X(EFF_BALLS_BOUNCE,    bounceRoutine(),                   "13. Мячики без границ",  18,  11,  70, 15, "Шлейф", "Палитра и количество") \
  X(EFF_POPCORN,         popcornRoutine(),                  "14. Попкорн",  19,  32,  16, 15, "Скорость", "Палитра и количество") \
  X(EFF_SPIRO,           spiroRoutine(),                    "15. Спирали",   9,  46,   3, 15, "Шлейф", "Палитра") \
  X(EFF_PRISMATA,        PrismataRoutine(),                 "16. Призмата",  17, 100,   2, 15, "Скорость", "Палитра и шлейф") \
  X(EFF_SMOKEBALLS,      smokeballsRoutine(),               "17. Дымовые шашки",  12,  44,  17, 15, "Скорость", "Количество") \
  X(EFF_FLAME,           execStringsFlame(),                "18. Пламя",  22,  53,   3, 15, "Количество языков", "Цвет и высота") \
  X(EFF_FIRE_2021,       Fire2021Routine(),                 "19. Огонь 2021",   9,  51,  11, 15, "Скорость", "Палитра и ширина языков") \
  X(EFF_PACIFIC,         pacificRoutine(),                  "20. Тихий океан",  55, 127, 100, 15, "Скорость", "") \
  X(EFF_SHADOWS,         shadowsRoutine(),                  "21. Тени",  39,  77,   1, 15, "Скорость", "Контраст") \
  X(EFF_DNA,             DNARoutine(),                      "22. ДНК",  15,  77,  95, 15, "Скорость", "Цвет") \
  X(EFF_FLOCK,           flockRoutine(false),               "23. Стая",  15, 136,   4, 15, "Скорость", "Палитра и шлейф") \
  X(EFF_FLOCK_N_PR,      flockRoutine(true),                "24. Стая и хищник",  15, 128,  80, 15, "Скорость", "Палитра и шлейф") \
  X(EFF_BUTTERFLYS,      butterflysRoutine(true),           "25. Мотыльки",  11,  53,  87, 15, "Скорость", "Количество и цвет") \
  X(EFF_BUTTERFLYS_LAMP, butterflysRoutine(false),          "26. Лампа с мотыльками",   7,  61, 100, 15, "Скорость", "Количество и цвет") \
  X(EFF_SNAKES,          snakesRoutine(),                   "27. Змейки",   9,  96,  31, 15, "Скорость", "Количество") \
  X(EFF_NEXUS,           nexusRoutine(),                    "28. Nexus",  19,  60,  20, 15, "Скорость", "Количество") \
  X(EFF_SPHERES,         spheresRoutine(),                  "29. Шары",   9,  85,  85, 15, "Скорость", "Палитра и количество") \
  X(EFF_SINUSOID3,       Sinusoid3Routine(),                "30. Синусоид",   7,  89,  83, 15, "Вариант и скорость", "Размер") \
  X(EFF_METABALLS,       MetaBallsRoutine(),                "31. Метаболз",   7,  85,   3, 15, "Скорость", "Палитра") \
  X(EFF_AURORA,          polarRoutine(),                    "32. Северное сияние",  12,  73,  38, 15, "Скорость", "Цвет") \
  X(EFF_SPIDER,          spiderRoutine(),                   "33. Плазменная лампа",   8,  59,  18, 15, "Скорость", "Палитра и количество линий") \
  X(EFF_LAVALAMP,        LavaLampRoutine(),                 "34. Лавовая лампа",  23, 203,   1, 15, "Скорость", "Цвет") \
  X(EFF_LIQUIDLAMP,      LiquidLampRoutine(true),           "35. Жидкая лампа",  11,  63,   1, 15, "Скорость", "Цвет и количество") \
  X(EFF_LIQUIDLAMP_AUTO, LiquidLampRoutine(false),          "36. Жидкая лампа (auto)",  11, 124,  39, 15, "Скорость", "Цвет и количество") \
  X(EFF_DROPS,           newMatrixRoutine(),                "37. Капли на стекле",  23,  71,  59, 15, "Количество капель", "Цвет") \
  X(EFF_MATRIX,          matrixRoutine(),                   "38. Матрица",  27, 186,  23,  0, "Скорость", "Плотность") \
  X(EFF_FIRE_2012,       fire2012again(),                   "39. Огонь 2012",   9, 225,  59,  0, "Скорость", "Палитра") \
  X(EFF_FIRE_2018,       Fire2018_2(),                      "40. Огонь 2018",  57, 225,  15,  0, "Скорость", "Оттенок") \
  X(EFF_FIRE_2020,       fire2020Routine2(),                "41. Огонь 2020",   9, 220,  20,  0, "Скорость", "Палитра и ширина языков") \
  X(EFF_FIRE,            fireRoutine(true),                 "42. Огонь",  22, 225,   1,  0, "Скорость", "Цвет") \
  X(EFF_WHIRL,           whirlRoutine(true),                "43. Вихри пламени",   9, 240,   1,  0, "Скорость", "Цвет") \
  X(EFF_WHIRL_MULTI,     whirlRoutine(false),               "44. Разноцветные вихри",   9, 240,  86,  0, "Скорость", "Цвет") \
  X(EFF_MAGMA,           magmaRoutine(),                    "45. Магма",   9, 198,  20,  0, "Скорость", "Палитра и количество") \
  X(EFF_LLAND,           LLandRoutine(),                    "46. Кипение",   7, 240,  18,  0, "Скорость", "Палитра и размер") \
  X(EFF_WATERFALL,       fire2012WithPalette(),             "47. Водопад",   5, 212,  54,  0, "Скорость", "Цвет") \
  X(EFF_WATERFALL_4IN1,  fire2012WithPalette4in1(),         "48. Водопад 4 в 1",   7, 197,  22,  0, "Скорость", "Вариант и высота") \
  X(EFF_POOL,            poolRoutine(),                     "49. Бассейн",   8, 222,  63,  0, "Скорость", "Цвет") \
  X(EFF_PULSE,           pulseRoutine(2U),                  "50. Пульс",  12, 185,   6,  0, "Скорость", "Цвет") \
  X(EFF_PULSE_RAINBOW,   pulseRoutine(4U),                  "51. Радужный пульс",  11, 185,  31,  0, "Скорость", "Шаг радуги") \
  X(EFF_PULSE_WHITE,     pulseRoutine(8U),                  "52. Белый пульс",   9, 179,  11,  0, "Скорость", "Оттенок") \
  X(EFF_OSCILLATING,     oscillatingRoutine(),              "53. Осциллятор",   8, 208, 100,  0, "Скорость", "Палитра или цвет") \
  X(EFF_FOUNTAIN,        starfield2Routine(),               "54. Источник",  15, 233,  77,  0, "Скорость", "Количество") \
  X(EFF_FAIRY,           fairyRoutine(),                    "55. Фея",  19, 212,  44,  0, "Скорость", "Количество") \
  X(EFF_COMET,           RainbowCometRoutine(),             "56. Комета",  16, 220,  28,  0, "Скорость", "Скорость смены цвета") \
  X(EFF_COMET_COLOR,     ColorCometRoutine(),               "57. Одноцветная комета",  14, 212,  69,  0, "Скорость", "Цвет") \
  X(EFF_COMET_TWO,       MultipleStream(),                  "58. Две кометы",  27, 186,  19,  0, "Скорость", "Шлейф") \
  X(EFF_COMET_THREE,     MultipleStream2(),                 "59. Три кометы",  24, 186,   9,  0, "Скорость", "Шлейф") \
  X(EFF_ATTRACT,         attractRoutine(),                  "60. Притяжение",  21, 203,  65,  0, "Скорость", "Палитра и количество") \
  X(EFF_FIREFLY,         MultipleStream3(),                 "61. Парящий огонь",  26, 206,  15,  0, "Скорость", "Шлейф") \
  X(EFF_FIREFLY_TOP,     MultipleStream5(),                 "62. Верховой огонь",  26, 190,  15,  0, "Скорость", "Шлейф") \
  X(EFF_SNAKE,           MultipleStream8(),                 "63. Радужный змей",  12, 178,   1,  0, "Скорость", "Цвет") \
  X(EFF_SPARKLES,        sparklesRoutine(),                 "64. Конфетти",  16, 142,  63,  0, "Скорость", "Количество") \
  X(EFF_TWINKLES,        twinklesRoutine(),                 "65. Мерцание",  25, 236,   4,  0, "Скорость", "Палитра и плотность") \
  X(EFF_SMOKE,           MultipleStreamSmoke(false),        "66. Дым",   9, 157, 100,  0, "Скорость", "Цвет") \
  X(EFF_SMOKE_COLOR,     MultipleStreamSmoke(true),         "67. Разноцветный дым",   9, 157,  30,  0, "Скорость", "Период смены цвета") \
  X(EFF_PICASSO,         picassoSelector(),                 "68. Пикассо",   9, 189,  43,  0, "Скорость", "Вариант и количество") \
  X(EFF_WAVES,           WaveRoutine(),                     "69. Волны",   9, 236,  80,  0, "Скорость", "Палитра и направление") \
  X(EFF_SAND,            sandRoutine(),                     "70. Цветные драже",   9, 195,  80,  0, "Скорость", "Насыщенность") \
  X(EFF_RINGS,           ringsRoutine(),                    "71. Кодовый замок",  10, 222,  92,  0, "Скорость", "Палитра и толщина колец") \
  X(EFF_CUBE2D,          cube2dRoutine(),                   "72. Кубик Рубика",  10, 231,  89,  0, "Скорость", "Палитра и размер ячеек") \
  X(EFF_SIMPLE_RAIN,     simpleRain(),                      "73. Тучка в банке",  30, 233,   2,  0, "Скорость", "Плотность дождя") \
  X(EFF_STORMY_RAIN,     stormyRain(),                      "74. Гроза в банке",  20, 236,  25,  0, "Скорость", "Плотность дождя") \
  X(EFF_COLOR_RAIN,      coloredRain(),                     "75. Осадки",  15, 198,  99,  0, "Скорость", "Цвет и длина капель") \
  X(EFF_RAIN,            RainRoutine(),                     "76. Разноцветный дождь",  15, 225,   1,  0, "Скорость", "Цвет") \
  X(EFF_SNOW,            snowRoutine(),                     "77. Снегопад",   9, 180,  90,  0, "Скорость", "Плотность") \
  X(EFF_STARFALL,        stormRoutine2(),                   "78. Звездопад / Метель",  20, 199,  54,  0, "Скорость", "Насыщенность и шлейф") \
  X(EFF_LEAPERS,         LeapersRoutine(),                  "79. Прыгуны",  24, 203,   5,  0, "Скорость", "Палитра и количество") \
  X(EFF_LIGHTERS,        lightersRoutine(),                 "80. Светлячки",  15, 157,  23,  0, "Скорость", "Количество") \
  X(EFF_LIGHTER_TRACES,  ballsRoutine(),                    "81. Светлячки со шлейфом",  21, 198,  93,  0, "Скорость", "Цвет") \
  X(EFF_LUMENJER,        lumenjerRoutine(),                 "82. Люменьер",  14, 223,  40,  0, "Скорость", "Палитра") \
  X(EFF_PAINTBALL,       lightBallsRoutine(),               "83. Пейнтбол",  11, 236,   7,  0, "Скорость", "Замедление") \
  X(EFF_RAINBOW_VER,     rainbowRoutine(),                  "84. Радуга",   8, 196,  56,  0, "Скорость", "Направление и закрутка") \
  X(EFF_CLOCK,           clockRoutine(),                    "85. Часы",   4,   5, 100,  0, "Положение цифр", "Цвет") \
  X(EFF_TEXT,            text_running(),                    "86. Бегущая строка",  10, 157,  38,  0, "Скорость", "Цвет") \
  X(EFF_SNAKE_GAME,      snakeGameRoutine(),                "87. Змейка",  14, 190,  30,  0, "Скорость", "Цвет") \
  X(EFF_EARTH,           earthRoutine(),                    "88. Земля",  14, 150,  60, 40, "Скорость вращения", "Яркость ночной стороны") \
  X(EFF_MARIO,           marioRoutine(),                    "89. Марио",  14, 150,  50, 40, "Скорость", "Положение героя") \
  X(EFF_PINGPONG,        pingPongRoutine(),                 "90. Пинг-понг",  14, 120,  30, 20, "Скорость", "Цвет") \
  X(EFF_DDP,             ddpRoutine(),                      "91. Кадры с компьютера", 255, 128, 50,  1, "", "")

#define EFFECT_ENUM(id, call, name, bri, spd, sca, frame, spdLabel, scaLabel)     id,
#define EFFECT_DEFAULTS(id, call, name, bri, spd, sca, frame, spdLabel, scaLabel) {bri, spd, sca},
#define EFFECT_NAME(id, call, name, bri, spd, sca, frame, spdLabel, scaLabel)     ";" name
#define EFFECT_FRAME(id, call, name, bri, spd, sca, frame, spdLabel, scaLabel)    frame,
#define EFFECT_SPEED(id, call, name, bri, spd, sca, frame, spdLabel, scaLabel)    ";" spdLabel
#define EFFECT_SCALE(id, call, name, bri, spd, sca, frame, spdLabel, scaLabel)    ";" scaLabel
#define EFFECT_CASE(id, call, name, bri, spd, sca, frame, spdLabel, scaLabel)     case id: call; break;

enum : uint8_t
{
  EFFECT_LIST(EFFECT_ENUM)
  MODE_AMOUNT
};

// яркость, Скорость, Масштаб по умолчанию
static const uint8_t defaultSettings[][3] PROGMEM = {
  EFFECT_LIST(EFFECT_DEFAULTS)
};

static const uint8_t effectFrameMs[] PROGMEM = {
  EFFECT_LIST(EFFECT_FRAME)
};

// списки через ';' с ведущим разделителем: так каждая строка списка одинаково начинается с ";"
static const char effectNamesRaw[] PROGMEM = EFFECT_LIST(EFFECT_NAME);
static const char effectSpeedLabelsRaw[] PROGMEM = EFFECT_LIST(EFFECT_SPEED);
static const char effectScaleLabelsRaw[] PROGMEM = EFFECT_LIST(EFFECT_SCALE);
#define effectNamesList (effectNamesRaw + 1)                // для виджета выбора эффекта: "0. Белый свет;1. Цвет;..."

// элемент списка через ';' из PROGMEM по номеру
inline String effectListItem(const char* list, uint8_t index)
{
  if (index >= MODE_AMOUNT)
  {
    return String();
  }
  uint16_t i = 0U;
  for (uint8_t skip = index + 1U; skip; i++)                // пропустить index + 1 разделителей, первый - ведущий
  {
    if (pgm_read_byte(&list[i]) == ';')
    {
      skip--;
    }
  }
  uint16_t end = i;
  while (pgm_read_byte(&list[end]) != ';' && pgm_read_byte(&list[end]) != '\0')
  {
    end++;
  }
  char buf[64];                                             // кириллица в UTF-8 - 2 байта на символ
  uint8_t len = min((int)(sizeof(buf) - 1), (int)(end - i));
  memcpy_P(buf, &list[i], len);
  buf[len] = '\0';
  return String(buf);
}

inline String getEffectName(uint8_t effectId)
{
  return effectListItem(effectNamesRaw, effectId);
}

inline String effectSpeedLabel(uint8_t effectId)
{
  return effectListItem(effectSpeedLabelsRaw, effectId);
}

inline String effectScaleLabel(uint8_t effectId)
{
  return effectListItem(effectScaleLabelsRaw, effectId);
}

// период кадра эффекта, мс
inline uint16_t effectFramePeriod(uint8_t effectId, uint8_t speed)
{
  uint8_t frame = pgm_read_byte(&effectFrameMs[effectId]);
  return frame ? frame : 256U - speed;
}
