#ifdef USE_MQTT
/*
 * MqttManager — управление лампой по MQTT (библиотека AsyncMqttClient)
 * поверх слоя команд LampControl.
 *
 * Топики команд (входящие; <id> = LedLamp_XXXXXXXX, где XXXXXXXX - ESP.getChipId() в hex):
 *   LedLamp/<id>/cmnd            - текстовые команды (формат совместим с прошивками 2.x):
 *                                    P_ON / P_OFF        - включить/выключить лампу
 *                                    EFFn                - выбрать эффект №n (нумерация с нуля)
 *                                    BRIn / SPDn / SCAn  - яркость/скорость/масштаб (1-255, масштаб 1-100)
 *                                    BTN ON|OFF          - разблокировать/заблокировать кнопку
 *                                    TEXT текст          - задать текст бегущей строки
 *                                    ALM_SETd ON|OFF|мин - будильник дня d (1-7): вкл/выкл или время в минутах от начала суток
 *                                    DAWNn               - опция "рассвет за ... минут" (номер в списке, с единицы)
 *                                    TMR_SET 1 o sec     - таймер выключения: вкл, опция, секунд до выключения (TMR_SET 0 - отключить)
 *                                    FAV_SET ...         - настройки режима Цикл (формат см. FavoritesManager)
 *                                    WOL [MAC]           - Wake-on-LAN (без MAC - адрес из настроек)
 *   LedLamp/<id>/cmnd/power      - true/1/on | false/0/off - включить/выключить лампу
 *   LedLamp/<id>/cmnd/effect     - номер эффекта (0..MODE_AMOUNT-1)
 *   LedLamp/<id>/cmnd/brightness - яркость: 1-100 масштабируется в 1-255, значения >100 применяются как есть
 *   LedLamp/<id>/cmnd/speed      - скорость: 1-100 масштабируется в 1-255, значения >100 применяются как есть
 *   LedLamp/<id>/cmnd/scale      - масштаб (как есть)
 *   LedLamp/<id>/cmnd/button     - true/1/on | false/0/off - разблокировать/заблокировать кнопку
 *   LedLamp/<id>/cmnd/wol        - Wake-on-LAN: разбудить компьютер магическим пакетом;
 *                                  payload - MAC (AA:BB:CC:DD:EE:FF), либо пусто/true/1/on -
 *                                  MAC из настроек (false/0/off игнорируется)
 *
 * Дополнительный WOL-топик: если включён в настройках (группа Wake-on-LAN), лампа
 * подписывается на произвольный топик вне своего дерева и будит компьютер по тем же
 * правилам payload - удобно для сценариев умного дома.
 *
 * Команды смены эффекта (EFFn и топик effect) на выключенной лампе также включают её.
 *
 * Топик состояния (исходящий, retained; публикуется после каждого изменения состояния лампы):
 *   LedLamp/<id>/state - JSON: {"effect":N,"brightness":1-100,"speed":1-100,"scale":N,
 *                               "power":bool,"espMode":N,"useNtp":bool,"timerRunning":bool,
 *                               "buttonEnabled":bool,"time":"HH:MM:SS","ip":"A.B.C.D"}
 *   ip - текущий адрес лампы (в режиме точки доступа - адрес AP). Роутер может выдать
 *   лампе новый адрес после перезагрузки, а retained state всегда содержит актуальный.
 *
 * Параметры брокера хранятся в базе настроек (Storage.h, ключи kk::mqtt_*)
 * и редактируются через веб-интерфейс Settings.
 *
 * Входящие сообщения приходят в контексте асинхронного TCP - там они только
 * копируются в "отложенную команду", а применяются в MqttManager::tick() из loop().
 */

#include <AsyncMqttClient.h>
#include "pgmspace.h"
#include "Constants.h"
#include "Types.h"
#include "Storage.h"
#include "TimerManager.h"
#include "FavoritesManager.h"

// глобалы прошивки, определённые в главном файле (доступны к моменту линковки единого скетча)
extern bool ONflag;
extern uint8_t currentMode;
extern ModeType modes[];
extern bool buttonEnabled;
extern AlarmType alarms[];

