// GyverLamp-Wa1den - прошивка лампы на ESP8266: https://github.com/Wa1den/GyverLamp-Wa1den
// Основана на прошивке Gunner47 v2.87in1 (https://github.com/SottNick/GyverLamp), та - на GyverLamp
// AlexGyver (https://alexgyver.ru/GyverLamp/, автор идеи и первой реализации, 2019). Авторы эффектов
// указаны у самих эффектов в effects.ino. Описание, сборка и история версий - в README.md и релизах.
//
// Настройки железа - в Config.h, поведения - в Constants.h, остальное задаётся на странице настроек.

#define FASTLED_USE_PROGMEM 1 // просим библиотеку FASTLED экономить память контроллера на свои палитры

#include "pgmspace.h"
#include "Constants.h"
#include <FastLED.h>
#include <NeoPixelBus.h>                                    // вывод кадров на ленту через аппаратный UART1 (см. ledsShow в utility.ino)
#include <ESP8266WiFi.h>
#include <WiFiConnector.h>                                  // подключение к WiFi сети с fallback в режим точки доступа
#define GS_CLIENT_TOUT (300U)                               // сколько синхронный вебсервер ждёт запрос от подключившегося клиента.
                                                            // всё это время стоит весь скетч (анимация, MQTT, кнопка), а браузеры открывают
                                                            // соединения "про запас" и не шлют по ним запрос - со штатными 1500 мс лампа
                                                            // заметно подвисала при каждом открытии страницы настроек
#include <SettingsGyverWS.h>                                // веб-интерфейс настроек (синхронный GyverHTTP + WebSocket);
                                                            // синхронный вариант выбран сознательно: все действия страницы обрабатываются
                                                            // в loop(), а не в контексте асинхронного TCP - у того маленький системный стек,
                                                            // и тяжёлые обработчики приводят к самопроизвольным перезагрузкам
// класс журнала TimedLogger и объект uiLog объявлены ниже, после подключения TimeLib
#include <WiFiUdp.h>
#include "Types.h"

void ledsShow();                                            // вывод кадра на ленту (utility.ino); прототипы объявлены до заголовков (TimerManager.h и др.), которые их вызывают
void ledsClear();                                           // очистка кадра

#include "timerMinim.h"
#ifdef ESP_USE_BUTTON
#include <GyverButton.h>
#endif
#include "fonts.h"
#ifdef USE_NTP
#include <NTPClient.h>
#endif
#if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
#include <Timezone.h>
#endif
#include <TimeLib.h>

extern bool timeSynched;                                    // определены ниже/в time.ino; нужны классу журнала
time_t getCurrentLocalTime();

void crashTailPut(char c);                                  // след журнала для разбора перезагрузок, определён ниже

// Журнал событий для веб-интерфейса: кольцевой буфер, в начало каждой строки
// автоматически добавляется временная метка - [ДД.ММ ЧЧ:ММ:СС] при
// синхронизированном времени, иначе [ЧЧ:ММ:СС] от старта лампы.
class TimedLogger : public sets::Logger
{
  public:
    using sets::Logger::Logger;

    size_t write(uint8_t v) override
    {
      if (_lineStart && v != '\n' && v != '\r')
      {
        _lineStart = false;
        char stamp[18];
        if (timeSynched)
        {
          time_t t = getCurrentLocalTime();
          snprintf_P(stamp, sizeof(stamp), PSTR("[%02u.%02u %02u:%02u:%02u] "),
                     (uint8_t)day(t), (uint8_t)month(t), (uint8_t)hour(t), (uint8_t)minute(t), (uint8_t)second(t));
        }
        else
        {
          uint32_t s = millis() / 1000UL;
          snprintf_P(stamp, sizeof(stamp), PSTR("[%02u:%02u:%02u] "),
                     (uint8_t)(s / 3600UL % 24UL), (uint8_t)(s / 60UL % 60UL), (uint8_t)(s % 60UL));
        }
        for (const char* p = stamp; *p != '\0'; p++)
        {
          crashTailPut(*p);
          sets::Logger::write(*p);
        }
      }
      if (v == '\n')
      {
        _lineStart = true;
      }
      crashTailPut(v);
      return sets::Logger::write(v);
    }

