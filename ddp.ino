// Кадры с компьютера по протоколу DDP (Distributed Display Protocol, порт 4048): так лампу
// подключают CaseLight, Hyperion, LedFx, xLights. Кадры показывает эффект «Кадры с компьютера»,
// в остальных эффектах они не принимаются. Без кадров дольше DDP_TIMEOUT_MS лампа гаснет.
// Приём включается в разделе «Сеть»; выключенный, он скрывает эффект и закрывает порт.
// С автоматическим переключением первый кадр переводит лампу на этот эффект без записи в настройки,
// как Цикл, а через ddp_wait секунд без кадров возвращает прежний эффект. Кадрами здесь считаются
// только те, где что-то светится: CaseLight шлёт чёрные кадры и тогда, когда эквалайзер молчит. В режиме «выключить лампу»
// кадры включают и выключенную лампу, а после них она гаснет. Выключенная во время кадров лампа
// не включается, пока в кадрах не будет паузы.
//
// Пиксели идут построчно сверху вниз, в строке слева направо, по 3 байта RGB: отправителю не
// нужно знать подключение матрицы, его учитывает XY(). На запрос статуса (флаг QUERY, адресат
// DDP_ID_STATUS) лампа отвечает в любом эффекте: по ответу программа находит её в сети и
// показывает, почему кадры не видны.

#define DDP_PORT               (4048U)
#define DDP_TIMEOUT_MS         (2500U)  // столько же ждёт WLED; CaseLight повторяет кадр раз в секунду
#define DDP_HEADER_LEN         (10U)
#define DDP_FLAGS_VER_MASK     (0xC0U)
#define DDP_FLAGS_VER1         (0x40U)
#define DDP_FLAGS_TIMECODE     (0x10U)
#define DDP_FLAGS_REPLY        (0x04U)
#define DDP_FLAGS_QUERY        (0x02U)
#define DDP_FLAGS_PUSH         (0x01U)
#define DDP_ID_DISPLAY         (1U)
#define DDP_ID_CONFIG          (250U)
#define DDP_ID_STATUS          (251U)
#define DDP_ID_ALL             (255U)
#define DDP_TYPE_RGBW          (4U)     // поле типа данных, биты 3-5
#define DDP_LIT_LEVEL          (8U)     // кадр светится, если хоть один канал ярче: шум у нуля - ещё темнота

static_assert(EFF_DDP == MODE_AMOUNT - 1U, "Кадры с компьютера скрываются укорачиванием списка эффектов с конца: новый эффект ставится перед ними");

static bool ddpEnabled = false;                             // приём включён в настройках; читается из базы при первом обращении
static bool ddpEnabledLoaded = false;
static WiFiUDP ddpUdp;
static bool ddpListening = false;
static bool ddpNoSleep = false;                             // сон WiFi выключен на время эффекта
static uint8_t ddpFrame[NUM_LEDS * 3U];                     // кадр в порядке отправителя, до выдачи на ленту
static bool ddpNewFrame = false;                            // пришёл PUSH, кадр ещё не выведен
static bool ddpHaveFrame = false;                           // был хотя бы один кадр с момента включения режима
static uint32_t ddpFrameAt = 0U;
static bool ddpLit = false;                                 // на ленте кадр, а не темнота
static IPAddress ddpSender;
static uint16_t ddpFrameCount = 0U;                         // кадры за текущую секунду, для страницы
static uint16_t ddpFps = 0U;
static uint32_t ddpFpsAt = 0U;
static bool ddpAutoOn = false;                              // эффект включён приходом кадров, а не вручную
static uint8_t ddpReturnMode = 0U;                          // эффект, к которому лампа вернётся без кадров
static uint32_t ddpLitAt = 0U;                              // последний кадр, в котором что-то светилось

static bool ddpModeActive()
{
  return ONflag && currentMode == EFF_DDP;
}

static bool ddpFresh()
{
  return ddpHaveFrame && millis() - ddpFrameAt < DDP_TIMEOUT_MS;
}

// пришедший кадр может включить эффект: лампа включена и не занята рассветом, картинкой или обновлением
static bool ddpAutoCanStart()
{
  #ifdef OTA
  if (OtaManager::OtaFlag != OtaPhase::None)
  {
    return false;
  }
  #endif
  return (ONflag || (uint8_t)db[kk::ddp_end] == 1U) && !dawnFlag && overlayCurrent() == OVERLAY_NONE && (bool)db[kk::ddp_auto];
}

static void ddpAutoStart()
{
  ddpReturnMode = currentMode;
  ddpAutoOn = true;
  lampShowEffect(EFF_DDP);
  lampSetPower(true);
}

static void ddpAutoReturn()
{
  ddpAutoOn = false;
  lampShowEffect(ddpReturnMode);
  if ((uint8_t)db[kk::ddp_end] == 1U)
  {
    lampSetPower(false);
  }
}

