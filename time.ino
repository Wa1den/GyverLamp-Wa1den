// Будильник-рассвет: за время «Рассвет начинается за» до будильника лампа плавно разгорается тёплым светом.
// Проверка раз в 3 секунды (timeTimer); до синхронизации времени будильники не работают.

static CRGB dawnColor[6];
static uint8_t dawnCounter = 0;                                           // счётчик первых шагов рассвета

void timeTick()
{
  clockTick();

  if (!timeTimer.isReady() || !timeSynched)
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
    if (!manualOff)                                                   // будильник не выключили кнопкой
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