// функции слоя LampControl (LampControl.ino) и вспомогательные (prototypes для использования из заголовка)
void lampSetPower(bool on);
void lampSetEffect(uint8_t effectId);
void lampSetBrightness(uint8_t value);
void lampSetSpeed(uint8_t value);
void lampSetScale(uint8_t value);
void lampSetButtonEnabled(bool enabled);
void lampSetRunningText(const char* text);
void lampSetAlarm(uint8_t day, bool state, uint16_t minutes);
void lampSetDawnMode(uint8_t mode);
void lampSetSleepTimer(uint16_t minutes);
void lampClearSleepTimer();
void mqttRequestPublish();
bool wolWake(const char* macStr);
#if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
extern uint8_t button_sleep_time;
#endif
#if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
void getFormattedTime(char *buf);
#endif

static const char MqttTopicBase[]      PROGMEM = "LedLamp"; // базовая часть топиков
static const char MqttTopicCmnd[]      PROGMEM = "cmnd";    // часть командных топиков (входящие команды лампе)
static const char MqttTopicState[]     PROGMEM = "state";   // часть топиков состояния (ответ от лампы)
static const char MqttTopicPower[]     PROGMEM = "power";
static const char MqttTopicEffect[]    PROGMEM = "effect";
static const char MqttTopicBright[]    PROGMEM = "brightness";
static const char MqttTopicSpeed[]     PROGMEM = "speed";
static const char MqttTopicScale[]     PROGMEM = "scale";
static const char MqttTopicButton[]    PROGMEM = "button";
static const char MqttTopicWol[]       PROGMEM = "wol";
static const char MqttClientIdPrefix[] PROGMEM = "LedLamp_";

class MqttManager
{
  public:
    static inline bool needToPublish = false;               // запрошена публикация состояния лампы в топик state

    // топики для отображения в веб-интерфейсе (пустые, если MQTT не сконфигурирован)
    static const String& getTopicInput() { return topicInput; }
    static const String& getTopicOutput() { return topicOutput; }

    // подписка/отписка на дополнительный WOL-топик (галка и имя топика в настройках);
    // вызывается при изменении настроек (из loop) и при каждом подключении к брокеру
    static void applyWolExtSubscription()
    {
      if (client == nullptr)
      {
        return;
      }

      String newTopic = (bool)db[kk::wol_ext_on] ? (String)db[kk::wol_ext_topic] : String();
      newTopic.trim();

      if (client->connected() && topicWolExt != newTopic)
      {
        if (topicWolExt.length())
        {
          client->unsubscribe(topicWolExt.c_str());
        }
        if (newTopic.length())
        {
          client->subscribe(newTopic.c_str(), 1);
          uiLog.printf_P(PSTR("WOL: слежение за топиком %s\n"), newTopic.c_str());
        }
      }
      topicWolExt = newTopic;
    }

