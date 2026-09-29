#pragma once
#include <Arduino.h>
#include <FS.h>

#include "GyverDB.h"

class GyverDBFile : public GyverDB {
   public:
    GyverDBFile(fs::FS* nfs = nullptr, const char* path = nullptr, uint32_t tout = 10000) {
        setFS(nfs, path);
        _tout = tout;
    }

    ~GyverDBFile() {
        update();
    }

    // установить файловую систему и имя файла
    void setFS(fs::FS* nfs, const char* path) {
        _fs = nfs;
        _path = path;
    }

    // установить таймаут записи, мс (умолч. 10000)
    void setTimeout(uint32_t tout = 10000) {
        _tout = tout;
    }

    // прочитать данные
    // Патч Wa1den: файл пишется через временный (см. update), поэтому основной файл всегда целый.
    // Временный остаётся, только если запись оборвалась: до переименования он неполный и удаляется,
    // а если основного файла нет - запись дошла до переименования, и временный становится основным.
    bool begin() {
        bool res = false;
        if (_fs) {
            String tmp = String(_path) + ".tmp";
            if (_fs->exists(tmp)) {
                if (_fs->exists(_path)) _fs->remove(tmp);
                else _fs->rename(tmp, _path);
            }
            if (_fs->exists(_path)) {
                File file = _fs->open(_path, "r");
                if (file) res = readFrom(file, file.size());
                _update = false;
            } else {
                File file = _fs->open(_path, "w");
                res = true;
            }
        }
        return res;
    }

    // обновить данные в файле, если было изменение БД. Вернёт true при успешной записи
    bool update() {
        _tmr = 0;
        if (!_update) return false;
        _update = false;
        // Патч Wa1den: запись во временный файл и атомарная подмена основного. Прямая запись
        // обрезает файл до нуля, и перезагрузка посреди неё стирала все настройки
        String tmp = String(_path) + ".tmp";
        File file = _fs->open(tmp, "w");
        if (!file) return 0;
        bool res = writeTo(file);
        file.close();
        if (!res) {
            _fs->remove(tmp);
            return 0;
        }
        return _fs->rename(tmp, _path);
    }

    // тикер, вызывать в loop. Сам обновит данные при изменении и выходе таймаута, вернёт true
    bool tick() {
        if (_update && !_tmr) {
            _tmr = millis();
        }
        if (_tmr && millis() - _tmr >= _tout) {
            update();
            return 1;
        }
        return 0;
    }

   private:
    fs::FS* _fs;
    const char* _path;
    uint32_t _tmr = 0, _tout = 10000;
};