// Управление WiFi подключением (библиотека WiFiConnector).
//
// Поведение:
//  - espMode == 1 (клиент): лампа пытается подключиться к сохранённой WiFi сети (SSID и пароль
//    хранятся в базе настроек Storage.h и вводятся через веб-интерфейс Settings). На время
//    подключения параллельно работает точка доступа лампы, чтобы веб-интерфейс был доступен
//    даже без роутера. Если за ESP_CONN_TIMEOUT секунд подключиться не удалось, лампа остаётся
//    в режиме точки доступа - можно в любой момент зайти на http://192.168.4.1 и настроить сеть.
//  - espMode == 0 (точка доступа): лампа сразу поднимает точку доступа и к роутеру не подключается.
//  - сохранённых сетей может быть до пяти. Лампа сканирует эфир и пробует видимые сохранённые сети
//    по убыванию сигнала, каждую до ESP_CONN_TIMEOUT; если ни одной не видно - все по порядку.

static uint8_t wifiQueue[WIFI_NETWORKS];                    // номера сетей в порядке попыток
static uint8_t wifiQueueLength = 0U;
static uint8_t wifiQueueNext = 0U;

// очередь попыток: видимые сохранённые сети по убыванию сигнала. С одной сетью эфир не сканируется,
// чтобы не задерживать старт на 2 секунды
static void wifiPlan()
{
  wifiQueueLength = 0U;
  wifiQueueNext = 0U;
  uint8_t saved[WIFI_NETWORKS];
  uint8_t savedCount = 0U;
  for (uint8_t i = 0U; i < WIFI_NETWORKS; i++)
  {
    if (((String)db[wifiSsidKeys[i]]).length())
    {
      saved[savedCount++] = i;
    }
  }

  int8_t rssi[WIFI_NETWORKS];
  if (savedCount > 1U)
  {
    WiFi.mode(WIFI_AP_STA);
    int16_t found = WiFi.scanNetworks();
    for (uint8_t k = 0U; k < savedCount; k++)
    {
      String ssid = db[wifiSsidKeys[saved[k]]];
      int8_t best = 0;
      for (int16_t n = 0; n < found; n++)
      {
        if (WiFi.SSID(n) == ssid && (best == 0 || WiFi.RSSI(n) > best))
        {
          best = WiFi.RSSI(n);
        }
      }
      if (best)
      {
        uint8_t pos = wifiQueueLength++;                    // вставка с сортировкой по сигналу
        while (pos && rssi[pos - 1U] < best)
        {
          wifiQueue[pos] = wifiQueue[pos - 1U];
          rssi[pos] = rssi[pos - 1U];
          pos--;
        }
        wifiQueue[pos] = saved[k];
        rssi[pos] = best;
      }
    }
    WiFi.scanDelete();
  }

  if (!wifiQueueLength)
  {
    memcpy(wifiQueue, saved, savedCount);
    wifiQueueLength = savedCount;
  }
}

// подключение к следующей сети из очереди; false - сети кончились
static bool wifiTryNext()
{
  if (wifiQueueNext >= wifiQueueLength)
  {
    return false;
  }
  uint8_t i = wifiQueue[wifiQueueNext++];
  String ssid = db[wifiSsidKeys[i]];
  LOG.printf_P(PSTR("Подключение к WiFi сети: %s\n"), ssid.c_str());
  uiLog.printf_P(PSTR("WiFi: подключение к %s\n"), ssid.c_str());
  WiFiConnector.connect(ssid, db[wifiPassKeys[i]]);
  return true;
}

static void wifiConnectSaved()
{
  wifiPlan();
  if (!wifiTryNext())
  {
    WiFiConnector.connect(emptyString);                     // ни одной сети не сохранено - только точка доступа
  }
}

void wifiSetup()
{
  WiFi.hostname(hostName());                                // имя, которое лампа передаёт роутеру в DHCP-запросе: роутер показывает
                                                            // её в списке клиентов по имени, а его локальный DNS отдаёт по нему адрес
                                                            // (задаётся до connect: запрос с именем уходит при подключении)

  WiFiConnector.setName(apName());                          // имя и пароль точки доступа задаются в веб-интерфейсе (группа "Точка доступа"),
  WiFiConnector.setPass(apPass());                          // значения из Config.h используются как начальные
  WiFiConnector.setTimeout(ESP_CONN_TIMEOUT);

  WiFiConnector.onConnect([]() {
    LOG.print(F("Подключено к WiFi сети. IP адрес: "));
    LOG.println(WiFi.localIP());
    uiLog.printf_P(PSTR("WiFi: подключено, IP %s\n"), WiFi.localIP().toString().c_str());

    if (WiFi.SSID() != (String)db[kk::wifi_last_ssid])     // подключились к новой (не той, что в прошлый раз) сети - покажем IP бегущей строкой,
    {                                                       // чтобы не искать адрес лампы в настройках роутера
      db.set(kk::wifi_last_ssid, WiFi.SSID());
      pendingShowIp = true;                                 // сам показ - в loop (handlePendingActions), не из колбэка
    }
  });

  WiFiConnector.onError([]() {
    if (espMode == 1U && wifiTryNext())                     // не подключились - следующая сохранённая сеть
    {
      return;
    }
    LOG.print(F("Подключение к WiFi сети не выполнено, работает точка доступа. IP адрес: "));
    LOG.println(WiFi.softAPIP());
    uiLog.println(F("WiFi: не подключено, работает точка доступа"));
  });

  if (espMode == 1U)
  {
    LOG.println(F("Старт в режиме WiFi клиента (подключение к роутеру)"));
    wifiConnectSaved();
  }
  else
  {
    LOG.println(F("Старт в режиме WiFi точки доступа"));
    WiFiConnector.connect(emptyString);                     // пустой SSID - только точка доступа
  }
}

void wifiTick()
{
  if (pendingWifiConnect)                                   // подключение по кнопке из веб-интерфейса выполняется здесь, чтобы не трогать WiFi и файловую систему из контекста асинхронного вебсервера
  {
    pendingWifiConnect = false;
    db.update();                                            // запись введённых SSID и пароля на флеш до попытки подключения
    wifiConnectSaved();
  }

  WiFiConnector.tick();
}

// сброс сохранённых SSID и пароля WiFi сети, имени и пароля точки доступа и пароля страницы
void resetWifiSettings()
{
  for (uint8_t i = 0U; i < WIFI_NETWORKS; i++)
  {
    db[wifiSsidKeys[i]] = "";
    db[wifiPassKeys[i]] = "";
  }
  db[kk::wifi_count] = (uint8_t)1;
  db[kk::ap_name] = AP_NAME;                                // имя и пароль точки доступа тоже возвращаются к значениям из Config.h:
  db[kk::ap_pass] = AP_PASS;                                // иначе забытый пароль точки доступа отрезает доступ к веб-интерфейсу
  db[kk::host_name] = HOST_NAME;                            // имя лампы в сети - часть тех же сетевых настроек
  db[kk::ui_pass] = "";                                     // забытый пароль страницы снимается тем же сбросом
  db.update();
  uiApplyPass();
}
