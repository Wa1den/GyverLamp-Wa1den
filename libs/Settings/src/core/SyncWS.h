#pragma once

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <WebSocketsServer.h>

#include "./profile.h"                                      // правка для GyverLamp-Wa1den: замеры длительности стадий

namespace sets {

class SyncWS {
   public:
    SyncWS() : _ws(81, "", "sets") {}

    ~SyncWS() {
        _clear();
    }

    void begin() {
        _ws.onEvent([this](uint8_t num, WStype_t type, uint8_t* data, size_t len) {
            switch (type) {
                case WStype_BIN:
                    _clear();
                    _buf = new uint8_t[len];
                    if (!_buf) return;

                    memcpy(_buf, data, len);
                    _len = len;
                    _id = num;
                    break;

                // правка для GyverLamp-Wa1den: подключения и отключения видны в журнале скетча (core/profile.h).
                // Адрес запоминается при подключении: к событию отключения клиент уже сброшен, и remoteIP пуст.
                // Отключение без подключения (соединение с портом без рукопожатия) не сообщается
                case WStype_CONNECTED:
                    if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
                        _ips[num] = (uint32_t)_ws.remoteIP(num);
                        profileEvent("подключён", num, _ips[num]);
                    }
                    break;

                case WStype_DISCONNECTED:
                    if (num < WEBSOCKETS_SERVER_CLIENT_MAX && _ips[num]) {
                        profileEvent("отключён", num, _ips[num]);
                        _ips[num] = 0;
                    }
                    break;

                default: break;
            }
        });

        // правка для GyverLamp-Wa1den: пинг клиентов. Без него вкладка, приостановленная браузером на телефоне
        // или ноутбуке, сутками числится подключённой, каждая рассылка ей ждёт таймаут записи, и всё это время
        // стоит анимация. На пинг отвечает браузер без участия страницы; клиент, дважды подряд не ответивший,
        // отключается
        _ws.enableHeartbeat(PING_INTERVAL_MS, PONG_TIMEOUT_MS, PONG_MISSES);

        _ws.begin();
    }

    void stop() {
        _ws.close();
        _clear();
    }

    void tick() {
        uint32_t prof = profileStart();
        _ws.loop();                                         // рукопожатие новых клиентов и чтение кадров, ждёт TCP с таймаутами из WebSockets.h
        profileEnd("WS приём", prof);

        if (_buf) {
            prof = profileStart();
            size_t len = _len;
            onData(_buf, _len);
            _clear();
            profileEnd("WS запрос", prof, len);
        }
    }

    void send(uint8_t* data, size_t len, bool broadcast) {
        uint32_t prof = profileStart();
        if (broadcast) _ws.broadcastBIN(data, len);
        else _ws.sendBIN(_id, data, len);
        profileEnd("WS отправка", prof, len);
    }

    virtual void onData(uint8_t* data, size_t len) = 0;

   private:
    static constexpr uint32_t PING_INTERVAL_MS = 10000;     // период пинга клиентов
    static constexpr uint32_t PONG_TIMEOUT_MS = 5000;       // сколько ждать ответа на пинг
    static constexpr uint8_t PONG_MISSES = 2;               // после стольких пропущенных ответов клиент отключается

    WebSocketsServer _ws;
    uint32_t _ips[WEBSOCKETS_SERVER_CLIENT_MAX] = {};       // адреса подключённых клиентов, для журнала
    uint8_t _id = 0;
    uint8_t* _buf = nullptr;
    size_t _len;

    void _clear() {
        if (_buf) delete[] _buf;
        _buf = nullptr;
    }
};

}  // namespace sets