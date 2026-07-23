// Управление WiFi подключением (библиотека WiFiConnector).
//
// Поведение:
//  - espMode == 1 (клиент): лампа пытается подключиться к сохранённой WiFi сети (SSID и пароль
//    хранятся в базе настроек Storage.h и вводятся через веб-интерфейс Settings). На время
//    подключения параллельно работает точка доступа AP_NAME, чтобы веб-интерфейс был доступен
//    даже без роутера. Если за ESP_CONN_TIMEOUT секунд подключиться не удалось, лампа остаётся
//    в режиме точки доступа - можно в любой момент зайти на http://192.168.4.1 и настроить сеть.
//  - espMode == 0 (точка доступа): лампа сразу поднимает точку доступа и к роутеру не подключается.

void wifiSetup()
{
  WiFiConnector.setName(AP_NAME);
  WiFiConnector.setPass(AP_PASS);
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
    LOG.print(F("Подключение к WiFi сети не выполнено, работает точка доступа. IP адрес: "));
    LOG.println(WiFi.softAPIP());
    uiLog.println(F("WiFi: не подключено, работает точка доступа"));
  });

  if (espMode == 1U)
  {
    LOG.printf_P(PSTR("Старт в режиме WiFi клиента (подключение к роутеру), сеть: %s\n"), ((String)db[kk::wifi_ssid]).c_str());
    WiFiConnector.connect(db[kk::wifi_ssid], db[kk::wifi_pass]);
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
    LOG.printf_P(PSTR("Запрошено подключение к WiFi сети: %s\n"), ((String)db[kk::wifi_ssid]).c_str());
    WiFiConnector.connect(db[kk::wifi_ssid], db[kk::wifi_pass]);
  }

  WiFiConnector.tick();
}

// сброс сохранённых SSID и пароля WiFi сети
void resetWifiSettings()
{
  db[kk::wifi_ssid] = "";
  db[kk::wifi_pass] = "";
  db.update();
}
