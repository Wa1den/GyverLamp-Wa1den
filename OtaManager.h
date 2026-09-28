#pragma once
/*
 * 11.07.2019
 * Класс, который отслеживает действия пользователя по запросу обновления прошивки по воздуху и выполняет эту прошивку.
 * Запрос на обновление - это вызов метода RequestOtaUpdate(), его нужно поместить, например, в обработчик нажатия кнопки, приёма UDP пакета и т.д.
 * Запрос подтверждается удержанием кнопки после четырёх кликов (button.ino): случайная серия кликов удержания не даёт, поэтому режим обновления включается с первого запроса.
 * Режим обновления - это прослушивание специального порта (ESP_OTA_PORT) в ожидании команды обновления прошивки по воздуху (по сети).
 * Режим обновления работает параллельно с основным режимом функционирования, в режиме клиента WiFi и точки доступа.
 * Режим обновления активен в течение заданного промежутка времени (ESP_CONF_TIMEOUT). Потом ESP автоматически перезагружается.
 * Обновление производится из Arduino IDE: меню Инструменты - Порт - <Выбрать обнаруженный СЕТЕВОЙ COM порт из списка> (если он не обнаружен, значит что-то настроено неправильно), затем обычная команда "Загрузка" для прошивки.
 * Для включения опции обновления по воздуху в основном файле должен быть определён идентификатор OTA "#define OTA" и режим "#define ESP_MODE (1U)" (а также в данном проекте должна быть подключена кнопка).
*/

#ifdef OTA

#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include "Storage.h"                                        // hostName() - имя лампы в локальной сети


enum OtaPhase                                               // определение стадий процесса обновления по воздуху: нет, получено первое подтверждение, получено второе подтверждение, получено второе подтверждение - в процессе, обновление окончено
{
  None = 0,
  Requested,
  InProgress,
  Done
};


class OtaManager
{
  public:
    static OtaPhase OtaFlag;

    OtaManager(ShowWarningDelegate showWarningDelegate)
    {
      this->showWarningDelegate = showWarningDelegate;
    }

    bool RequestOtaUpdate()                                 // включает режим обновления; возвращает true, если он не был включён
    {
      if (OtaFlag != OtaPhase::None)
      {
        return false;
      }

      OtaFlag = OtaPhase::Requested;
      momentOfOtaStart = millis();

      #ifdef GENERAL_DEBUG
      LOG.print(F("Старт режима обновления по воздуху
"));
      #endif

      showWarningDelegate(CRGB::Yellow, 2000U, 500U);       // мигание жёлтым цветом 2 секунды (2 раза) - готовность к прошивке
      startOtaUpdate();
      return true;
    }

    void HandleOtaUpdate()
    {
      if ((OtaFlag == OtaPhase::Requested || OtaFlag == OtaPhase::InProgress) &&
          millis() - momentOfOtaStart >= ESP_CONF_TIMEOUT * 1000)
      {
        OtaFlag = OtaPhase::None;
        momentOfOtaStart = 0;

        #ifdef GENERAL_DEBUG
        LOG.print(F("Таймаут ожидания прошивки по воздуху превышен\nСброс флага в исходное состояние\nПерезагрузка\n"));
        delay(500);
        #endif

        showWarningDelegate(CRGB::Red, 2000U, 500U);        // мигание красным цветом 2 секунды (2 раза) - ожидание прошивки по воздуху прекращено, перезагрузка

        ESP.restart();
        return;
      }

      if (OtaFlag == OtaPhase::InProgress)
      {
        ArduinoOTA.handle();
      }
    }

  private:
    uint32_t momentOfOtaStart = 0;                          // момент времени, когда развёрнута WiFi точка доступа для обновления по воздуху
    ShowWarningDelegate showWarningDelegate;

    void startOtaUpdate()
    {
      String espHostName = hostName();                      // то же имя, что и в локальной сети: ArduinoOTA.begin() перезапускает
                                                            // общий объект MDNS со своим именем, и с отдельным именем лампа на
                                                            // время прошивки перестала бы отвечать на <имя>.local
      ArduinoOTA.setPort(ESP_OTA_PORT);
      ArduinoOTA.setHostname(espHostName.c_str());
      ArduinoOTA.setPassword(AP_PASS);

      ArduinoOTA.onStart([this]()
      {
        OtaFlag = OtaPhase::InProgress;
        char type[16];
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
          strcpy_P(type, PSTR("sketch"));
        }
        else // U_SPIFFS
        {
          strcpy_P(type, PSTR("filesystem"));
        }

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()

        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("Start updating %s\n"), type);
        #endif
      });

      ArduinoOTA.onEnd([this]()
      {
        OtaFlag = OtaPhase::Done;
        momentOfOtaStart = 0;

        #ifdef GENERAL_DEBUG
        LOG.print(F("Обновление по воздуху выполнено\nПерезапуск"));
        delay(500);
        #endif
      });

      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
      {
        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("Ход выполнения: %u%%\r"), (progress / (total / 100)));
        #endif
      });

      ArduinoOTA.onError([this](ota_error_t error)
      {
        OtaFlag = OtaPhase::None;
        momentOfOtaStart = 0;

        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("Обновление по воздуху завершилось ошибкой [%u]: "), error);
        #endif

        if (error == OTA_AUTH_ERROR)
        {
          #ifdef GENERAL_DEBUG
          LOG.println(F("Auth Failed"));
          #endif
        }
        else if (error == OTA_BEGIN_ERROR)
        {
          #ifdef GENERAL_DEBUG
          LOG.println(F("Begin Failed"));
          #endif
        }
        else if (error == OTA_CONNECT_ERROR)
        {
          #ifdef GENERAL_DEBUG
          LOG.println(F("Connect Failed"));
          #endif
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
          #ifdef GENERAL_DEBUG
          LOG.println(F("Receive Failed"));
          #endif
        }
        else if (error == OTA_END_ERROR)
        {
          #ifdef GENERAL_DEBUG
          LOG.println(F("End Failed"));
          #endif
        }

        #ifdef GENERAL_DEBUG
        LOG.print(F("Сброс флага в исходное состояние\nПереход в режим ожидания запроса прошивки по воздуху\n"));
        #endif
      });

      ArduinoOTA.setRebootOnSuccess(true);
      ArduinoOTA.begin();
      OtaFlag = OtaPhase::InProgress;

      #ifdef GENERAL_DEBUG
      LOG.printf_P(PSTR("Для обновления в Arduino IDE выберите пункт меню Инструменты - Порт - '%s at "), espHostName.c_str());
      LOG.print(WiFi.localIP());
      LOG.println(F("'"));
      LOG.printf_P(PSTR("Затем нажмите кнопку 'Загрузка' в течение %u секунд и по запросу введите пароль '%s'\n"), ESP_CONF_TIMEOUT, AP_PASS);
      LOG.println(F("Устройство с Arduino IDE должно быть в одной локальной сети с модулем ESP!"));
      #endif
    }
};

#endif
