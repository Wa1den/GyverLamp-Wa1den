// Wake-on-LAN: пробуждение компьютера в локальной сети "магическим пакетом".
//
// Magic packet - это UDP-бродкаст из 6 байт 0xFF и MAC-адреса цели, повторённого
// 16 раз (102 байта). Отправляется на широковещательный адрес подсети, порт 9.
// Лампа всегда в сети и подключена к MQTT, поэтому отлично подходит на роль
// WoL-шлюза - отдельная железка не нужна.
//
// Использование:
//  - MAC компьютера хранится в базе настроек (kk::wol_mac) и вводится на странице
//    настроек в группе Wake-on-LAN, там же кнопка "Разбудить";
//  - по MQTT: топик LedLamp/<id>/cmnd/wol (payload - MAC или пусто, тогда
//    берётся сохранённый) либо текстовая команда "WOL [MAC]" на базовом cmnd.

// разбор MAC-адреса из строки вида "AA:BB:CC:DD:EE:FF" (разделители ':' или '-')
bool wolParseMac(const char* str, uint8_t mac[6])
{
  if (str == NULL)
  {
    return false;
  }

  uint8_t byteIndex = 0;
  uint8_t nibbleCount = 0;
  uint8_t current = 0;

  for (const char* p = str; *p != '\0'; p++)
  {
    char c = *p;
    int8_t nibble;
    if (c >= '0' && c <= '9')      nibble = c - '0';
    else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
    else if (c == ':' || c == '-' || c == ' ') continue;
    else return false;

    current = (current << 4) | nibble;
    if (++nibbleCount == 2)
    {
      if (byteIndex >= 6)
      {
        return false;                                       // байт больше шести - это не MAC
      }
      mac[byteIndex++] = current;
      nibbleCount = 0;
      current = 0;
    }
  }

  return (byteIndex == 6 && nibbleCount == 0);
}

// отправка магического пакета на указанный MAC (бродкаст в текущую подсеть, порт 9, три повтора)
bool wolSendMagicPacket(const uint8_t mac[6])
{
  if (!WiFi.isConnected())
  {
    return false;
  }

  // широковещательный адрес подсети (IP лампы с единицами в хостовой части)
  IPAddress broadcast = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  for (uint8_t i = 0; i < 4; i++)
  {
    broadcast[i] |= ~mask[i];
  }

  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (uint8_t i = 0; i < 16; i++)
  {
    memcpy(packet + 6 + i * 6, mac, 6);
  }

  WiFiUDP udp;
  if (!udp.begin(40009))                                    // локальный порт источника (любой свободный)
  {
    return false;
  }

  bool ok = true;
  for (uint8_t attempt = 0; attempt < 3; attempt++)         // несколько повторов - UDP не гарантирует доставку
  {
    ok &= udp.beginPacket(broadcast, 9) &&
          udp.write(packet, sizeof(packet)) == sizeof(packet) &&
          udp.endPacket();
    delay(2);
  }
  udp.stop();

  return ok;
}

// пробуждение по MAC из строки; при пустой строке используется MAC из настроек. Результат - в Журнал
bool wolWake(const char* macStr)
{
  String saved;
  if (macStr == NULL || strlen(macStr) == 0)
  {
    saved = (String)db[kk::wol_mac];
    macStr = saved.c_str();
  }

  uint8_t mac[6];
  if (!wolParseMac(macStr, mac))
  {
    uiLog.printf_P(PSTR("WOL: некорректный MAC \"%s\"\n"), macStr);
    return false;
  }

  bool ok = wolSendMagicPacket(mac);
  if (ok)
  {
    uiLog.printf_P(PSTR("WOL: пакет отправлен на %02X:%02X:%02X:%02X:%02X:%02X\n"),
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  else
  {
    uiLog.println(F("WOL: ошибка отправки (нет WiFi подключения?)"));
  }
  return ok;
}
