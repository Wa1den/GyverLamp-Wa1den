#pragma once
#include "ServerBase.h"

// таймаут ожидания данных от подключившегося клиента. ВАЖНО: сервер синхронный, и всё это
// время блокируется loop() - браузеры открывают соединения "про запас" и не шлют по ним запрос,
// каждое такое соединение стоит полной остановки скетча. Значение можно переопределить в скетче
// (#define GS_CLIENT_TOUT до подключения Settings)
#ifndef GS_CLIENT_TOUT
#define GS_CLIENT_TOUT 1500
#endif

namespace ghttp {

template <typename server_t, typename client_t>
class Server : public ServerBase {
   public:
    Server(uint16_t port) : server(port) {}

    // запустить
    void begin() {
        server.begin();
    }

    // вызывать в loop
    void tick(HeadersCollector* collector = nullptr) {
        client_t client = server.accept();
        if (client) {
            client.Stream::setTimeout(GS_CLIENT_TOUT);
            handleRequest(client, collector);
        }
    }

    server_t server;

   private:
};

}