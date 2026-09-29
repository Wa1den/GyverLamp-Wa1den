#pragma once

// Время лампы. Системные часы ядра ESP8266 идут в UTC и синхронизируются встроенным клиентом SNTP
// из lwIP: запрос имени сервера и ожидание ответа идут в фоне, loop не останавливается. Местное
// время получается из часового пояса в формате POSIX, который собирается из настроек «Часовой пояс»
// и «Переход на летнее время». Ручная установка на странице настроек записывает те же часы через
// settimeofday; следующая синхронизация по NTP её поправит, а без интернета она остаётся.
//
// Заголовок, а не .ino: переопределение sntp_update_delay_MS_rfc_not_less_than_15000 должно быть
// extern "C", а для .ino Arduino генерирует свои объявления функций.

#include <time.h>
#include <sys/time.h>
#include <coredecls.h>                                      // settimeofday_cb

#define NTP_NO_ANSWER_MS    (60000UL)                       // через сколько после подключения к роутеру без ответа сервера это пишется в Журнал

enum class ClockSource : uint8_t
{
  None,                                                     // время не известно, лампа считает секунды от старта
  Ntp,
  Manual
};

static ClockSource clockSource = ClockSource::None;
static volatile bool clockJustSet = false;                  // часы установлены: SNTP сообщает об этом из контекста lwIP, Журнал пишется в loop
static volatile bool clockSetBySntp = false;
static String clockServerName;                              // SNTP хранит указатель на имя сервера, поэтому строка живёт всё время работы
static uint32_t clockWaitFrom = 0U;                         // с какого момента ждём ответа сервера: подключение к роутеру или синхронизация по кнопке
static bool clockWaitReported = true;
static uint16_t clockSetCount = 0U;
static bool clockForced = false;                            // идёт синхронизация по кнопке: о её результате сообщается на странице
const char* clockNotice = nullptr;                          // сообщение для страницы настроек, забирает settingsTick                        // сколько раз устанавливались часы: по нему страница настроек обновляет поля времени

// часовые пояса для выбора в веб-интерфейсе: смещение от UTC в минутах
static const int16_t timezoneOffsets[] PROGMEM = {
  -720, -660, -600, -570, -540, -480, -420, -360, -300, -240, -210, -180, -120, -60,
  0, 60, 120, 180, 210, 240, 270, 300, 330, 345, 360, 390, 420, 480, 525, 540, 570,
  600, 630, 660, 720, 765, 780, 840
};
#define TIMEZONE_COUNT        (sizeof(timezoneOffsets) / sizeof(timezoneOffsets[0]))

// список поясов строкой для виджета выбора: "UTC-12;...;UTC+5:30;..."
String timezoneList()
{
  String list;
  for (uint8_t i = 0U; i < TIMEZONE_COUNT; i++)
  {
    int16_t offset = (int16_t)pgm_read_word(&timezoneOffsets[i]);
    uint16_t absOffset = abs(offset);
    char item[12];
    if (absOffset % 60U)
    {
      sprintf_P(item, PSTR("UTC%c%u:%02u"), offset < 0 ? '-' : '+', absOffset / 60U, absOffset % 60U);
    }
    else
    {
      sprintf_P(item, PSTR("UTC%c%u"), offset < 0 ? '-' : '+', absOffset / 60U);
    }
    if (i)
    {
      list += ';';
    }
    list += item;
  }
  return list;
}

// номер пояса в списке по смещению; смещение не из списка (файл настроек испорчен) даёт UTC+0
uint8_t timezoneIndex(int16_t offset)
{
  for (uint8_t i = 0U; i < TIMEZONE_COUNT; i++)
  {
    if ((int16_t)pgm_read_word(&timezoneOffsets[i]) == offset)
    {
      return i;
    }
  }
  return timezoneIndex(0);
}

int16_t timezoneOffset(uint8_t index)
{
  return (int16_t)pgm_read_word(&timezoneOffsets[index < TIMEZONE_COUNT ? index : timezoneIndex(0)]);
}

extern "C" uint32_t sntp_update_delay_MS_rfc_not_less_than_15000()
{
  return NTP_INTERVAL;
}

// часовой пояс POSIX. В POSIX смещение пишется к западу от UTC, поэтому знак обратный: UTC+3 - "STD-3".
// Час перехода на летнее время задаётся по местному времени до перехода
static void clockAppendOffset(String& zone, int16_t minutesWest)
{
  zone += minutesWest < 0 ? '-' : '+';
  uint16_t value = abs(minutesWest);
  zone += value / 60U;
  if (value % 60U)
  {
    char buf[4];
    sprintf_P(buf, PSTR(":%02u"), value % 60U);
    zone += buf;
  }
}

