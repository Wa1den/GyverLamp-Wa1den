#ifdef USE_NTP

#define RESOLVE_INTERVAL      (15UL * 1000UL)                             // интервал проверки подключения к интернету в миллисекундах (15 секунд)
                                                                          // при старте ESP пытается получить точное время от сервера времени в интрнете
                                                                          // эта попытка длится RESOLVE_TIMEOUT
                                                                          // если при этом отсутствует подключение к интернету (но есть WiFi подключение),
                                                                          // модуль будет подвисать на RESOLVE_TIMEOUT каждое срабатывание таймера, т.е., 1,5 секунды
                                                                          // чтобы избежать этого, будем пытаться узнать состояние подключения 1 раз в RESOLVE_INTERVAL (15 секунд)
                                                                          // попытки будут продолжаться до первой успешной синхронизации времени
                                                                          // до этого момента функции будильника работать не будут (или их можно ввести через USE_MANUAL_TIME_SETTING)
                                                                          // интервал последующих синхронизаций времени определяён в NTP_INTERVAL (30-60 минут)
                                                                          // при ошибках повторной синхронизации времени функции будильника отключаться не будут
#define RESOLVE_TIMEOUT       (1500UL)                                    // таймаут ожидания подключения к интернету в миллисекундах (1,5 секунды)
uint32_t lastResolveTryMoment = 0UL;
IPAddress ntpServerIp = {0, 0, 0, 0};
String ntpServerIpStr;                                                    // IP сервера времени строкой: NTPClient получает ЕГО вместо имени хоста.
                                                                          // иначе NTPClient при каждой отправке пакета делает DNS-запрос через
                                                                          // WiFiUdp::beginPacket(имя) -> WiFi.hostByName() с таймаутом 10 секунд,
                                                                          // который наглухо блокирует loop (лампа замирала на ~10 с каждые 15 с,
                                                                          // пока время не синхронизировано). hostByName со строкой-IP возвращается сразу
uint32_t ntpRetryInterval = RESOLVE_INTERVAL;                             // текущий интервал попыток синхронизации (растёт при неудачах)
#define NTP_RETRY_MAX         (10UL * 60UL * 1000UL)                      // максимальный интервал попыток (10 минут)
#define NTP_START_DELAY       (5000UL)                                    // пауза между подключением к роутеру и первой попыткой синхронизации, мс

// сброс паузы между попытками к обычной (вызывается при ручной синхронизации из веб-интерфейса;
// отдельная функция, т.к. LampControl.ino компилируется раньше time.ino и переменную оттуда не видит)
void ntpResetRetryInterval()
{
  ntpRetryInterval = RESOLVE_INTERVAL;
}

// неудачная попытка синхронизации: увеличиваем паузу до следующей вдвое (до NTP_RETRY_MAX),
// чтобы недоступный сервер времени не дёргал лампу секундными паузами каждые 15 секунд
void ntpRetryFailed(const __FlashStringHelper* reason)
{
  if (ntpRetryInterval < NTP_RETRY_MAX)
  {
    ntpRetryInterval = (ntpRetryInterval * 2UL > NTP_RETRY_MAX) ? NTP_RETRY_MAX : ntpRetryInterval * 2UL;
  }
  uiLog.printf_P(PSTR("NTP: %s, следующая попытка через %u с"), String(reason).c_str(), (uint16_t)(ntpRetryInterval / 1000UL));
  uiLog.println();
}

#endif

#if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)

static CRGB dawnColor[6];

static uint8_t dawnCounter = 0;                                           // счётчик первых 10 шагов будильника

