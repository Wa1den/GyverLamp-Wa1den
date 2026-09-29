#pragma once


class TimerManager
{
  public:
    static bool TimerRunning;                               // флаг "таймер взведён"
    static bool TimerHasFired;                              // флаг "таймер отработал"
    static uint32_t TimeToFire;                             // millis() срабатывания; сравнивается знаковой разностью, поэтому переживает переполнение millis() раз в 49,7 суток

    static bool HandleTimer()                               // true - таймер только что сработал
    {
      if (!TimerManager::TimerHasFired &&
           TimerManager::TimerRunning &&
           (int32_t)(millis() - TimerManager::TimeToFire) >= 0)
      {
        TimerManager::TimerRunning = false;
        TimerManager::TimerHasFired = true;
        return true;
      }
      return false;
    }
};