// эффект для записи в настройки: после перезагрузки лампа не должна остаться на автовключённом эффекте
uint8_t ddpSavedMode()
{
  return ddpAutoOn && currentMode == EFF_DDP ? ddpReturnMode : currentMode;
}

// эффект включён приходом кадров; Цикл в это время не переключает
bool ddpAutoActive()
{
  return ddpAutoOn && currentMode == EFF_DDP;
}

static void ddpReplyStatus()
{
  char json[224];
  int len = snprintf_P(json, sizeof(json),
    PSTR("{\"status\":{\"man\":\"" FIRMWARE_NAME "\",\"mod\":\"GyverLamp\",\"ver\":\"" FIRMWARE_VERSION "\","
         "\"mac\":\"%s\",\"name\":\"%s\",\"w\":%u,\"h\":%u,\"on\":%s,\"live\":%s}}"),
    WiFi.macAddress().c_str(), hostName().c_str(), (unsigned)WIDTH, (unsigned)HEIGHT,
    ONflag ? "true" : "false", currentMode == EFF_DDP || (bool)db[kk::ddp_auto] ? "true" : "false");
  if (len <= 0 || len >= (int)sizeof(json))
  {
    return;
  }

  uint8_t header[DDP_HEADER_LEN] = {
    DDP_FLAGS_VER1 | DDP_FLAGS_REPLY | DDP_FLAGS_PUSH, 0U, 0U, DDP_ID_STATUS,
    0U, 0U, 0U, 0U, (uint8_t)(len >> 8), (uint8_t)len
  };
  ddpUdp.beginPacket(ddpUdp.remoteIP(), ddpUdp.remotePort());
  ddpUdp.write(header, sizeof(header));
  ddpUdp.write((const uint8_t*)json, len);
  ddpUdp.endPacket();
}

// разбор одного принятого пакета; true - пришёл конец кадра (PUSH)
static bool ddpReadPacket(int size)
{
  uint8_t header[DDP_HEADER_LEN + 4U];
  if (size < (int)DDP_HEADER_LEN || ddpUdp.read(header, DDP_HEADER_LEN) != (int)DDP_HEADER_LEN)
  {
    return false;
  }

  uint8_t flags = header[0];
  if ((flags & DDP_FLAGS_VER_MASK) != DDP_FLAGS_VER1)
  {
    return false;
  }

  uint8_t headerLen = DDP_HEADER_LEN;
  if (flags & DDP_FLAGS_TIMECODE)                           // метка времени не используется, кадр показывается сразу
  {
    headerLen += 4U;
    if (size < (int)headerLen || ddpUdp.read(header + DDP_HEADER_LEN, 4U) != 4)
    {
      return false;
    }
  }

  uint8_t id = header[3];
  if (flags & DDP_FLAGS_QUERY)
  {
    if (id == DDP_ID_STATUS)
    {
      ddpReplyStatus();
    }
    return false;
  }

  if ((flags & DDP_FLAGS_REPLY) || (id != DDP_ID_DISPLAY && id != DDP_ID_ALL) ||
      ((header[2] >> 3) & 0x07U) == DDP_TYPE_RGBW)
  {
    return false;
  }
  if (!ddpModeActive() && !ddpAutoActive() && !ddpAutoCanStart()) // кадр не покажут и он не включит эффект
  {
    return false;
  }

  uint32_t offset = ((uint32_t)header[4] << 24) | ((uint32_t)header[5] << 16) | ((uint32_t)header[6] << 8) | header[7];
  uint16_t length = ((uint16_t)header[8] << 8) | header[9];
  length = min((int)length, size - headerLen);
  if (offset < sizeof(ddpFrame))
  {
    ddpUdp.read(ddpFrame + offset, min((uint32_t)length, (uint32_t)sizeof(ddpFrame) - offset));
  }

  if (!(flags & DDP_FLAGS_PUSH))
  {
    return false;
  }
  ddpSender = ddpUdp.remoteIP();
  return true;
}

static bool ddpEnabledSetting()
{
  if (!ddpEnabledLoaded)                                    // база открывается в setup, а первым спросить может и страница, и MQTT
  {
    ddpEnabledLoaded = true;
    ddpEnabled = (bool)db[kk::ddp_on];
  }
  return ddpEnabled;
}

// в принятом кадре что-то светится
static bool ddpFrameLit()
{
  for (uint16_t i = 0U; i < sizeof(ddpFrame); i++)
  {
    if (ddpFrame[i] >= DDP_LIT_LEVEL)
    {
      return true;
    }
  }
  return false;
}

// эффект не скрыт настройкой
bool effectAvailable(uint8_t effectId)
{
  ddpEnabledSetting();
  return effectId < MODE_AMOUNT && (effectId != EFF_DDP || ddpEnabled);
}