    // чтение параметров брокера из хранилища, сборка топиков, создание клиента (вызывается один раз в setup при espMode == 1)
    static void setup()
    {
      if (!(bool)db[kk::mqtt_enabled])
      {
        #ifdef GENERAL_DEBUG
        LOG.println(F("MQTT отключен в настройках"));
        #endif
        return;
      }

      mqttHost = (String)db[kk::mqtt_host];
      mqttPort = (uint16_t)db[kk::mqtt_port];
      mqttUser = (String)db[kk::mqtt_user];
      mqttPass = (String)db[kk::mqtt_pass];
      if (!mqttHost.length())
      {
        uiLog.println(F("MQTT: адрес брокера не задан (настраивается на этой странице)"));
        return;
      }

      char chipIdBuf[9];
      sprintf_P(chipIdBuf, PSTR("%08X"), ESP.getChipId());
      clientId = String(FPSTR(MqttClientIdPrefix)) + chipIdBuf;

      String base = String(FPSTR(MqttTopicBase)) + '/' + clientId + '/';
      topicInput = base + FPSTR(MqttTopicCmnd);              // LedLamp/<id>/cmnd
      topicOutput = base + FPSTR(MqttTopicState);            // LedLamp/<id>/state
      topicPower = topicInput + '/' + FPSTR(MqttTopicPower);
      topicEffect = topicInput + '/' + FPSTR(MqttTopicEffect);
      topicBrightness = topicInput + '/' + FPSTR(MqttTopicBright);
      topicSpeed = topicInput + '/' + FPSTR(MqttTopicSpeed);
      topicScale = topicInput + '/' + FPSTR(MqttTopicScale);
      topicButton = topicInput + '/' + FPSTR(MqttTopicButton);
      topicWol = topicInput + '/' + FPSTR(MqttTopicWol);

      #ifdef GENERAL_DEBUG
      LOG.printf_P(PSTR("MQTT топик для входящих команд: %s\n"), topicInput.c_str());
      LOG.printf_P(PSTR("MQTT топик для исходящих ответов лампы: %s\n"), topicOutput.c_str());
      #endif

      client = new AsyncMqttClient();
      client->setServer(mqttHost.c_str(), mqttPort);
      client->setClientId(clientId.c_str());
      if (mqttUser.length())
      {
        client->setCredentials(mqttUser.c_str(), mqttPass.c_str());
      }
      client->onConnect(onMqttConnect);
      client->onDisconnect(onMqttDisconnect);
      client->onMessage(onMqttMessage);
    }

    // обслуживание MQTT: переподключение, применение отложенных команд, публикация состояния; вызывается в loop()
    static void tick()
    {
      if (!client)
      {
        return;
      }

      if (espMode == 1U && WiFi.isConnected() && !client->connected())
      {
        connect();                                          // библиотека не умеет восстанавливать соединение сама, управляем этим явно (с периодом MQTT_RECONNECT_TIME)
        needToPublish = true;
      }

      processPending();                                     // применение команды, принятой в async-контексте

      if (needToPublish)
      {
        publishState();
      }
    }

  private:
    // типы отложенной команды (заполняется в onMqttMessage, применяется в processPending)
    enum PendingType : uint8_t { PT_NONE = 0, PT_POWER, PT_EFFECT, PT_BRIGHTNESS, PT_SPEED, PT_SCALE, PT_BUTTON, PT_RAW };

    static inline AsyncMqttClient* client = nullptr;
    static inline String mqttHost, mqttUser, mqttPass, clientId;
    static inline uint16_t mqttPort = 0;
    static inline String topicInput, topicOutput;
    static inline String topicPower, topicEffect, topicBrightness, topicSpeed, topicScale, topicButton, topicWol;
    static inline String topicWolExt;                       // дополнительный WOL-топик (пустой - слежение выключено)
    static inline uint32_t lastConnectingAttempt = 0;
    static inline volatile uint8_t pendingType = PT_NONE;
    static inline volatile int32_t pendingValue = 0;
    static inline char pendingRaw[CMD_BUFFER_SIZE] = {0};
    static const uint8_t qos = 0U;                          // MQTT quality of service для публикаций