  private:
    bool _lineStart = true;
};

TimedLogger uiLog(1200);                                    // объявлен до MqttManager.h, который в него пишет

// След для разбора перезагрузок по сторожевому таймеру. Журнал uiLog живёт в оперативной памяти и
// при сбросе теряется, а RTC-память переживает любой сброс, кроме отключения питания. Поэтому в неё
// пишутся последняя завершённая стадия основного цикла, эффект, время работы и хвост журнала, а
// после сброса по сторожу или исключению они выводятся в журнал. Первые 32 слова пользовательской
// RTC-памяти занимает команда загрузчика при обновлении по воздуху, след лежит за ними.
// RTC-память доступна только целыми словами, поэтому хвост копится в RAM и переносится в конце строки.
#define CRASH_TRACE_ADDR    (0x60001200UL + 40U * 4U)
#define CRASH_TRACE_MAGIC   (0x4C414D50UL)
#define CRASH_TAIL_WORDS    (64U)

struct CrashTrace
{
  uint32_t magic;
  uint32_t build;                                           // хэш даты и времени сборки: указатель на имя стадии верен только в той же прошивке
  uint32_t stage;                                           // указатель на имя последней завершённой стадии цикла
  uint32_t uptime;                                          // millis() на момент этой стадии
  uint32_t state;                                           // номер эффекта, в старшем байте - включена ли лампа
  uint32_t head;                                            // позиция записи в кольце хвоста
  uint32_t tail[CRASH_TAIL_WORDS];
};

static volatile CrashTrace* const crashTrace = (volatile CrashTrace*)CRASH_TRACE_ADDR;
static char crashTail[CRASH_TAIL_WORDS * 4U] __attribute__((aligned(4))); // хвост журнала в RAM
static uint16_t crashTailHead = 0U;
static bool crashTraceReady = false;                        // след прошлого запуска уже прочитан, можно писать новый
static CrashTrace crashPrev;                                // след прошлого запуска
static bool crashPrevValid = false;

static uint32_t crashBuildHash()
{
  uint32_t hash = 2166136261UL;                             // FNV-1a
  for (const char* p = __DATE__ " " __TIME__; *p; p++)
  {
    hash = (hash ^ (uint8_t)*p) * 16777619UL;
  }
  return hash;
}

void crashTailPut(char c)
{
  if (!crashTraceReady)
  {
    return;
  }
  crashTail[crashTailHead] = c;
  crashTailHead = (crashTailHead + 1U) % sizeof(crashTail);
  if (c == '\n')                                            // строка закончилась - хвост переносится в RTC
  {
    const uint32_t* words = (const uint32_t*)crashTail;
    for (uint8_t i = 0U; i < CRASH_TAIL_WORDS; i++)
    {
      crashTrace->tail[i] = words[i];
    }
    crashTrace->head = crashTailHead;
  }
}

// вызывается в каждой точке LOOP_STAGE: три записи слова в RTC на стадию
void crashTraceStage(const char* name, uint8_t mode, bool on)
{
  crashTrace->stage = (uint32_t)name;
  crashTrace->uptime = millis();
  crashTrace->state = mode | ((uint32_t)on << 24);
}

