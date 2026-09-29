// реализация на GyverHTTP

#pragma once
#include <Arduino.h>
#include <GyverHTTP.h>
#include <LittleFS.h>

#include "./core/DnsWrapper.h"
#include "./core/SettingsBase.h"
#include "./core/ota.h"
#include "./web/settings.h"

#ifndef SETS_UPLOAD_TOUT
#define SETS_UPLOAD_TOUT 5000  // GyverLamp-Wa1den: ожидание данных при загрузке файла и прошивки, мс
#endif

template <typename server_t, typename client_t>
class SettingsT : public sets::SettingsBase {
   public:
#ifndef SETT_NO_DB
    SettingsT(const String& title = "", GyverDB* db = nullptr) : sets::SettingsBase(title, db), server(80) {}
#else
    SettingsT(const String& title = "") : sets::SettingsBase(title), server(80) {}
#endif

    // запустить. captive - запустить mdns для автооткрытия окна в режиме AP при подключении к точке
    // domain - домен, по которому есп будет доступна в локальной сети по адресу домен.local
    void begin(bool captive = true, const char* domain = nullptr) {
        _dns.begin(captive, domain);
        server.begin();

#ifdef SETS_NO_CORS
        server.useCors(false);
#endif

        server.onRequest([this](ghttp::ServerBase::Request req) {
            switch (req.path().hash()) {
                case SH("/settings"):
                    parse(req.param("auth").toInt32HEX(),
                          req.param("action").hash(),
                          req.param("id").toInt32HEX(),
                          req.param("value").decodeUrl());
                    break;

                case SH("/fetch"):
                    if (authenticate(req.param("auth").toInt32HEX())) {
                        String path = req.param("path").decodeUrl();
                        File f = fs.openRead(path.c_str());
                        if (f) server.sendFile(f);
                        else server.send(500);
                        if (fetch_cb) fetch_cb(path);
                    } else {
                        server.send(401);
                    }
                    break;

                case SH("/upload"):
                    if (authenticate(req.param("auth").toInt32HEX())) {
                        String path = req.param("path").decodeUrl();
                        File f = fs.openWrite(path.c_str());
                        if (f) {
                            // GyverLamp-Wa1den: ожидание данных - не GS_CLIENT_TOUT, а SETS_UPLOAD_TOUT, и файл принимается только целиком
                            size_t expected = req.body().length();
                            req.body().setTimeout(SETS_UPLOAD_TOUT);
                            if (req.body().writeTo(f) == expected) {
                                server.send(200);
                                if (upload_cb) upload_cb(path);
                            } else server.send(500);
                        } else server.send(500);
                    } else {
                        server.send(401);
                    }
                    break;

                case SH("/ota"):
                    if (authenticate(req.param("auth").toInt32HEX())) {
                        // GyverLamp-Wa1den: при паузе в приёме дольше тайм-аута потока writeTo возвращал часть файла, а
                        // Update.end(true) принимал обрезанный образ - лампа перезагружалась в нерабочую прошивку.
                        // Ожидание данных - SETS_UPLOAD_TOUT, и образ принимается, только если записан целиком
                        size_t expected = req.body().length();
                        req.body().setTimeout(SETS_UPLOAD_TOUT);
                        bool ok = false;
                        if (expected && sets::beginOta()) {
                            if (req.body().writeTo(Update) == expected) ok = Update.end(true) && !Update.hasError();
                            else Update.end(false);  // запись не завершена - образ не будет установлен
                        }
                        if (ok) {
                            server.send(200);
                            restart();
                        } else server.send(500);
                    } else {
                        server.send(401);
                    }
                    break;

                case SH("/script.js"):
                    server.sendFile_P(settings_script_gz, sizeof(settings_script_gz), "text/javascript", true, true);
                    break;

                case SH("/style.css"):
                    server.sendFile_P(settings_style_gz, sizeof(settings_style_gz), "text/css", true, true);
                    break;

                case SH("/favicon.svg"):
                    server.sendFile_P(settings_favicon_gz, sizeof(settings_favicon_gz), "image/svg+xml", true, true);
                    break;

                case SH("/custom.js"):
                    if (!custom.p) server.send(500);
                    else {
                        if (!custom.isFile) server.sendFile_P((const uint8_t*)custom.p, custom.len, "text/javascript", false, custom.gz);
                        else {
                            File f = fs.openRead(custom.p);
                            if (f) server.sendFile(f, "text/javascript", false, custom.gz);
                            else server.send(500);
                        }
                    }
                    break;

                default:
                    server.sendFile_P(settings_index_gz, sizeof(settings_index_gz), "text/html", false, true);
                    break;
            }
        });
    }

    void stop() {
        server.server.stop();
        _dns.stop();
    }

    // правка для GyverLamp-Wa1den: стадии разнесены по замерам, см. core/profile.h
    void tick() {
        uint32_t prof = sets::profileStart();
        _dns.tick();
        sets::profileEnd("DNS", prof);

        prof = sets::profileStart();
        server.tick();                                      // приём и обработка HTTP-запроса, блокирует loop() на время сеанса
        sets::profileEnd("HTTP", prof);

        prof = sets::profileStart();
        sets::SettingsBase::tick();
        sets::profileEnd("фон", prof);
    }

    ghttp::Server<server_t, client_t> server;

   private:
    sets::DnsWrapper _dns;
    bool _rst = false;

    void answer(uint8_t* data, size_t len) override {
        uint32_t prof = sets::profileStart();
        server.send(Text(data, len));
        sets::profileEnd("ответ HTTP", prof, len);
    }
};