void timeTick()
{
  {
    if (timeTimer.isReady())
    {
      #ifdef USE_NTP
if (espMode == 1U){      
      if (!timeSynched)
      {
        // до первой синхронизации пытаемся не чаще RESOLVE_INTERVAL (15с): и resolveNtpServerAddress
        // (блокирует до 1.5с при недоступном DNS), и timeClient.update() ниже (блокирует до 1с,
        // ожидая UDP-ответ, пока время не получено) иначе тормозили бы луп каждые 3с (timeTimer) -
        // отсюда сильные лаги анимации и веб-интерфейса первые секунды/минуты после загрузки,
        // пока роутер не отдаст DNS/NTP
        if (lastResolveTryMoment != 0 && millis() - lastResolveTryMoment < ntpRetryInterval)
        {
          return;
        }

        // первая попытка - только после подключения к роутеру и паузы NTP_START_DELAY: сразу после
        // старта сети ещё нет, и попытка через 3 с после загрузки всегда заканчивалась ошибкой DNS
        static uint32_t wifiUpAt = 0U;
        if (!WiFiConnector.connected())
        {
          wifiUpAt = 0U;
          return;
        }
        if (wifiUpAt == 0U)
        {
          wifiUpAt = millis();
        }
        if (millis() - wifiUpAt < NTP_START_DELAY)
        {
          return;
        }
        lastResolveTryMoment = millis();
        resolveNtpServerAddress(ntpServerAddressResolved);              // пытаемся получить IP адрес сервера времени (тест интернет подключения) до тех пор, пока время не будет успешно синхронизировано
        if (!ntpServerAddressResolved)
        {
          ntpRetryFailed(F("сервер недоступен (ошибка DNS/нет интернета)"));
          return;                                                         // если нет интернет подключения, отключаем будильник до тех пор, пока оно не будет восстановлено
        }
      }

#ifdef PHONE_N_MANUAL_TIME_PRIORITY
if (stillUseNTP)
#endif      
      if (timeClient.update()){
         #ifdef WARNING_IF_NO_TIME
           noTimeClear();
         #endif
         if (!timeSynched)
         {
           uiLog.println(F("NTP: время синхронизировано"));
         }
         ntpRetryInterval = RESOLVE_INTERVAL;                             // сервер ответил - возвращаем обычный интервал попыток
         timeSynched = true;
         #if defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE) // если ручное время тоже поддерживается, сохраняем туда реальное на случай отвалившегося NTP
           manualTimeShift = localTimeZone.toLocal(timeClient.getEpochTime()) - millis() / 1000UL;
         #endif
         #ifdef PHONE_N_MANUAL_TIME_PRIORITY
           stillUseNTP = false;
         #endif
      }
      else if (!timeSynched)
      {
        ntpRetryFailed(F("сервер не ответил"));                          // пакет ушёл, но ответа нет (порт 123 закрыт у провайдера, сервер молчит)
      }
}
      #endif //USE_NTP
      
      if (!timeSynched)                                                   // если время не было синхронизиировано ни разу, отключаем будильник до тех пор, пока оно не будет синхронизировано
      {
        return;
      }

      time_t currentLocalTime = getCurrentLocalTime();
      
      uint8_t thisDay = dayOfWeek(currentLocalTime);
      if (thisDay == 1) thisDay = 8;                                      // в библиотеке Time воскресенье - это 1; приводим к диапазону [0..6], где воскресенье - это 6
      thisDay -= 2;
      thisTime = hour(currentLocalTime) * 60 + minute(currentLocalTime);
      uint32_t thisFullTime = hour(currentLocalTime) * 3600 + minute(currentLocalTime) * 60 + second(currentLocalTime);

      #ifdef PRINT_TIME
      #if (PRINT_TIME != 0U) 
      printTime(thisTime, false, ONflag);                                 // проверка текущего времени и его вывод (если заказан и если текущее время соответстует заказанному расписанию вывода)
      #endif
      #endif

      // проверка рассвета
      if (alarms[thisDay].State &&                                                                                          // день будильника
          thisTime >= (uint16_t)constrain(alarms[thisDay].Time - pgm_read_byte(&dawnOffsets[dawnMode]), 0, (24 * 60)) &&    // позже начала
          thisTime < (alarms[thisDay].Time + DAWN_TIMEOUT))                                                                 // раньше конца + минута
      {
        if (!manualOff)                                                   // будильник не был выключен вручную (из приложения или кнопкой)
        {
          // величина рассвета 0-255
          int32_t dawnPosition = 255 * ((float)(thisFullTime - (alarms[thisDay].Time - pgm_read_byte(&dawnOffsets[dawnMode])) * 60) / (pgm_read_byte(&dawnOffsets[dawnMode]) * 60));
          dawnPosition = constrain(dawnPosition, 0, 255);
          for (uint8_t j = 5U; j > 0U; j--)
            if (dawnCounter >= j)
              dawnColor[j] = dawnColor[j - 1U];
          dawnColor[0] = CHSV(map(dawnPosition, 0, 255, 10, 35),
                           map(dawnPosition, 0, 255, 255, 170),
                           map(dawnPosition, 0, 255, 2, DAWN_BRIGHT));

          if (dawnCounter < 5U) dawnCounter++;
          
          
          for (uint16_t i = 0U; i < NUM_LEDS; i++)
            leds[i] = dawnColor[i % 6U];
          FastLED.setBrightness(255);
          delay(1);
          ledsShow();
          dawnFlag = true;
        }

        #if defined(ALARM_PIN) && defined(ALARM_LEVEL)                    // установка сигнала в пин, управляющий будильником
        if (thisTime == alarms[thisDay].Time)                             // установка, только в минуту, на которую заведён будильник
        {
          digitalWrite(ALARM_PIN, manualOff ? !ALARM_LEVEL : ALARM_LEVEL);// установка сигнала в зависимости от того, был ли отключен будильник вручную
        }
        #endif

        #if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)                  // установка сигнала в пин, управляющий MOSFET транзистором, матрица должна быть включена на время работы будильника
        digitalWrite(MOSFET_PIN, MOSFET_LEVEL);
        #endif
      }
      else
      {
        // не время будильника (ещё не начался или закончился по времени)
        if (dawnFlag)
        {
          dawnFlag = false;
          ledsClear();
          delay(2);
          ledsShow();
          changePower();                                                  // выключение матрицы или установка яркости текущего эффекта в засисимости от того, была ли включена лампа до срабатывания будильника
        }
        manualOff = false;
        for (uint8_t j = 0U; j < 6U; j++)
          dawnColor[j] = 0;
          
        dawnCounter = 0;
        

        #if defined(ALARM_PIN) && defined(ALARM_LEVEL)                    // установка сигнала в пин, управляющий будильником
        digitalWrite(ALARM_PIN, !ALARM_LEVEL);
        #endif

        #if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)                  // установка сигнала в пин, управляющий MOSFET транзистором, соответственно состоянию вкл/выкл матрицы
        digitalWrite(MOSFET_PIN, ONflag ? MOSFET_LEVEL : !MOSFET_LEVEL);
        #endif
      }
    }
  }
}