// в самом начале setup: забрать след прошлого запуска и начать новый
void crashTraceLoad()
{
  crashPrevValid = crashTrace->magic == CRASH_TRACE_MAGIC;
  if (crashPrevValid)
  {
    crashPrev.build = crashTrace->build;
    crashPrev.stage = crashTrace->stage;
    crashPrev.uptime = crashTrace->uptime;
    crashPrev.state = crashTrace->state;
    crashPrev.head = crashTrace->head;
    for (uint8_t i = 0U; i < CRASH_TAIL_WORDS; i++)
    {
      crashPrev.tail[i] = crashTrace->tail[i];
    }
  }
  crashTrace->magic = CRASH_TRACE_MAGIC;
  crashTrace->build = crashBuildHash();
  crashTrace->stage = 0U;
  crashTrace->uptime = 0U;
  crashTrace->state = 0U;
  crashTrace->head = 0U;
  for (uint8_t i = 0U; i < CRASH_TAIL_WORDS; i++)
  {
    crashTrace->tail[i] = 0U;
  }
  crashTraceReady = true;
}

// после строки "Старт": если лампа перезагрузилась по сторожу или исключению, вывести след в журнал
void crashTraceReport()
{
  const rst_info* info = ESP.getResetInfoPtr();
  if (!crashPrevValid || (info->reason != REASON_WDT_RST && info->reason != REASON_EXCEPTION_RST && info->reason != REASON_SOFT_WDT_RST))
  {
    return;
  }

  if (info->reason == REASON_EXCEPTION_RST)
  {
    uiLog.printf_P(PSTR("Исключение %u по адресу 0x%08x\n"), info->exccause, info->epc1);
  }
  uiLog.printf_P(PSTR("До сброса: работала %u с, эффект %u, лампа %s"), crashPrev.uptime / 1000U,
                 crashPrev.state & 0xFFU, (crashPrev.state >> 24) ? "включена" : "выключена");
  if (crashPrev.build == crashBuildHash() && crashPrev.stage)
  {
    uiLog.printf_P(PSTR(", последняя завершённая стадия цикла: %s"), (const char*)crashPrev.stage);
  }
  uiLog.println();

  // хвост журнала прошлого запуска, начиная с первой целой строки
  const char* tail = (const char*)crashPrev.tail;
  const uint16_t size = CRASH_TAIL_WORDS * 4U;
  uint16_t i = 0U;
  while (i < size && tail[(crashPrev.head + i) % size] != '\n')
  {
    i++;
  }
  if (++i >= size)
  {
    return;
  }
  uiLog.println(F("Журнал до сброса:"));
  for (; i < size; i++)
  {
    char c = tail[(crashPrev.head + i) % size];
    if (c)
    {
      uiLog.write((uint8_t)c);
    }
  }
}

#ifdef OTA
#include "OtaManager.h"
#endif
#if USE_MQTT
#include "MqttManager.h"
#endif
#include "TimerManager.h"
#include "Storage.h"                                        // хранилище настроек на LittleFS/GyverDB
#include "FavoritesManager.h"


// --- ИНИЦИАЛИЗАЦИЯ ОБЪЕКТОВ ----------
CRGB leds[NUM_LEDS];
#if (LED_PIN != 2U)
#error "Вывод на ленту идёт через аппаратный UART1, его TX жёстко закреплён за GPIO2 (D4). Для другого пина нужен другой метод NeoPixelBus."
#endif
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Ws2812xMethod> ledStrip(NUM_LEDS); // аппаратный вывод на ленту: UART1 TX = GPIO2 = LED_PIN

// Порядок цветов ленты. NeoGrbFeature отправляет по проводу байты (G, R, B) из переданного RgbColor(R, G, B),
// поэтому ledsShow переставляет каналы так, чтобы на провод ушёл порядок COLOR_ORDER. Цифры восьмеричного
// значения FastLED EOrder - номера каналов (0 - R, 1 - G, 2 - B) в порядке отправки: GRB = 0102
#define COLOR_WIRE_0  ((COLOR_ORDER >> 6) & 0x07)
#define COLOR_WIRE_1  ((COLOR_ORDER >> 3) & 0x07)
#define COLOR_WIRE_2  (COLOR_ORDER & 0x07)