// длина списка названий для выбора эффекта: без последнего пункта, если он скрыт
uint16_t effectListLength()
{
  uint16_t length = strlen_P(effectNamesList);
  if (!ddpEnabledSetting())
  {
    length -= getEffectName(EFF_DDP).length() + 1U;         // название и разделитель перед ним
  }
  return length;
}

void ddpSetEnabled(bool on)
{
  ddpEnabledLoaded = true;
  ddpEnabled = on;
  db.set(kk::ddp_on, on);
}

// приём пакетов: вызывается в каждом цикле, до effectsTick, чтобы кадр ушёл на ленту в том же проходе
void ddpTick()
{
  if (!ddpEnabledSetting())
  {
    if (ddpAutoActive())                                    // приём выключили, пока эффект был включён кадрами
    {
      ddpAutoReturn();
    }
    else if (currentMode == EFF_DDP)                        // приём выключили, пока эффект был выбран, или так сохранилось до обновления
    {
      lampSetEffect(effectNeighbour(EFF_DDP, -1));
    }
    if (ddpListening)
    {
      ddpUdp.stop();
      ddpListening = false;
    }
    if (ddpNoSleep)
    {
      ddpNoSleep = false;
      WiFi.setSleepMode(WIFI_MODEM_SLEEP);
    }
    return;
  }

  if (!ddpListening)
  {
    ddpListening = ddpUdp.begin(DDP_PORT);
    if (!ddpListening)
    {
      return;
    }
  }

  // Без этого ESP8266 в простое уходит в modem sleep и принимает пакеты пачками по маяку
  // точки доступа, раз в ~100 мс: картинка дёргается. В других эффектах сон остаётся.
  if (ddpNoSleep != ddpModeActive())
  {
    ddpNoSleep = ddpModeActive();
    WiFi.setSleepMode(ddpNoSleep ? WIFI_NONE_SLEEP : WIFI_MODEM_SLEEP);
  }

  for (uint8_t i = 0U; i < 8U; i++)                         // накопившиеся пакеты разбираются разом, на ленту идёт последний кадр
  {
    int size = ddpUdp.parsePacket();
    if (size <= 0)
    {
      break;
    }
    if (ddpReadPacket(size))
    {
      bool lit = ddpFrameLit();
      if (lit)
      {
        ddpLitAt = millis();
      }
      if (!ddpModeActive())
      {
        if (lit && !ddpAutoActive() && ddpAutoCanStart())   // выключенная во время кадров лампа ждёт паузы в них
        {
          ddpAutoStart();
        }
        if (!ddpModeActive())
        {
          continue;
        }
      }
      ddpNewFrame = true;
      ddpHaveFrame = true;
      ddpFrameAt = millis();
      ddpFrameCount++;
    }                                                       // остаток пакета отбрасывает следующий parsePacket
  }

  if (ddpAutoOn && currentMode != EFF_DDP)                  // эффект сменили вручную - возвращать некуда
  {
    ddpAutoOn = false;
  }
  if (ddpAutoActive() && millis() - ddpLitAt >= max((uint16_t)db[kk::ddp_wait], (uint16_t)1U) * 1000UL)
  {
    ddpAutoReturn();
  }

  if (millis() - ddpFpsAt >= 1000U)
  {
    ddpFpsAt = millis();
    ddpFps = ddpFrameCount;
    ddpFrameCount = 0U;
  }
}

// эффект «Кадры с компьютера»: выводит принятый кадр, без кадров гасит ленту.
// Масштаб поворачивает картинку вокруг лампы: 1 - без поворота, 100 - почти полный оборот
void ddpRoutine()
{
  static uint8_t shownScale = 0U;
  bool fresh = ddpFresh();
  uint8_t scale = modes[currentMode].Scale;
  if (!loadingFlag && !ddpNewFrame && ddpLit == fresh && shownScale == scale)
  {
    return;
  }
  shownScale = scale;
  loadingFlag = false;
  ddpNewFrame = false;
  ddpLit = fresh;

  if (fresh)
  {
    uint8_t shift = (uint16_t)(constrain(scale, 1U, 100U) - 1U) * WIDTH / 100U;
    const uint8_t* p = ddpFrame;
    for (uint8_t row = 0U; row < HEIGHT; row++)             // первая строка кадра - верх лампы
    {
      for (uint8_t x = 0U; x < WIDTH; x++, p += 3)
      {
        leds[XY((x + shift) % WIDTH, HEIGHT - 1U - row)] = CRGB(p[0], p[1], p[2]);
      }
    }
  }
  else
  {
    ddpHaveFrame = false;
    ledsClear();
  }
  ledsShow();
}

// состояние приёма для страницы настроек
String ddpStateText()
{
  if (!ddpFresh())
  {
    return F("нет, лампа ждёт их по DDP на порту 4048");
  }
  String text(ddpFps);
  text += F(" в секунду от ");
  text += ddpSender.toString();
  return text;
}