    static void connect()
    {
      if ((!lastConnectingAttempt) || (millis() - lastConnectingAttempt >= MQTT_RECONNECT_TIME * 1000U))
      {
        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("Подключение к MQTT брокеру \"%s:%u\"...\n"), mqttHost.c_str(), mqttPort);
        #endif
        client->connect();
        lastConnectingAttempt = millis();
      }
    }

    static void onMqttConnect(bool sessionPresent)
    {
      #ifdef GENERAL_DEBUG
      LOG.println(F("Подключено к MQTT брокеру"));
      #endif
      uiLog.println(F("MQTT: подключено к брокеру"));        // запись в кольцевой буфер журнала - безопасно из async-контекста
      lastConnectingAttempt = 0;

      client->subscribe(topicInput.c_str(), 1);
      client->subscribe(topicPower.c_str(), 1);
      client->subscribe(topicEffect.c_str(), 1);
      client->subscribe(topicBrightness.c_str(), 1);
      client->subscribe(topicSpeed.c_str(), 1);
      client->subscribe(topicScale.c_str(), 1);
      client->subscribe(topicButton.c_str(), 1);
      client->subscribe(topicWol.c_str(), 1);

      topicWolExt = "";                                     // подписка на дополнительный WOL-топик выполняется заново при каждом подключении
      applyWolExtSubscription();

      needToPublish = true;                                 // публикация состояния сразу после подключения (выполнится из tick)
    }

    // человекочитаемая причина отключения от брокера (для журнала)
    static PGM_P disconnectReasonText(AsyncMqttClientDisconnectReason reason)
    {
      switch (reason)
      {
        case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:                    return PSTR("обрыв соединения");
        case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION: return PSTR("версия протокола отклонена");
        case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:           return PSTR("id клиента отклонён");
        case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:            return PSTR("сервер недоступен");
        case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:         return PSTR("некорректные логин/пароль");
        case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:                return PSTR("доступ запрещён");
        case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:           return PSTR("не хватает памяти");
        case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:                return PSTR("ошибка TLS");
      }
      return PSTR("неизвестная причина");
    }

    static void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
    {
      #ifdef GENERAL_DEBUG
      LOG.println(F("Отключено от MQTT брокера"));
      #endif
      char reasonBuf[64];
      strncpy_P(reasonBuf, disconnectReasonText(reason), sizeof(reasonBuf) - 1U); // текст копируется из PROGMEM, читать его напрямую по байтам нельзя
      reasonBuf[sizeof(reasonBuf) - 1U] = '\0';
      uiLog.printf_P(PSTR("MQTT: отключено от брокера (%s)\n"), reasonBuf);
    }

    // приём сообщения (async-контекст!): только копирование в отложенную команду, никакого управления лампой отсюда
    static void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
    {
      if (payload == NULL || index != 0 || total != len)
      {
        return;
      }

      char buf[CMD_BUFFER_SIZE];
      size_t n = len < (CMD_BUFFER_SIZE - 1U) ? len : (CMD_BUFFER_SIZE - 1U);
      memcpy(buf, payload, n);
      buf[n] = '\0';

      #ifdef GENERAL_DEBUG
      LOG.printf_P(PSTR("MQTT получено: топик \"%s\", значение \"%s\"\n"), topic, buf);
      #endif

      if (topicPower == topic)
      {
        pendingValue = parseBoolPayload(buf);
        pendingType = PT_POWER;
      }
      else if (topicEffect == topic)
      {
        pendingValue = atoi(buf);
        pendingType = PT_EFFECT;
      }
      else if (topicBrightness == topic)
      {
        pendingValue = scalePercentPayload(buf);            // 1-100 -> 1-255, значения вне диапазона как есть
        pendingType = PT_BRIGHTNESS;
      }
      else if (topicSpeed == topic)
      {
        pendingValue = scalePercentPayload(buf);
        pendingType = PT_SPEED;
      }
      else if (topicScale == topic)
      {
        pendingValue = atoi(buf);
        pendingType = PT_SCALE;
      }
      else if (topicButton == topic)
      {
        pendingValue = parseBoolPayload(buf);
        pendingType = PT_BUTTON;
      }
      else if (topicWol == topic ||
               (topicWolExt.length() && topicWolExt == topic)) // Wake-on-LAN (свой или дополнительный топик): payload - MAC, true/пусто - MAC из настроек
      {
        snprintf_P(pendingRaw, CMD_BUFFER_SIZE, PSTR("WOL %s"), buf);
        pendingType = PT_RAW;
      }
      else                                                  // базовый командный топик - легаси-команда текстом
      {
        strncpy(pendingRaw, buf, CMD_BUFFER_SIZE - 1U);
        pendingRaw[CMD_BUFFER_SIZE - 1U] = '\0';
        pendingType = PT_RAW;
      }
    }

    // применение отложенной команды из loop()
    static void processPending()
    {
      uint8_t type = pendingType;
      if (type == PT_NONE)
      {
        return;
      }
      int32_t value = pendingValue;
      pendingType = PT_NONE;

      switch (type)
      {
        case PT_POWER:      lampSetPower(value != 0);                            break;
        case PT_EFFECT:
          lampSetEffect(constrain(value, 0, MODE_AMOUNT - 1));
          lampSetPower(true);                               // команда смены эффекта на выключенной лампе также включает её (на включенной lampSetPower ничего не делает)
          break;
        case PT_BRIGHTNESS: lampSetBrightness(constrain(value, 1, 255));         break;
        case PT_SPEED:      lampSetSpeed(constrain(value, 1, 255));              break;
        case PT_SCALE:      lampSetScale(constrain(value, 0, 255));              break;
        case PT_BUTTON:
          buttonEnabled = (value != 0);
          Storage::SaveButtonEnabled(&buttonEnabled);
          break;
        case PT_RAW:        processRawCommand(pendingRaw);                       break;
      }

      needToPublish = true;
    }

    // легаси-команды текстом на базовом cmnd топике
    static void processRawCommand(char* cmd)
    {
      if (!strncmp_P(cmd, PSTR("P_ON"), 4))
      {
        lampSetPower(true);
      }
      else if (!strncmp_P(cmd, PSTR("P_OFF"), 5))
      {
        lampSetPower(false);
      }
      else if (!strncmp_P(cmd, PSTR("EFF"), 3))
      {
        lampSetEffect(constrain(atoi(cmd + 3), 0, MODE_AMOUNT - 1));
        lampSetPower(true);                                 // команда смены эффекта на выключенной лампе также включает её
      }
      else if (!strncmp_P(cmd, PSTR("BRI"), 3))
      {
        lampSetBrightness(constrain(atoi(cmd + 3), 1, 255));
      }
      else if (!strncmp_P(cmd, PSTR("SPD"), 3))
      {
        lampSetSpeed(constrain(atoi(cmd + 3), 1, 255));
      }
      else if (!strncmp_P(cmd, PSTR("SCA"), 3))
      {
        lampSetScale(constrain(atoi(cmd + 3), 0, 255));
      }
      else if (!strncmp_P(cmd, PSTR("BTN"), 3))
      {
        lampSetButtonEnabled(strstr_P(cmd, PSTR("ON")) != NULL);
      }
      else if (!strncmp_P(cmd, PSTR("TEXT"), 4))
      {
        lampSetRunningText(cmd + (cmd[4] == ' ' ? 5 : 4));  // формат: "TEXT новый текст бегущей строки"
      }
      else if (!strncmp_P(cmd, PSTR("WOL"), 3))             // формат: "WOL" (MAC из настроек) или "WOL AA:BB:CC:DD:EE:FF"
      {
        wolWake(cmd + (cmd[3] == ' ' ? 4 : 3));
      }
      else if (!strncmp_P(cmd, PSTR("ALM_SET"), 7))         // формат: "ALM_SET1 ON" / "ALM_SET1 OFF" / "ALM_SET1 390" (номер дня 1-7, время в минутах от начала суток)
      {
        uint8_t alarmNum = (uint8_t)(cmd[7] - '0') - 1U;
        if (alarmNum < 7U)
        {
          const char* onPos = strstr_P(cmd, PSTR("ON"));
          const char* offPos = strstr_P(cmd, PSTR("OFF"));
          if (onPos != NULL && onPos - cmd == 9)
          {
            lampSetAlarm(alarmNum, true, alarms[alarmNum].Time);
          }
          else if (offPos != NULL && offPos - cmd == 9)
          {
            lampSetAlarm(alarmNum, false, alarms[alarmNum].Time);
          }
          else
          {
            lampSetAlarm(alarmNum, alarms[alarmNum].State, atoi(cmd + 8));
          }
        }
      }
      else if (!strncmp_P(cmd, PSTR("DAWN"), 4))            // формат: "DAWN1" (номер опции "рассвет за ... минут", нумерация с единицы)
      {
        lampSetDawnMode(atoi(cmd + 4) - 1);
      }
      else if (!strncmp_P(cmd, PSTR("TMR_SET"), 7))         // формат: "TMR_SET 1 3 300" (вкл/выкл, номер опции в списке, секунды до выключения)
      {
        bool timerOn = atoi(cmd + 8) != 0;
        uint32_t seconds = (strlen(cmd) > 12U) ? strtoul(cmd + 12, NULL, 10) : 0UL;
        if (timerOn && seconds > 0UL)
        {
          TimerManager::TimerOption = (uint8_t)atoi(cmd + 10);
          #if defined(BUTTON_CAN_SET_SLEEP_TIMER) && defined(ESP_USE_BUTTON)
          button_sleep_time = constrain(seconds / 60UL, 1, 255);
          Storage::Save_button_sleep_time(&button_sleep_time);
          #endif
          TimerManager::TimeToFire = millis() + seconds * 1000UL;
          TimerManager::TimerRunning = true;
          TimerManager::TimerHasFired = false;
          mqttRequestPublish();
        }
        else
        {
          lampClearSleepTimer();
        }
      }
      else if (!strncmp_P(cmd, PSTR("FAV_SET"), 7))         // формат: "FAV_SET 1 60 120 0 0 1 0 ..." (см. FavoritesManager::ConfigureFavorites)
      {
        FavoritesManager::ConfigureFavorites(cmd);
        FavoritesManager::SaveFavoritesToStorage();
        mqttRequestPublish();
      }
      else
      {
        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("MQTT: неизвестная команда \"%s\"\n"), cmd);
        #endif
      }
    }

    // публикация состояния лампы в топик state (retained JSON)
    static void publishState()
    {
      if (!client->connected())
      {
        return;                                             // флаг needToPublish сохраняется - публикация уйдёт после переподключения
      }

      char timeBuf[9];
      #if defined(USE_NTP) || defined(USE_MANUAL_TIME_SETTING) || defined(GET_TIME_FROM_PHONE)
      getFormattedTime(timeBuf);
      #else
      time_t currentTicks = millis() / 1000UL;
      sprintf_P(timeBuf, PSTR("%02u:%02u:%02u"), hour(currentTicks), minute(currentTicks), second(currentTicks));
      #endif

      char json[CMD_BUFFER_SIZE];
      snprintf_P(json, sizeof(json),
        PSTR("{\"effect\":%u,\"brightness\":%d,\"speed\":%d,\"scale\":%u,\"power\":%s,\"espMode\":%u,\"useNtp\":%s,\"timerRunning\":%s,\"buttonEnabled\":%s,\"time\":\"%s\",\"ip\":\"%s\"}"),
        currentMode,
        (int)map(constrain(modes[currentMode].Brightness, 1, 255), 1, 255, 1, 100), // яркость наружу в диапазоне 1-100
        (int)map(constrain(modes[currentMode].Speed, 1, 255), 1, 255, 1, 100),      // скорость наружу в диапазоне 1-100
        modes[currentMode].Scale,
        ONflag ? "true" : "false",
        espMode,
        #ifdef USE_NTP
        "true",
        #else
        "false",
        #endif
        TimerManager::TimerRunning ? "true" : "false",
        buttonEnabled ? "true" : "false",
        timeBuf,
        (WiFiConnector.connected() ? WiFi.localIP() : WiFi.softAPIP()).toString().c_str());

      #ifdef GENERAL_DEBUG
      LOG.printf_P(PSTR("Отправлено MQTT: топик \"%s\", значение \"%s\"\n"), topicOutput.c_str(), json);
      #endif

      client->publish(topicOutput.c_str(), qos, true, json, 0);
      needToPublish = false;
    }

    // разбор булевых payload'ов: true/1/on - включено, всё остальное - выключено
    static int32_t parseBoolPayload(const char* buf)
    {
      return (strncmp(buf, "true", 4) == 0 || strncmp(buf, "1", 1) == 0 || strncmp(buf, "on", 2) == 0) ? 1 : 0;
    }

    // разбор процентных payload'ов: 1-100 масштабируется в 1-255, значения вне диапазона применяются как есть
    static int32_t scalePercentPayload(const char* buf)
    {
      int value = atoi(buf);
      if (value > 0 && value <= 100)
      {
        return map(value, 1, 100, 1, 255);
      }
      return value;
    }
};

#endif