#ifdef USE_NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0, NTP_INTERVAL); // объект, запрашивающий время с ntp сервера; в нём смещение часового пояса не используется (перенесено в объект localTimeZone); здесь всегда должно быть время UTC
  #ifdef PHONE_N_MANUAL_TIME_PRIORITY
    bool stillUseNTP = true;
  #endif    
#endif

#if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
TimeChangeRule utcRule = { "", week_t::Last, dow_t::Sun, month_t::Mar, 1U, 0 };
Timezone localTimeZone(utcRule);                            // часовой пояс и летнее время задаются в веб-интерфейсе, правила ставит timezoneApply() при старте
#endif

timerMinim timeTimer(3000);
bool ntpServerAddressResolved = false;
bool timeSynched = false;
uint32_t lastTimePrinted = 0U;

#if defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
time_t manualTimeShift;
#endif

#ifdef GET_TIME_FROM_PHONE
time_t phoneTimeLastSync;
#endif

uint8_t selectedSettings = 0U;
#ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
uint8_t random_on = RANDOM_SETTINGS_IN_CYCLE_MODE;
#endif //RANDOM_SETTINGS_IN_CYCLE_MODE
#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
uint8_t button_sleep_time = 1U;
#endif //#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)

#ifdef ESP_USE_BUTTON
#if (BUTTON_IS_SENSORY == 1)
GButton touch(BTN_PIN, LOW_PULL, NORM_OPEN);  // для сенсорной кнопки LOW_PULL
#else
GButton touch(BTN_PIN, HIGH_PULL, NORM_OPEN); // для физической (не сенсорной) кнопки HIGH_PULL. ну и кнопку нужно ставить без резистора в разрыв между пинами D2 и GND
#endif
#endif //ESP_USE_BUTTON

#ifdef OTA
OtaManager otaManager(&showWarning);
OtaPhase OtaManager::OtaFlag = OtaPhase::None;
#endif

// статические члены MqttManager определены в самом MqttManager.h (inline static)

// --- ИНИЦИАЛИЗАЦИЯ ПЕРЕМЕННЫХ -------
static const uint8_t maxDim = max(WIDTH, HEIGHT);

ModeType modes[MODE_AMOUNT];
AlarmType alarms[7];

static const uint8_t dawnOffsets[] PROGMEM = {5, 10, 15, 20, 25, 30, 40, 50, 60};   // опции для выпадающего списка параметра "время перед 'рассветом'" (будильник); синхронизировано с android приложением
uint8_t dawnMode;
bool dawnFlag = false;
uint32_t thisTime;
bool manualOff = false;

uint8_t currentMode = 0;
bool loadingFlag = true;
bool ONflag = false;
uint32_t eepromTimeout;
bool settChanged = false;
bool buttonEnabled = true; // это важное первоначальное значение. нельзя делать false по умолчанию
bool needResetWifiOnStart = false;                          // запрошен сброс настроек WiFi при старте с зажатой кнопкой (см. ESP_RESET_ON_START)
bool pendingWifiConnect = false;                            // запрошено подключение к WiFi сети из веб-интерфейса (кнопка "Подключить"); обрабатывается в wifiTick()

unsigned char matrixValue[8][16];                           // буфер эффекта Огонь

bool TimerManager::TimerRunning = false;
bool TimerManager::TimerHasFired = false;
uint8_t TimerManager::TimerOption = 1U;
uint32_t TimerManager::TimeToFire = 0U;

uint8_t FavoritesManager::FavoritesRunning = 0;
uint16_t FavoritesManager::Interval = DEFAULT_FAVORITES_INTERVAL;
uint16_t FavoritesManager::Dispersion = DEFAULT_FAVORITES_DISPERSION;
uint8_t FavoritesManager::UseSavedFavoritesRunning = 0;
uint8_t FavoritesManager::FavoriteModes[MODE_AMOUNT] = {0};
uint32_t FavoritesManager::nextModeAt = 0UL;

