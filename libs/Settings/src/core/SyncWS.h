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
                    _markAlive(num);
                    _clear();
                    _buf = new uint8_t[len];
                    if (!_buf) return;

                    memcpy(_buf, data, len);
                    _len = len;
                    _id = num;
                    break;

                // правка для GyverLamp-Wa1den: в журнал скетча (core/profile.h) попадают только отключения по сбою,
                // обычное закрытие вкладки не сообщается. Сбоем считается отключение сразу после отправки, упёршейся
                // в таймаут (такое соединение рвёт WebSockets::write), или после молчания клиента дольше периода пинга
                // с таймаутом ответа: живая вкладка отвечает на пинг раз в PING_INTERVAL_MS, а клиента, пропустившего
                // ответы, отключают не раньше чем через PING_INTERVAL_MS + 2 * PONG_TIMEOUT_MS молчания
                case WStype_CONNECTED:
                    if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
                        _ips[num] = (uint32_t)_ws.remoteIP(num);    // к событию отключения клиент уже сброшен, и remoteIP пуст
                        _markAlive(num);
                    }
                    break;

                case WStype_PONG:
                    _markAlive(num);
                    break;

                case WStype_DISCONNECTED:
                    if (num < WEBSOCKETS_SERVER_CLIENT_MAX && _ips[num]) {    // без адреса - соединение без рукопожатия
                        uint32_t ip = _ips[num];
                        _ips[num] = 0;                                        // до подсчёта: отключённый клиент в число активных не входит
                        if (_stallMs && millis() - _stallMs < STALL_WINDOW_MS) {
                            profileEvent("оборван: не принимал данные", num, ip, _activeClients());
                        } else if (millis() - _alive[num] >= PING_INTERVAL_MS + PONG_TIMEOUT_MS) {
                            profileEvent("отключён: не отвечал на пинг", num, ip, _activeClients());
                        }
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
        uint32_t start = millis();
        uint32_t prof = profileStart();
        if (broadcast) _ws.broadcastBIN(data, len);
        else _ws.sendBIN(_id, data, len);
        profileEnd("WS отправка", prof, len);
        if (millis() - start >= WEBSOCKETS_WRITE_TIMEOUT) _stallMs = millis();    // см. WStype_DISCONNECTED
    }

    virtual void onData(uint8_t* data, size_t len) = 0;

   private:
    static constexpr uint32_t PING_INTERVAL_MS = 10000;     // период пинга клиентов
    static constexpr uint32_t PONG_TIMEOUT_MS = 5000;       // сколько ждать ответа на пинг
    static constexpr uint8_t PONG_MISSES = 2;               // после стольких пропущенных ответов клиент отключается
    static constexpr uint32_t STALL_WINDOW_MS = 1000;       // отключение в пределах этого времени после долгой отправки - обрыв зависшего клиента

    WebSocketsServer _ws;
    uint32_t _ips[WEBSOCKETS_SERVER_CLIENT_MAX] = {};       // адреса подключённых клиентов, для журнала
    uint32_t _alive[WEBSOCKETS_SERVER_CLIENT_MAX] = {};     // когда клиент последний раз присылал данные или ответ на пинг
    uint32_t _stallMs = 0;                                  // когда закончилась последняя отправка, упёршаяся в таймаут записи

    void _markAlive(uint8_t num) {
        if (num < WEBSOCKETS_SERVER_CLIENT_MAX) _alive[num] = millis();
    }

    // клиенты, прошедшие рукопожатие и ещё не отключённые. Считается по своему списку адресов: connectedClients()
    // библиотеки при проверке сам отключает потерянных клиентов и вызвал бы обработчик отключения изнутри текущего
    uint8_t _activeClients() {
        uint8_t n = 0;
        for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
            if (_ips[i]) n++;
        }
        return n;
    }
    uint8_t _id = 0;
    uint8_t* _buf = nullptr;
    size_t _len;

    void _clear() {
        if (_buf) delete[] _buf;
        _buf = nullptr;
    }
};

}  // namespace sets