static void clockAppendRuleTime(String& zone, int16_t minutes)
{
  minutes = constrain(minutes, 0, 24 * 60);
  zone += '/';
  zone += minutes / 60;
  if (minutes % 60)
  {
    char buf[4];
    sprintf_P(buf, PSTR(":%02u"), (uint16_t)(minutes % 60));
    zone += buf;
  }
}

String clockZone()
{
  int16_t offset = timezoneOffset(timezoneIndex(db[kk::tz_offset].toInt()));
  String zone = F("STD");
  clockAppendOffset(zone, -offset);

  switch ((uint8_t)db[kk::tz_dst])
  {
    case 1U:                                                // Европа: последние воскресенья марта и октября, в 01:00 UTC
      zone += F("DST,M3.5.0");
      clockAppendRuleTime(zone, 60 + offset);
      zone += F(",M10.5.0");
      clockAppendRuleTime(zone, 120 + offset);
      break;
    case 2U:                                                // США и Канада: второе воскресенье марта и первое ноября, в 02:00 местного времени
      zone += F("DST,M3.2.0/2,M11.1.0/2");
      break;
  }
  return zone;
}

// часовой пояс и сервер из настроек; запускает синхронизацию заново
void clockApply()
{
  clockServerName = (String)db[kk::ntp_host];
  if (!clockServerName.length())
  {
    clockServerName = NTP_ADDRESS;
  }
  configTime(clockZone().c_str(), clockServerName.c_str());
}

static void clockOnSet(bool fromSntp)
{
  clockSetBySntp = fromSntp;
  clockJustSet = true;
}

void clockSetup()
{
  settimeofday_cb(clockOnSet);
  clockApply();
}

// ручная установка времени, unix-время по UTC
void clockSetManual(uint32_t utc)
{
  timeval tv = { (time_t)utc, 0 };
  settimeofday(&tv, nullptr);
}

// синхронизация по кнопке на странице настроек: сервер мог смениться, поэтому настройки применяются заново
void clockForceSync()
{
  clockApply();
  uiLog.printf_P(PSTR("NTP: синхронизация с %s...\n"), clockServerName.c_str());
  clockWaitFrom = millis();
  clockWaitReported = false;
  clockForced = true;
}

// в loop: Журнал и признак синхронизации
void clockTick()
{
  if (clockJustSet)
  {
    clockJustSet = false;
    clockSetCount++;
    bool report = !timeSynched || clockSource != ClockSource::Ntp || !clockWaitReported; // первая синхронизация, после ручной установки или по кнопке
    timeSynched = true;
    clockWaitReported = true;
    if (clockSetBySntp)
    {
      if (report)
      {
        uiLog.println(F("NTP: время синхронизировано"));
      }
      clockSource = ClockSource::Ntp;
      if (clockForced)
      {
        clockForced = false;
        clockNotice = "Время синхронизировано";
      }
    }
    else
    {
      clockSource = ClockSource::Manual;
    }
    #ifdef WARNING_IF_NO_TIME
    noTimeClear();
    #endif
  }

  static bool wasConnected = false;
  bool connected = WiFiConnector.connected();
  if (connected && !wasConnected && clockSource != ClockSource::Ntp)
  {
    clockWaitFrom = millis();
    clockWaitReported = false;
  }
  wasConnected = connected;

  if (!clockWaitReported && connected && millis() - clockWaitFrom >= NTP_NO_ANSWER_MS)
  {
    clockWaitReported = true;
    uiLog.printf_P(PSTR("NTP: сервер %s не ответил за минуту\n"), clockServerName.c_str());
    if (clockForced)
    {
      clockForced = false;
      clockNotice = "Сервер времени не ответил за минуту";
    }
  }
}

const char* clockSyncState()
{
  switch (clockSource)
  {
    case ClockSource::Ntp:    return "выполнена (NTP)";
    case ClockSource::Manual: return "выполнена (вручную)";
    default:                  return "не выполнена";
  }
}

// местное время в секундах от 1970 года - в этом виде его считают TimeLib (hour, minute...) и будильники;
// пока время не известно - секунды от старта лампы
time_t getCurrentLocalTime()
{
  if (!timeSynched)
  {
    return millis() / 1000UL;
  }
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  tmElements_t e;
  e.Second = local.tm_sec;
  e.Minute = local.tm_min;
  e.Hour = local.tm_hour;
  e.Wday = local.tm_wday + 1;
  e.Day = local.tm_mday;
  e.Month = local.tm_mon + 1;
  e.Year = local.tm_year + 1900 - 1970;
  return makeTime(e);
}

time_t getCurrentUtcTime()
{
  return timeSynched ? time(nullptr) : (time_t)(millis() / 1000UL);
}

void getFormattedTime(char* buf)
{
  time_t t = getCurrentLocalTime();
  sprintf_P(buf, PSTR("%02u:%02u:%02u"), hour(t), minute(t), second(t));
}