char TextTicker[CMD_BUFFER_SIZE + 1];                   // текст эффекта Бегущая строка
bool pendingRestart = false;                                // запрошена перезагрузка из веб-интерфейса (выполняется из loop, не из контекста вебсервера)
bool pendingWifiReset = false;                              // запрошен сброс настроек WiFi из веб-интерфейса (выполняется из loop)
bool pendingWolWake = false;                                // запрошено пробуждение компьютера Wake-on-LAN из веб-интерфейса (выполняется из loop)
bool pendingWolResub = false;                               // изменены настройки дополнительного WOL-топика - нужно обновить MQTT-подписку
bool pendingShowIp = false;                                 // подключились к новой WiFi сети - показать IP бегущей строкой (обрабатывается в loop)
uint32_t buttonFeedbackAt = 0U;                             // момент последнего касания кнопки (для световой волны-отклика, см. ledsShow)
#ifdef USE_NTP
bool pendingNtpSync = false;                                // запрошена принудительная синхронизация времени из веб-интерфейса (выполняется из loop)
String ntpServerName;                                       // адрес NTP сервера из хранилища настроек; NTPClient хранит указатель, поэтому строка должна жить всё время работы
#endif //USE_NTP

void setup()
{
  Serial.begin(115200);
  Serial.println();
  ESP.wdtEnable(WDTO_8S);
  crashTraceLoad();                                         // до первой записи в журнал, иначе след прошлого запуска затрётся


  // ПИНЫ
  #ifdef MOSFET_PIN                                         // инициализация пина, управляющего MOSFET транзистором в состояние "выключен"
  pinMode(MOSFET_PIN, OUTPUT);
  #ifdef MOSFET_LEVEL
  digitalWrite(MOSFET_PIN, !MOSFET_LEVEL);
  #endif
  #endif

  #ifdef ALARM_PIN                                          // инициализация пина, управляющего будильником в состояние "выключен"
  pinMode(ALARM_PIN, OUTPUT);
  #ifdef ALARM_LEVEL
  digitalWrite(ALARM_PIN, !ALARM_LEVEL);
  #endif
  #endif


  // TELNET
  #if defined(GENERAL_DEBUG) && GENERAL_DEBUG_TELNET
  telnetServer.begin();
  for (uint8_t i = 0; i < 100; i++)                         // пауза 10 секунд в отладочном режиме, чтобы успеть подключиться по протоколу telnet до вывода первых сообщений
  {
    handleTelnetClient();
    delay(100);
    ESP.wdtFeed();
  }
  #endif


  // КНОПКА
  #if defined(ESP_USE_BUTTON)
  touch.setStepTimeout(BUTTON_STEP_TIMEOUT);
  touch.setClickTimeout(BUTTON_CLICK_TIMEOUT);
  touch.setDebounce(BUTTON_SET_DEBOUNCE);
    #if ESP_RESET_ON_START
    delay(1000);                                            // ожидание инициализации модуля кнопки ttp223 (по спецификации 250мс)
    if (digitalRead(BTN_PIN))
    {
      needResetWifiOnStart = true;                          // сброс SSID и пароля выполнится после инициализации хранилища настроек (ниже в setup)
      LOG.println(F("Запрошен сброс настроек WiFi (старт с зажатой кнопкой)"));
    }
    ESP.wdtFeed();
    #elif defined(BUTTON_LOCK_ON_START) && (BUTTON_IS_SENSORY == 1) // с механическими кнопками надо считывать инвертированный сигнал, но смысла нет
    delay(1000);                                            // ожидание инициализации модуля кнопки ttp223 (по спецификации 250мс)
    if (digitalRead(BTN_PIN))
      buttonEnabled = false;
    ESP.wdtFeed();
    #endif
  #endif


  // ЛЕНТА/МАТРИЦА
  ledStrip.Begin();                                         // вывод на ленту через аппаратный UART1 (GPIO2); FastLED остаётся для математики эффектов
  FastLED.setBrightness(BRIGHTNESS);                        // глобальная яркость хранится в FastLED и применяется в ledsShow (вместе с лимитом по току CURRENT_LIMIT)
  ledsClear();
  ledsShow();

#ifdef USE_SHUFFLE_FAVORITES // первоначальная очередь избранного до перемешивания
    for (uint8_t i = 0; i < MODE_AMOUNT; i++)
      shuffleFavoriteModes[i] = i;
#endif

  // ХРАНИЛИЩЕ НАСТРОЕК (LittleFS + GyverDB)
  Storage::InitSettings(                                    // чтение базы настроек из файла; запись начального состояния настроек, если их там ещё нет; инициализация настроек лампы значениями из базы
    modes, alarms, &espMode, &ONflag, &dawnMode, &currentMode, &buttonEnabled,
    #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
    &random_on,
    #endif //ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
    #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
    &button_sleep_time,
    #endif //#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
    &(FavoritesManager::ReadFavoritesFromStorage),
    &(FavoritesManager::SaveFavoritesToStorage),
    &(restoreSettings)); // восстановление настроек эффектов по умолчанию выполняется в обработчике инициализации Storage
  LOG.printf_P(PSTR("Рабочий режим лампы: ESP_MODE = %d\n"), espMode);
  #if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
  timezoneApply();                                          // часовой пояс и летнее время из настроек
  #endif

  if (needResetWifiOnStart)                                 // сброс сохранённых SSID и пароля при старте с зажатой кнопкой, если разрешено (ESP_RESET_ON_START)
  {
    resetWifiSettings();
    Storage::SaveButtonEnabled(&buttonEnabled);
    LOG.println(F("Настройки WiFi сброшены"));
  }
  ESP.wdtFeed();


  // WI-FI + ВЕБ-ИНТЕРФЕЙС НАСТРОЕК
  #ifdef WARNING_IF_NO_TIME
  noTimeWarningShow();
  #endif
  wifiSetup();                                              // запуск подключения к WiFi сети или точки доступа (не блокирует выполнение, обслуживается wifiTick() в loop)
  settingsSetup();                                          // запуск веб-интерфейса настроек (доступен по IP лампы)

  ESP.wdtFeed();


  // NTP
  #ifdef USE_NTP
  ntpServerName = (String)db[kk::ntp_host];                 // адрес NTP сервера из хранилища настроек (по умолчанию NTP_ADDRESS)
  if (!ntpServerName.length())
  {
    ntpServerName = NTP_ADDRESS;
  }
  timeClient.begin();                                       // имя сервера в NTPClient не передаём: после резолва ему отдаётся уже IP
                                                            // (иначе он резолвит имя при каждой отправке пакета и блокирует loop на 10 сек)
  ESP.wdtFeed();
  #endif


  // MQTT
  #if (USE_MQTT)
  if (espMode == 1U)
  {
    MqttManager::setup();                                   // чтение параметров брокера из хранилища настроек, сборка топиков, создание клиента
  }
  ESP.wdtFeed();
  #endif


  // ОСТАЛЬНОЕ
  memset(matrixValue, 0, sizeof(matrixValue)); //это массив для эффекта Огонь. странно, что его нужно залить нулями
  randomSeed(micros());
  changePower();
  loadingFlag = true;

  ((String)db[kk::running_text]).toCharArray(TextTicker, sizeof(TextTicker)); // текст бегущей строки из хранилища настроек
  if (!strlen(TextTicker))
  {
    strcpy(TextTicker, RUNNING_TEXT_DEFAULT);
  }

  uiLog.printf_P(PSTR("Старт. Причина перезагрузки: %s\n"), ESP.getResetReason().c_str()); // в журнал веб-интерфейса; помогает заметить самопроизвольные перезагрузки
  crashTraceReport();
}


