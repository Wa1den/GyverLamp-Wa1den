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

                default: break;
            }
        });

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
    WebSocketsServer _ws;
    uint8_t _id = 0;
    uint8_t* _buf = nullptr;
    size_t _len;

    void _clear() {
        if (_buf) delete[] _buf;
        _buf = nullptr;
    }
};

}  // namespace sets