#ifdef USE_NTP
void resolveNtpServerAddress(bool &ntpServerAddressResolved)              // функция проверки подключения к интернету
{
  if (ntpServerAddressResolved)
  {
    return;
  }

  // резолвим адрес, заданный в настройках (раньше здесь была захардкоженная константа NTP_ADDRESS -
  // смена сервера на странице настроек на проверку доступности не влияла), с коротким таймаутом RESOLVE_TIMEOUT
  if (!WiFi.hostByName(ntpServerName.c_str(), ntpServerIp, RESOLVE_TIMEOUT) || ntpServerIp[0] == 0 || ntpServerIp == IPAddress(255U, 255U, 255U, 255U))
  {
    if (ntpServerAddressResolved)                           // переход "интернет был - пропал"
    {
      #ifdef GENERAL_DEBUG
      LOG.println(F("Подключение к интернету отсутствует"));
      #endif
      uiLog.println(F("NTP: сервер недоступен (ошибка DNS/нет интернета)"));
    }

    ntpServerAddressResolved = false;
  }
  else
  {
    #ifdef GENERAL_DEBUG
    if (!ntpServerAddressResolved)
    {
      LOG.println(F("Подключение к интернету установлено"));
    }
    #endif

    ntpServerIpStr = ntpServerIp.toString();                              // дальше NTPClient работает по IP - без DNS-запросов, блокирующих loop
    timeClient.setPoolServerName(ntpServerIpStr.c_str());                 // NTPClient хранит указатель, поэтому строка глобальная и живёт всё время работы
    ntpServerAddressResolved = true;
  }
}
#endif

void getFormattedTime(char *buf)
{
  time_t currentLocalTime = getCurrentLocalTime();
  sprintf_P(buf, PSTR("%02u:%02u:%02u"), hour(currentLocalTime), minute(currentLocalTime), second(currentLocalTime));
}

#endif

time_t getCurrentLocalTime()
{
  #if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
    #if defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
      static uint32_t milliscorrector;
    #endif

    if (timeSynched)
    {
      #if defined(USE_NTP) && defined(USE_MANUAL_TIME_SETTING) || defined(USE_NTP) && defined(GET_TIME_FROM_PHONE)
        if (milliscorrector > millis()
          #ifdef GET_TIME_FROM_PHONE
            && manualTimeShift + millis() / 1000UL < phoneTimeLastSync
          #endif
        ){
          manualTimeShift += 4294967; // защищаем время от переполнения millis()
          #ifdef GET_TIME_FROM_PHONE
            phoneTimeLastSync += 4294967; // а это, чтобы через 49 дней всё не заглючило
          #endif
        }
        milliscorrector = millis();
   
        if (ntpServerAddressResolved)
          return localTimeZone.toLocal(timeClient.getEpochTime());
        else    
          return millis() / 1000UL + manualTimeShift;
      #endif

      #if !defined(USE_NTP) && defined(USE_MANUAL_TIME_SETTING) || !defined(USE_NTP) && defined(GET_TIME_FROM_PHONE)
        if (milliscorrector > millis()
          #ifdef GET_TIME_FROM_PHONE
            && manualTimeShift + millis() / 1000UL < phoneTimeLastSync
          #endif
        ){
          manualTimeShift += 4294967; // защищаем время от переполнения millis()
          #ifdef GET_TIME_FROM_PHONE
            phoneTimeLastSync += 4294967; // а это, чтобы через 49 дней всё не заглючило
          #endif
        }
        milliscorrector = millis();
        return millis() / 1000UL + manualTimeShift;
      #endif

      #if defined(USE_NTP) && !defined(USE_MANUAL_TIME_SETTING) || defined(USE_NTP) && !defined(GET_TIME_FROM_PHONE)
        return localTimeZone.toLocal(timeClient.getEpochTime());
      #endif
    }
      else
  #endif
        return millis() / 1000UL;
}