// Диагностика подвисаний: измеряет длительность каждой стадии основного цикла и пишет
// в Журнал те, что заняли больше LOOP_WATCHDOG_MS. Заодно показывает состояние кучи:
// свободно всего, самый большой непрерывный блок и процент фрагментации. Свободной памяти
// может быть много, но если крупный блок не выделяется, lwIP не может отправить пакет -
// и синхронный сервер зависает в ожидании отправки. Стадию "веб-интерфейс" разбирает
// более подробное профилирование в SettingsUI.ino (UI_PROFILE_MS)
#ifdef LOOP_WATCHDOG_MS
static uint32_t loopStageStart = 0U;
#define LOOP_STAGE(name)                                                                          do {                                                                                              uint32_t stageMs = millis() - loopStageStart;                                                   if (stageMs >= LOOP_WATCHDOG_MS)                                                                {                                                                                                 uiLog.printf_P(PSTR("Долгий цикл: %s %u мс (память %u, блок %u, фрагм %u%%)"), name, stageMs, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(), ESP.getHeapFragmentation());       uiLog.println();                                                                              }                                                                                               loopStageStart = millis();                                                                    crashTraceStage(name, currentMode, ONflag); } while (0)
#else
#define LOOP_STAGE(name) crashTraceStage(name, currentMode, ONflag)
#endif

