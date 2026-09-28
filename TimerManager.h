#pragma once


class TimerManager
{
  public:
    static bool TimerRunning;                               // флаг "таймер взведён"
    static bool TimerHasFired;                              // флаг "таймер отработал"
    static uint8_t TimerOption;                             // индекс элемента в списке List Picker'а
    static uint32_t TimeToFire;                             // millis() срабатывания; сравнивается знаковой разностью, поэтому переживает переполнение millis() раз в 49,7 суток

    static void HandleTimer(                                // функция, обрабатывающая срабатывание таймера, гасит матрицу
      bool* ONflag,
      bool* settChanged,
      uint32_t* eepromTimeout,
      void (*changePower)())
    {
      if (!TimerManager::TimerHasFired &&
           TimerManager::TimerRunning &&
           (int32_t)(millis() - TimerManager::TimeToFire) >= 0)
      {
        #ifdef GENERAL_DEBUG
        LOG.print(F("Выключение по таймеру\n\n"));
        #endif

        TimerManager::TimerRunning = false;
        TimerManager::TimerHasFired = true;
        ledsClear();
        delay(2);
        ledsShow();
        *ONflag = false;
        changePower();
        *settChanged = true;
        *eepromTimeout = millis();

//        #ifdef USE_BLYNK короче, раз в Блинке нет управления таймером, то и это мы поддерживать не будем
//        updateRemoteBlynkParams();
//        #endif
      }
    }
};
