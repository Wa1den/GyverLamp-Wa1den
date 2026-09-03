#pragma once
#include <Arduino.h>

// Профилирование обработки запросов веб-интерфейса.
// Правка для GyverLamp-Wa1den, в оригинальной библиотеке этого нет: библиотека сообщает
// скетчу, сколько заняла каждая стадия tick() и каждый запрос страницы. Нужно потому, что
// сервер синхронный - пока он ждёт данные от браузера или пытается их отправить, стоит
// весь loop(), а вместе с ним анимация лампы, кнопка и MQTT. По одному суммарному замеру
// стадии "веб-интерфейс" в скетче не видно, что именно блокирует: DNS, HTTP, вебсокет,
// запись настроек на флеш или обход файловой системы.
//
// Скетч подставляет свою функцию через sets::onProfile(). Пока она не задана, стоимость
// вызовов - одна проверка указателя на стадию.

namespace sets {

// stage - имя стадии, ms - её длительность, arg - уточнение (хэш действия или размер данных)
typedef void (*ProfileCallback)(const char* stage, uint32_t ms, uint32_t arg);

inline ProfileCallback& _profileCb() {
    static ProfileCallback cb = nullptr;
    return cb;
}

// подключить приёмник замеров. nullptr - отключить профилирование
inline void onProfile(ProfileCallback cb) {
    _profileCb() = cb;
}

// засечь начало стадии
inline uint32_t profileStart() {
    return _profileCb() ? millis() : 0;
}

// сообщить о завершении стадии, начатой profileStart()
inline void profileEnd(const char* stage, uint32_t start, uint32_t arg = 0) {
    if (_profileCb()) _profileCb()(stage, millis() - start, arg);
}

}  // namespace sets