void loop()
{
  #ifdef LOOP_WATCHDOG_MS
  loopStageStart = millis();
  #endif

  wifiTick();                                               // обслуживание WiFi подключения (WiFiConnector)
  LOOP_STAGE("wifi");
  settingsTick();                                           // обслуживание веб-интерфейса настроек
  LOOP_STAGE("веб-интерфейс");
  handlePendingActions();                                   // отложенные действия из веб-интерфейса (перезагрузка, сброс WiFi)
  ledsFeedbackTick();                                       // анимация отклика на касание кнопки (работает и на выключенной лампе)
  autoBrightnessTick();                                     // автояркость по датчику освещённости
  LOOP_STAGE("датчик света");

  effectsTick();
  LOOP_STAGE("эффект");

  Storage::HandleTick(&settChanged, &eepromTimeout, &ONflag,
    &currentMode, modes, &(FavoritesManager::SaveFavoritesToStorage));
  LOOP_STAGE("сохранение настроек");

  //#ifdef USE_NTP
  #if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
  //if (millis() > 30 * 1000U) можно попытаться оттянуть срок первой попытки синхронизации времени на 30 секунд, чтобы роутер успел не только загрузиться, но и соединиться с интернетом
    timeTick();
  #endif
  LOOP_STAGE("время/NTP");

  #ifdef ESP_USE_BUTTON
  //if (buttonEnabled) в процедуре ведь есть эта проверка
    buttonTick();
  #endif
  LOOP_STAGE("кнопка");

  #ifdef OTA
  otaManager.HandleOtaUpdate();                             // ожидание и обработка команды на обновление прошивки по воздуху
  #endif

  TimerManager::HandleTimer(&ONflag, &settChanged,          // обработка событий таймера отключения лампы
    &eepromTimeout, &changePower);

  if (!countdownActive() && !diceActive() &&               // во время обратного отсчёта и кубика Цикл эффект не переключает, иначе они прервались бы
      FavoritesManager::HandleFavorites(                    // обработка режима избранных эффектов
      &ONflag,
      &currentMode,
      &loadingFlag
      //#ifdef USE_NTP
      #if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
      , &dawnFlag
      #endif
      #ifdef RANDOM_SETTINGS_IN_CYCLE_MODE
      , &random_on
      , &selectedSettings
      #endif
      ))
  {
    FastLED.setBrightness(modes[currentMode].Brightness);
  }

  #if USE_MQTT
  MqttManager::tick();                                      // переподключение к брокеру, применение принятых команд, публикация состояния
  #endif
  LOOP_STAGE("mqtt");

  #if defined(GENERAL_DEBUG) && GENERAL_DEBUG_TELNET
  handleTelnetClient();
  #endif

  ESP.wdtFeed();
  
  #ifdef FIX_DEFECTIVE_BOARD
  delay(FIX_DEFECTIVE_BOARD);
  #endif
}

